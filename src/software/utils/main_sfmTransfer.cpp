// This file is part of the AliceVision project.
// Copyright (c) 2016 AliceVision contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include <aliceVision/sfmData/SfMData.hpp>
#include <aliceVision/sfmDataIO/sfmDataIO.hpp>
#include <aliceVision/sfm/utils/alignment.hpp>
#include <aliceVision/system/Logger.hpp>
#include <aliceVision/system/cmdline.hpp>
#include <aliceVision/config.hpp>

#include <boost/program_options.hpp>

#include <string>
#include <sstream>

// These constants define the current software version.
// They must be updated when the command line is changed.
#define ALICEVISION_SOFTWARE_VERSION_MAJOR 1
#define ALICEVISION_SOFTWARE_VERSION_MINOR 0

using namespace aliceVision;
using namespace aliceVision::sfm;

namespace po = boost::program_options;


/**
* @brief Matching Views method enum
*/
enum class EMatchingMethod : unsigned char
{
    FROM_VIEWID = 0
    , FROM_FILEPATH
    , FROM_METADATA
};

/**
* @brief Convert an EMatchingMethod enum to its corresponding string
* @param[in] matchingMethod The given EMatchingMethod enum
* @return string
*/
std::string EMatchingMethod_enumToString(EMatchingMethod alignmentMethod)
{
    switch (alignmentMethod)
    {
    case EMatchingMethod::FROM_VIEWID:   return "from_viewid";
    case EMatchingMethod::FROM_FILEPATH: return "from_filepath";
    case EMatchingMethod::FROM_METADATA: return "from_metadata";
    }
    throw std::out_of_range("Invalid EMatchingMethod enum");
}

/**
* @brief Convert a string to its corresponding EMatchingMethod enum
* @param[in] matchingMethod The given string
* @return EMatchingMethod enum
*/
EMatchingMethod EMatchingMethod_stringToEnum(const std::string& alignmentMethod)
{
    std::string method = alignmentMethod;
    std::transform(method.begin(), method.end(), method.begin(), ::tolower); //tolower

    if (method == "from_viewid")   return EMatchingMethod::FROM_VIEWID;
    if (method == "from_filepath") return EMatchingMethod::FROM_FILEPATH;
    if (method == "from_metadata") return EMatchingMethod::FROM_METADATA;
    throw std::out_of_range("Invalid SfM alignment method : " + alignmentMethod);
}

inline std::istream& operator>>(std::istream& in, EMatchingMethod& alignment)
{
    std::string token;
    in >> token;
    alignment = EMatchingMethod_stringToEnum(token);
    return in;
}

inline std::ostream& operator<<(std::ostream& os, EMatchingMethod e)
{
    return os << EMatchingMethod_enumToString(e);
}


