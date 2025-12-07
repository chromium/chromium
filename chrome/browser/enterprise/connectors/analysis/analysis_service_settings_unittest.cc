// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/analysis_test_utils.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <initializer_list>

#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/source_destination_test_util.h"
#endif

namespace enterprise_connectors {

namespace {

using test::GetExpectedLearnMoreUrlSpecs;
using test::kEnablePatternIsNotADictSettings;
using test::kNoEnabledPatternsSettings;
using test::kNoProviderSettings;
using test::kNormalSettings;
using test::kNormalSettingsDlpRequiresBypassJustification;
using test::kNormalSettingsWithCustomMessage;
using test::kScan1DotCom;
using test::kUrlAndSourceDestinationListSettings;
using test::NormalDlpAndMalwareSettings;
using test::NormalDlpSettings;
using test::NormalMalwareSettings;
using test::NormalSettingsDlpRequiresBypassJustification;
using test::NormalSettingsWithCustomMessage;
using test::NoSettings;
using test::OnlyDlpEnabledSettings;
using test::TestParam;

#if BUILDFLAG(IS_CHROMEOS)
using VolumeInfo = SourceDestinationTestingHelper::VolumeInfo;

struct SourceDestinationTestParam {
  SourceDestinationTestParam(
      std::pair<VolumeInfo, VolumeInfo> source_destination_pair,
      const char* settings_value,
      AnalysisSettings* expected_settings,
      DataRegion data_region = DataRegion::NO_PREFERENCE)
      : source_destination_pair(source_destination_pair),
        settings_value(settings_value),
        expected_settings(expected_settings),
        data_region(data_region) {}

  std::pair<VolumeInfo, VolumeInfo> source_destination_pair;
  const char* settings_value;
  raw_ptr<AnalysisSettings> expected_settings;
  DataRegion data_region;
};

constexpr char kNormalSourceDestinationSettings[] = R"({
  "service_provider": "%s",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "ANY"
          }],
          "destinations": [{
            "file_system_type": "ANY"
          }]
        }
      ],
      "tags": ["dlp", "malware"]
    },
  ],
  "disable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "REMOVABLE"
          }],
          "destinations": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ]
        }
      ],
      "tags": ["dlp"]
    },
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ],
          "destinations": [{
            "file_system_type": "REMOVABLE"
          }]
        }
      ],
      "tags": ["malware"]},
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ],
          "destinations": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ]
        }
      ],
      "tags": ["dlp", "malware"]
    },
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
  "minimum_data_size": 123,
})";

constexpr char kOnlyEnabledSourceDestinationSettings[] = R"({
  "service_provider": "%s",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "REMOVABLE"},
            {"file_system_type": "PROVIDED"}
          ],
          "destinations": [
            {"file_system_type": "REMOVABLE"},
            {"file_system_type": "PROVIDED"}
          ]
        }
      ],
      "tags": ["dlp", "malware"]
    },
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "REMOVABLE"}
          ],
          "destinations": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ]
        }
      ],
      "tags": ["malware"]
    },
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ],
          "destinations": [
            {"file_system_type": "REMOVABLE"}
          ]
        }
      ],
      "tags": ["dlp"]
    },
  ],
})";

constexpr char kEnableEntryIsNotADictSourceDestinationSettings[] = R"({
  "service_provider": "%s",
  "enable": [
    "source_destination_list",
  ],
})";

constexpr char kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings[] =
    R"({
  "service_provider": "%s",
  "enable": [
    {"tags": ["dlp"]},
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "REMOVABLE"},
            {"file_system_type": "PROVIDED"}
          ],
          "destinations": [
            {"file_system_type": "REMOVABLE"},
            {"file_system_type": "PROVIDED"}
          ]
        }
      ],
      "tags": ["dlp", "malware"]
    },
    {
      "tags": ["dlp"]
    },
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "REMOVABLE"}
          ],
          "destinations": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ]
        }
      ],
      "tags": ["malware"]
    },
    {
      "source_destination_list": [
        {
          "sources": []
        }
      ],
      "tags": ["dlp"]
    },
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ],
          "destinations": [
            {"file_system_type": "REMOVABLE"}
          ]
        }
      ],
      "tags": ["dlp"]
    },
    {
      "source_destination_list": [
        {
          "sources": [],
          "destinations": []
        }
      ],
      "tags": ["malware"]
    },
  ],
})";

constexpr char kNoProviderSourceDestinationSettings[] = R"({
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "ANY"
          }],
          "destinations": [{
            "file_system_type": "ANY"
          }]
        }
      ],
      "tags": ["dlp", "malware"]
    },
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
  "minimum_data_size": 123,
})";

constexpr char kNothingEnabledSourceDestinationSettings[] = R"({
  "service_provider": "%s",
  "disable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "REMOVABLE"
          }],
          "destinations": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ]
        }
      ],
      "tags": ["dlp"]
    },
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ],
          "destinations": [{
            "file_system_type": "REMOVABLE"
          }]
        }
      ],
      "tags": ["malware"]},
    {
      "source_destination_list": [
        {
          "sources": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ],
          "destinations": [
            {"file_system_type": "MY_FILES"},
            {"file_system_type": "GOOGLE_DRIVE"}
          ]
        }
      ],
      "tags": ["dlp", "malware"]
    },
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
})";

constexpr char kNormalSourceDestinationSettingsWithCustomMessage[] = R"({
  "service_provider": "%s",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "ANY"
          }],
          "destinations": [{
            "file_system_type": "ANY"
          }]
        }
      ],
      "tags": ["dlp", "malware"]
    },
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
  "minimum_data_size": 123,
  "custom_messages": [
    {
      "message": "dlpabcèéç",
      "learn_more_url": "http://www.example.com/dlp",
      "tag": "dlp"
    },
    {
      "message": "malwareabcèéç",
      "learn_more_url": "http://www.example.com/malware",
      "tag": "malware"
    },
  ],
})";

constexpr char
    kNormalSourceDestinationSettingsDlpRequiresBypassJustification[] = R"({
  "service_provider": "%s",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "ANY"
          }],
          "destinations": [{
            "file_system_type": "ANY"
          }]
        }
      ],
      "tags": ["dlp", "malware"]
    },
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
  "minimum_data_size": 123,
  "require_justification_tags": ["dlp"],
})";

constexpr VolumeInfo kRemovableVolumeInfo{
    file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION, std::nullopt,
    "REMOVABLE"};
constexpr VolumeInfo kProvidedVolumeInfo{file_manager::VOLUME_TYPE_PROVIDED,
                                         std::nullopt, "PROVIDED"};
constexpr VolumeInfo kMyFilesVolumeInfo{
    file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY, std::nullopt, "MY_FILES"};
constexpr VolumeInfo kDriveVolumeInfo{file_manager::VOLUME_TYPE_GOOGLE_DRIVE,
                                      std::nullopt, "GOOGLE_DRIVE"};

constexpr std::initializer_list<VolumeInfo> kVolumeInfos{
    kRemovableVolumeInfo, kProvidedVolumeInfo, kMyFilesVolumeInfo,
    kDriveVolumeInfo};

constexpr std::pair<VolumeInfo, VolumeInfo> kDlpMalwareVolumePair1 = {
    kRemovableVolumeInfo, kProvidedVolumeInfo};
constexpr std::pair<VolumeInfo, VolumeInfo> kDlpMalwareVolumePair2 = {
    kProvidedVolumeInfo, kRemovableVolumeInfo};
constexpr std::pair<VolumeInfo, VolumeInfo> kNoDlpNoMalwareVolumePair1 = {
    kMyFilesVolumeInfo, kDriveVolumeInfo};
constexpr std::pair<VolumeInfo, VolumeInfo> kNoDlpNoMalwareVolumePair2 = {
    kDriveVolumeInfo, kMyFilesVolumeInfo};
constexpr std::pair<VolumeInfo, VolumeInfo> kNoDlpMalwareVolumePair1 = {
    kRemovableVolumeInfo, kMyFilesVolumeInfo};
constexpr std::pair<VolumeInfo, VolumeInfo> kNoDlpMalwareVolumePair2 = {
    kRemovableVolumeInfo, kDriveVolumeInfo};
constexpr std::pair<VolumeInfo, VolumeInfo> kDlpNoMalwareVolumePair1 = {
    kMyFilesVolumeInfo, kRemovableVolumeInfo};