int main(int argc, char **argv)
{
    // command-line parameters

    std::string verboseLevel = system::EVerboseLevel_enumToString(system::Logger::getDefaultVerboseLevel());
    std::string sfmDataFilename;
    std::string outSfMDataFilename;
    std::string sfmDataReferenceFilename;
    bool transferPoses = true;
    bool transferIntrinsics = true;
    EMatchingMethod matchingMethod = EMatchingMethod::FROM_VIEWID;
    std::string fileMatchingPattern;
    std::vector<std::string> metadataMatchingList = { "Make", "Model", "Exif:BodySerialNumber" , "Exif:LensSerialNumber" };

    po::options_description allParams("AliceVision sfmAlignment");

    po::options_description requiredParams("Required parameters");
    requiredParams.add_options()
        ("input,i", po::value<std::string>(&sfmDataFilename)->required(),
             "SfMData file to align.")
        ("output,o", po::value<std::string>(&outSfMDataFilename)->required(),
            "Output SfMData scene.")
        ("reference,r", po::value<std::string>(&sfmDataReferenceFilename)->required(),
            "Path to the scene used as the reference coordinate system.");

    po::options_description optionalParams("Optional parameters");
    optionalParams.add_options()
        ("method", po::value<EMatchingMethod>(&matchingMethod)->default_value(matchingMethod),
            "Matching Method:\n"
            "\t- from_viewid: Align cameras with same view Id\n"
            "\t- from_filepath: Align cameras with a filepath matching, using --fileMatchingPattern\n"
            "\t- from_metadata: Align cameras with matching metadata, using --metadataMatchingList\n")
        ("fileMatchingPattern", po::value<std::string>(&fileMatchingPattern)->default_value(fileMatchingPattern),
            "Matching pattern for the from_filepath method.\n")
        ("metadataMatchingList", po::value<std::vector<std::string>>(&metadataMatchingList)->multitoken()->default_value(metadataMatchingList),
            "List of metadata that should match to create the correspondences.\n")
        ("transferPoses", po::value<bool>(&transferPoses)->default_value(transferPoses),
            "Transfer poses.")
        ("transferIntrinsics", po::value<bool>(&transferIntrinsics)->default_value(transferIntrinsics),
            "Transfer intrinsics.")
        ;

    po::options_description logParams("Log parameters");
    logParams.add_options()
        ("verboseLevel,v", po::value<std::string>(&verboseLevel)->default_value(verboseLevel),
            "verbosity level (fatal,  error, warning, info, debug, trace).");

    allParams.add(requiredParams).add(optionalParams).add(logParams);

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, allParams), vm);

        if (vm.count("help") || (argc == 1))
        {
            ALICEVISION_COUT(allParams);
            return EXIT_SUCCESS;
        }
        po::notify(vm);
    }
    catch (boost::program_options::required_option& e)
    {
        ALICEVISION_CERR("ERROR: " << e.what());
        ALICEVISION_COUT("Usage:\n\n" << allParams);
        return EXIT_FAILURE;
    }
    catch (boost::program_options::error& e)
    {
        ALICEVISION_CERR("ERROR: " << e.what());
        ALICEVISION_COUT("Usage:\n\n" << allParams);
        return EXIT_FAILURE;
    }

    ALICEVISION_COUT("Program called with the following parameters:");
    ALICEVISION_COUT(vm);

    // set verbose level
    system::Logger::get()->setLogLevel(verboseLevel);

    // Load input scene
    sfmData::SfMData sfmDataIn;
    if (!sfmDataIO::Load(sfmDataIn, sfmDataFilename, sfmDataIO::ESfMData::ALL))
    {
        ALICEVISION_LOG_ERROR("The input SfMData file '" << sfmDataFilename << "' cannot be read");
        return EXIT_FAILURE;
    }

    // Load reference scene
    sfmData::SfMData sfmDataInRef;
    if (!sfmDataIO::Load(sfmDataInRef, sfmDataReferenceFilename, sfmDataIO::ESfMData::ALL))
    {
        ALICEVISION_LOG_ERROR("The reference SfMData file '" << sfmDataReferenceFilename << "' cannot be read");
        return EXIT_FAILURE;
    }

    ALICEVISION_LOG_INFO("Search similarity transformation.");

    std::vector<std::pair<IndexT, IndexT>> commonViewIds;
    switch (matchingMethod)
    {
        case EMatchingMethod::FROM_VIEWID:
        {
            std::vector<IndexT> commonViewIdsTmp;
            getCommonViews(sfmDataIn, sfmDataInRef, commonViewIdsTmp);
            for (IndexT id : commonViewIdsTmp)
            {
                commonViewIds.push_back(std::make_pair(id, id));
            }
            break;
        }
        case EMatchingMethod::FROM_FILEPATH:
        {
            sfm::matchViewsByFilePattern(sfmDataIn, sfmDataInRef, fileMatchingPattern, commonViewIds);
            break;
        }
        case EMatchingMethod::FROM_METADATA:
        {
            sfm::matchViewsByMetadataMatching(sfmDataIn, sfmDataInRef, metadataMatchingList, commonViewIds);
            break;
        }
    }
    ALICEVISION_LOG_DEBUG("Found " << commonViewIds.size() << " common views.");

    if (commonViewIds.empty())
    {
        ALICEVISION_LOG_ERROR("Failed to find matching Views between the 2 SfmData.");
        return EXIT_FAILURE;
    }

    if (!transferPoses && !transferIntrinsics)
    {
        ALICEVISION_LOG_ERROR("Nothing to do.");
    }
    else
    {
        for (const auto& matchingViews: commonViewIds)
        {
            if(!sfmDataIn.isPoseAndIntrinsicDefined(matchingViews.first) &&
                sfmDataInRef.isPoseAndIntrinsicDefined(matchingViews.second))
            {
                // Missing pose in sfmDataIn and valid pose in sfmDataInRef,
                // so we can transfer the pose.

                auto& viewA = sfmDataIn.getView(matchingViews.first);
                const auto& viewB = sfmDataInRef.getView(matchingViews.second);
                if (viewA.isPartOfRig() || viewB.isPartOfRig())
                {
                    ALICEVISION_LOG_DEBUG("Rig poses are not yet supported in SfMTransfer.");
                    continue;
                }

                if (transferPoses)
                {
                    sfmDataIn.getPoses()[viewA.getPoseId()] = sfmDataInRef.getPoses().at(viewB.getPoseId());
                }
                if (transferIntrinsics)
                {
                    sfmDataIn.getIntrinsicPtr(viewA.getIntrinsicId())->assign(*sfmDataInRef.getIntrinsicPtr(viewB.getIntrinsicId()));
                }
            }
        }
    }

    ALICEVISION_LOG_INFO("Save into '" << outSfMDataFilename << "'");
    // Export the SfMData scene in the expected format
    if (!sfmDataIO::Save(sfmDataIn, outSfMDataFilename, sfmDataIO::ESfMData::ALL))
    {
        ALICEVISION_LOG_ERROR("An error occurred while trying to save '" << outSfMDataFilename << "'");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