constexpr std::pair<VolumeInfo, VolumeInfo> kDlpNoMalwareVolumePair2 = {
    kDriveVolumeInfo, kRemovableVolumeInfo};

// These are only used for SourceDestination tests and are unused on non-ash
// chrome.
AnalysisSettings* OnlyMalwareEnabledSettings() {
  static base::NoDestructor<AnalysisSettings> settings([]() {
    AnalysisSettings settings;
    settings.tags = {{"malware", TagSettings()}};
    return settings;
  }());
  return settings.get();
}

AnalysisSettings* OnlyDlpAndMalwareEnabledSettings() {
  static base::NoDestructor<AnalysisSettings> settings([]() {
    AnalysisSettings settings;
    settings.tags = {{"dlp", TagSettings()}, {"malware", TagSettings()}};
    return settings;
  }());
  return settings.get();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class AnalysisServiceSettingsLocalTest
    : public testing::TestWithParam<TestParam> {
 public:
  GURL url() const { return GURL(GetParam().url); }
  std::string GetSettingsValue() const {
    static const char* verification = R"(
      "verification": {
        "linux": ["key"],
        "mac": ["key"],
        "windows": ["key"]
      },
    )";

    std::string value = GetParam().settings_value;
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "%s", "local_user_agent");
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "%s", verification);
    return value;
  }
  AnalysisSettings* expected_settings() const {
    // Set the GURL field dynamically to avoid static initialization issues.
    if (GetParam().expected_settings != NoSettings()) {
      LocalAnalysisSettings local_settings;
      local_settings.local_path = "path_user";
      local_settings.user_specific = true;
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
      local_settings.verification_signatures.push_back("key");
#endif
      GetParam().expected_settings->cloud_or_local_settings =
          CloudOrLocalAnalysisSettings(std::move(local_settings));

      // The "local_user_agent" analysis provider only supports the "dlp" tag,
      // so it is expected that the malware tag is absent from final settings
      // even when it is included in the policy.
      GetParam().expected_settings->tags.erase("malware");
      if (GetParam().expected_settings->tags.empty()) {
        return NoSettings();
      }
    }

    return GetParam().expected_settings;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(AnalysisServiceSettingsLocalTest, LocalTest) {
  std::string json_string = GetSettingsValue();
  auto settings =
      base::JSONReader::Read(json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  AnalysisServiceSettings service_settings(settings.value(),
                                           *GetServiceProviderConfig());

  auto analysis_settings =
      service_settings.GetAnalysisSettings(url(), DataRegion::NO_PREFERENCE);
  ASSERT_EQ((expected_settings() != nullptr), analysis_settings.has_value());
  if (analysis_settings.has_value()) {
    ASSERT_EQ(analysis_settings.value().block_until_verdict,
              expected_settings()->block_until_verdict);
    ASSERT_EQ(analysis_settings.value().default_action,
              expected_settings()->default_action);
    ASSERT_EQ(analysis_settings.value().block_password_protected_files,
              expected_settings()->block_password_protected_files);
    ASSERT_EQ(analysis_settings.value().block_large_files,
              expected_settings()->block_large_files);
    ASSERT_EQ(analysis_settings.value().minimum_data_size,
              expected_settings()->minimum_data_size);

    const auto& cloud_or_local_settings =
        analysis_settings.value().cloud_or_local_settings;
    ASSERT_TRUE(cloud_or_local_settings.is_local_analysis());
    ASSERT_EQ(cloud_or_local_settings.local_path(),
              expected_settings()->cloud_or_local_settings.local_path());
    ASSERT_EQ(cloud_or_local_settings.user_specific(),
              expected_settings()->cloud_or_local_settings.user_specific());
    ASSERT_EQ(
        cloud_or_local_settings.verification_signatures(),
        expected_settings()->cloud_or_local_settings.verification_signatures());

    for (const auto& entry : expected_settings()->tags) {
      const std::string& tag = entry.first;
      ASSERT_TRUE(analysis_settings.value().tags.count(entry.first));
      ASSERT_EQ(analysis_settings.value().tags[tag].custom_message.message,
                entry.second.custom_message.message);
      if (!analysis_settings.value()
               .tags[tag]
               .custom_message.learn_more_url.is_empty()) {
        ASSERT_EQ(GetExpectedLearnMoreUrlSpecs().at(tag),
                  analysis_settings.value()
                      .tags[tag]
                      .custom_message.learn_more_url.spec());
        ASSERT_EQ(GetExpectedLearnMoreUrlSpecs().at(tag),
                  service_settings.GetLearnMoreUrl(tag).value().spec());
      }
      ASSERT_EQ(analysis_settings.value().tags[tag].requires_justification,
                entry.second.requires_justification);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AnalysisServiceSettingsLocalTest,
    testing::Values(
        // Validate that no settings are returned for various invalid or empty
        // configurations.
        TestParam(kScan1DotCom, kEnablePatternIsNotADictSettings, NoSettings()),
        TestParam(kScan1DotCom,
                  kUrlAndSourceDestinationListSettings,
                  NoSettings()),
        TestParam(kScan1DotCom, kNoProviderSettings, NoSettings()),
        TestParam(kScan1DotCom, kNoEnabledPatternsSettings, NoSettings()),

        // Validate local analysis settings, custom messages and bypass
        // justifications.
        TestParam(kScan1DotCom, kNormalSettings, NormalDlpAndMalwareSettings()),
        TestParam(kScan1DotCom,
                  kNormalSettingsWithCustomMessage,
                  NormalSettingsWithCustomMessage()),
        TestParam(kScan1DotCom,
                  kNormalSettingsDlpRequiresBypassJustification,
                  NormalSettingsDlpRequiresBypassJustification())));

#if BUILDFLAG(IS_CHROMEOS)

class AnalysisServiceSourceDestinationSettingsTest
    : public testing::TestWithParam<SourceDestinationTestParam> {
 public:
  AnalysisServiceSourceDestinationSettingsTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    source_destination_testing_helper_ =
        std::make_unique<SourceDestinationTestingHelper>(profile_,
                                                         kVolumeInfos);
  }

  ~AnalysisServiceSourceDestinationSettingsTest() override {
    profile_manager_.DeleteAllTestingProfiles();
  }

  storage::FileSystemURL source_url() const {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        GetParam().source_destination_pair.first);
  }
  storage::FileSystemURL destination_url() const {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        GetParam().source_destination_pair.second);
  }
  content::BrowserContext* fs_context() const { return profile_; }
  std::string GetSettingsValue() const {
    std::string value = GetParam().settings_value;
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "%s", "google");
    return value;
  }
  AnalysisSettings* expected_settings() const {
    // Set the GURL field dynamically to avoid static initialization issues.
    if (GetParam().expected_settings != NoSettings()) {
      GURL regionalized_url =
          GURL(GetServiceProviderConfig()
                   ->at("google")
                   .analysis->region_urls[static_cast<int>(data_region())]);
      CloudAnalysisSettings cloud_settings;
      cloud_settings.analysis_url = regionalized_url;
      GetParam().expected_settings->cloud_or_local_settings =
          CloudOrLocalAnalysisSettings(std::move(cloud_settings));
    }

    return GetParam().expected_settings;
  }
  DataRegion data_region() const { return GetParam().data_region; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<SourceDestinationTestingHelper>
      source_destination_testing_helper_;
};

TEST_P(AnalysisServiceSourceDestinationSettingsTest, CloudTest) {
  auto settings = base::JSONReader::Read(GetSettingsValue(),
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  AnalysisServiceSettings service_settings(settings.value(),
                                           *GetServiceProviderConfig());

  auto analysis_settings = service_settings.GetAnalysisSettings(
      fs_context(), source_url(), destination_url(), data_region());
  ASSERT_EQ((expected_settings() != nullptr), analysis_settings.has_value());
  if (analysis_settings.has_value()) {
    ASSERT_EQ(analysis_settings.value().block_until_verdict,
              expected_settings()->block_until_verdict);
    ASSERT_EQ(analysis_settings.value().default_action,
              expected_settings()->default_action);
    ASSERT_EQ(analysis_settings.value().block_password_protected_files,
              expected_settings()->block_password_protected_files);
    ASSERT_EQ(analysis_settings.value().block_large_files,
              expected_settings()->block_large_files);
    ASSERT_TRUE(
        analysis_settings.value().cloud_or_local_settings.is_cloud_analysis());
    ASSERT_EQ(analysis_settings.value().cloud_or_local_settings.analysis_url(),
              expected_settings()->cloud_or_local_settings.analysis_url());
    ASSERT_EQ(analysis_settings.value().minimum_data_size,
              expected_settings()->minimum_data_size);
    for (const auto& entry : expected_settings()->tags) {
      const std::string& tag = entry.first;
      ASSERT_TRUE(analysis_settings.value().tags.count(entry.first));
      ASSERT_EQ(analysis_settings.value().tags[tag].custom_message.message,
                entry.second.custom_message.message);
      if (!analysis_settings.value()
               .tags[tag]
               .custom_message.learn_more_url.is_empty()) {
        ASSERT_EQ(GetExpectedLearnMoreUrlSpecs().at(tag),
                  analysis_settings.value()
                      .tags[tag]
                      .custom_message.learn_more_url.spec());
        ASSERT_EQ(GetExpectedLearnMoreUrlSpecs().at(tag),
                  service_settings.GetLearnMoreUrl(tag).value().spec());
      }
      ASSERT_EQ(analysis_settings.value().tags[tag].requires_justification,
                entry.second.requires_justification);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AnalysisServiceSourceDestinationSettingsTest,
    testing::Values(
        // Validate that the enabled patterns match the expected patterns.
        SourceDestinationTestParam(kDlpMalwareVolumePair1,
                                   kNormalSourceDestinationSettings,
                                   NormalDlpAndMalwareSettings()),
        SourceDestinationTestParam(kDlpMalwareVolumePair2,
                                   kNormalSourceDestinationSettings,
                                   NormalDlpAndMalwareSettings()),
        SourceDestinationTestParam(kNoDlpNoMalwareVolumePair1,
                                   kNormalSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpNoMalwareVolumePair2,
                                   kNormalSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpMalwareVolumePair1,
                                   kNormalSourceDestinationSettings,
                                   NormalMalwareSettings()),
        SourceDestinationTestParam(kNoDlpMalwareVolumePair2,
                                   kNormalSourceDestinationSettings,
                                   NormalMalwareSettings()),
        SourceDestinationTestParam(kDlpNoMalwareVolumePair1,
                                   kNormalSourceDestinationSettings,
                                   NormalDlpSettings()),
        SourceDestinationTestParam(kDlpNoMalwareVolumePair2,
                                   kNormalSourceDestinationSettings,
                                   NormalDlpSettings()),

        // Validate that the enabled patterns match the expected patterns.
        SourceDestinationTestParam(kDlpMalwareVolumePair1,
                                   kOnlyEnabledSourceDestinationSettings,
                                   OnlyDlpAndMalwareEnabledSettings()),
        SourceDestinationTestParam(kDlpMalwareVolumePair2,
                                   kOnlyEnabledSourceDestinationSettings,
                                   OnlyDlpAndMalwareEnabledSettings()),
        SourceDestinationTestParam(kNoDlpNoMalwareVolumePair1,
                                   kOnlyEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpNoMalwareVolumePair2,
                                   kOnlyEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpMalwareVolumePair1,
                                   kOnlyEnabledSourceDestinationSettings,
                                   OnlyMalwareEnabledSettings()),
        SourceDestinationTestParam(kNoDlpMalwareVolumePair2,
                                   kOnlyEnabledSourceDestinationSettings,
                                   OnlyMalwareEnabledSettings()),
        SourceDestinationTestParam(kDlpNoMalwareVolumePair1,
                                   kOnlyEnabledSourceDestinationSettings,
                                   OnlyDlpEnabledSettings()),
        SourceDestinationTestParam(kDlpNoMalwareVolumePair2,
                                   kOnlyEnabledSourceDestinationSettings,
                                   OnlyDlpEnabledSettings()),

        // Validate that the enabled patterns match the expected patterns.
        SourceDestinationTestParam(
            kDlpMalwareVolumePair1,
            kEnableEntryIsNotADictSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kDlpMalwareVolumePair2,
            kEnableEntryIsNotADictSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kNoDlpNoMalwareVolumePair1,
            kEnableEntryIsNotADictSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kNoDlpNoMalwareVolumePair2,
            kEnableEntryIsNotADictSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kNoDlpMalwareVolumePair1,
            kEnableEntryIsNotADictSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kNoDlpMalwareVolumePair2,
            kEnableEntryIsNotADictSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kDlpNoMalwareVolumePair1,
            kEnableEntryIsNotADictSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kDlpNoMalwareVolumePair2,
            kEnableEntryIsNotADictSourceDestinationSettings,
            NoSettings()),

        // Validate that the enabled patterns match the expected patterns.
        SourceDestinationTestParam(
            kDlpMalwareVolumePair1,
            kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings,
            OnlyDlpAndMalwareEnabledSettings()),
        SourceDestinationTestParam(
            kDlpMalwareVolumePair2,
            kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings,
            OnlyDlpAndMalwareEnabledSettings()),
        SourceDestinationTestParam(
            kNoDlpNoMalwareVolumePair1,
            kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kNoDlpNoMalwareVolumePair2,
            kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings,
            NoSettings()),
        SourceDestinationTestParam(
            kNoDlpMalwareVolumePair1,
            kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings,
            OnlyMalwareEnabledSettings()),
        SourceDestinationTestParam(
            kNoDlpMalwareVolumePair2,
            kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings,
            OnlyMalwareEnabledSettings()),
        SourceDestinationTestParam(
            kDlpNoMalwareVolumePair1,
            kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings,
            OnlyDlpEnabledSettings()),
        SourceDestinationTestParam(
            kDlpNoMalwareVolumePair2,
            kOnlyEnabledPatternsAndIrrelevantSourceDestinationSettings,
            OnlyDlpEnabledSettings()),

        // Validate that the enabled patterns match the expected patterns.
        SourceDestinationTestParam(kDlpMalwareVolumePair1,
                                   kNoProviderSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kDlpMalwareVolumePair2,
                                   kNoProviderSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpNoMalwareVolumePair1,
                                   kNoProviderSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpNoMalwareVolumePair2,
                                   kNoProviderSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpMalwareVolumePair1,
                                   kNoProviderSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpMalwareVolumePair2,
                                   kNoProviderSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kDlpNoMalwareVolumePair1,
                                   kNoProviderSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kDlpNoMalwareVolumePair2,
                                   kNoProviderSourceDestinationSettings,
                                   NoSettings()),

        // Validate that the enabled patterns match the expected patterns.
        SourceDestinationTestParam(kDlpMalwareVolumePair1,
                                   kNothingEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kDlpMalwareVolumePair2,
                                   kNothingEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpNoMalwareVolumePair1,
                                   kNothingEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpNoMalwareVolumePair2,
                                   kNothingEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpMalwareVolumePair1,
                                   kNothingEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kNoDlpMalwareVolumePair2,
                                   kNothingEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kDlpNoMalwareVolumePair1,
                                   kNothingEnabledSourceDestinationSettings,
                                   NoSettings()),
        SourceDestinationTestParam(kDlpNoMalwareVolumePair2,
                                   kNothingEnabledSourceDestinationSettings,
                                   NoSettings()),

        // Validate that the enabled patterns match the expected patterns.
        SourceDestinationTestParam(
            kDlpMalwareVolumePair1,
            kNormalSourceDestinationSettingsWithCustomMessage,
            NormalSettingsWithCustomMessage()),

        SourceDestinationTestParam(
            kDlpMalwareVolumePair1,
            kNormalSourceDestinationSettingsDlpRequiresBypassJustification,
            NormalSettingsDlpRequiresBypassJustification()),

        // Validate regionalized endpoints.
        SourceDestinationTestParam(kDlpMalwareVolumePair1,
                                   kNormalSourceDestinationSettings,
                                   NormalDlpSettings(),
                                   DataRegion::UNITED_STATES),

        SourceDestinationTestParam(kDlpMalwareVolumePair1,
                                   kNormalSourceDestinationSettings,
                                   NormalDlpSettings(),
                                   DataRegion::EUROPE)));

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace enterprise_connectors
