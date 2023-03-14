// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/enterprise/connectors/analysis/source_destination_test_util.h"
#endif

namespace enterprise_connectors {

namespace {

constexpr AnalysisConnector kAllAnalysisConnectors[] = {
    AnalysisConnector::FILE_DOWNLOADED, AnalysisConnector::FILE_ATTACHED,
    AnalysisConnector::BULK_DATA_ENTRY, AnalysisConnector::PRINT};

constexpr ReportingConnector kAllReportingConnectors[] = {
    ReportingConnector::SECURITY_EVENT};

constexpr char kEmptySettingsPref[] = "[]";

constexpr char kNormalCloudAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]},
    ],
    "disable": [
      {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
      {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
           "tags": ["malware"]},
    ],
    "block_until_verdict": 1,
    "block_password_protected": true,
    "block_large_files": true,
    "block_unsupported_file_types": true,
  },
])";

constexpr char kNormalLocalAnalysisSettingsPref[] = R"([
  {
    "service_provider": "local_user_agent",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp"]},
    ],
    "disable": [
      {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    ],
    "block_until_verdict": 1,
    "block_password_protected": true,
    "block_large_files": true,
    "block_unsupported_file_types": true,
  },
])";

constexpr char kNormalReportingSettingsPref[] = R"([
  {
    "service_provider": "google"
  }
])";

constexpr char kDlpAndMalwareUrl[] = "https://foo.com";
constexpr char kOnlyDlpUrl[] = "https://no.malware.com";
constexpr char kOnlyMalwareUrl[] = "https://no.dlp.com";
constexpr char kNoTagsUrl[] = "https://no.dlp.or.malware.ca";

}  // namespace

class ConnectorsManagerTest : public testing::Test {
 public:
  ConnectorsManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  PrefService* pref_service() { return profile_->GetPrefs(); }

  void ValidateSettings(const AnalysisSettings& settings) {
    ASSERT_EQ(settings.block_until_verdict, expected_block_until_verdict_);
    ASSERT_EQ(settings.block_password_protected_files,
              expected_block_password_protected_files_);
    ASSERT_EQ(settings.block_large_files, expected_block_large_files_);
    ASSERT_EQ(settings.block_unsupported_file_types,
              expected_block_unsupported_file_types_);
    for (const auto& expected_tag : expected_tags_) {
      const std::string& tag = expected_tag.first;
      ASSERT_TRUE(settings.tags.count(tag));
      ASSERT_EQ(settings.tags.at(tag).requires_justification,
                expected_tag.second.requires_justification);
      ASSERT_EQ(settings.tags.at(tag).custom_message.message,
                expected_tag.second.custom_message.message);
      ASSERT_EQ(settings.tags.at(tag).custom_message.learn_more_url,
                expected_tag.second.custom_message.learn_more_url);
    }
  }

  void ValidateSettings(const ReportingSettings& settings) {
    // For now, the URL is the same for both legacy and new policies, so
    // checking the specific URL here.  When service providers become
    // configurable this will change.
    ASSERT_EQ(GURL("https://chromereporting-pa.googleapis.com/v1/events"),
              settings.reporting_url);
  }

  class ScopedConnectorPref {
   public:
    ScopedConnectorPref(PrefService* pref_service,
                        const char* pref,
                        const char* pref_value)
        : pref_service_(pref_service), pref_(pref) {
      auto maybe_pref_value =
          base::JSONReader::Read(pref_value, base::JSON_ALLOW_TRAILING_COMMAS);
      EXPECT_TRUE(maybe_pref_value.has_value());
      pref_service_->Set(pref, maybe_pref_value.value());
    }

    ~ScopedConnectorPref() { pref_service_->ClearPref(pref_); }

   private:
    raw_ptr<PrefService> pref_service_;
    const char* pref_;
  };

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;

  // Set to the default value of their legacy policy.
  std::map<std::string, TagSettings> expected_tags_ = {};
  BlockUntilVerdict expected_block_until_verdict_ = BlockUntilVerdict::kNoBlock;
  bool expected_block_password_protected_files_ = false;
  bool expected_block_large_files_ = false;
  bool expected_block_unsupported_file_types_ = false;

  std::set<std::string> expected_mime_types_;
};

// Platform policies should only act as a kill switch.
class ConnectorsManagerLocalAnalysisPolicyTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<
          std::tuple<AnalysisConnector, bool, bool>> {
 protected:
  AnalysisConnector connector() const { return std::get<0>(GetParam()); }
  bool enable_feature() const { return std::get<1>(GetParam()); }
  bool set_policy() const { return std::get<2>(GetParam()); }
};

TEST_P(ConnectorsManagerLocalAnalysisPolicyTest, Test) {
  if (enable_feature()) {
    scoped_feature_list_.InitWithFeatures({kLocalContentAnalysisEnabled}, {});
  } else {
    scoped_feature_list_.InitWithFeatures({}, {kLocalContentAnalysisEnabled});
  }

  std::unique_ptr<ScopedConnectorPref> scoped_pref =
      set_policy() ? std::make_unique<ScopedConnectorPref>(
                         pref_service(), ConnectorPref(connector()),
                         kNormalLocalAnalysisSettingsPref)
                   : nullptr;

  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  EXPECT_EQ(enable_feature() && set_policy(),
            manager.IsConnectorEnabled(connector()));
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerLocalAnalysisPolicyTest,
    ConnectorsManagerLocalAnalysisPolicyTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::Bool(),
                     testing::Bool()));

class ConnectorsManagerConnectorPoliciesTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<
          std::tuple<AnalysisConnector, const char*, const char*>> {
 public:
  ConnectorsManagerConnectorPoliciesTest() = default;

  AnalysisConnector connector() const { return std::get<0>(GetParam()); }

  const char* url() const { return std::get<1>(GetParam()); }

  const char* pref_value() const { return std::get<2>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }

  void SetUpExpectedAnalysisSettings(const char* pref) {
    auto expected_settings = ExpectedAnalysisSettings(pref, url());
    expect_settings_ = expected_settings.has_value();
    if (expected_settings.has_value()) {
      expected_tags_ = expected_settings.value().tags;
      expected_block_until_verdict_ =
          expected_settings.value().block_until_verdict;
      expected_block_password_protected_files_ =
          expected_settings.value().block_password_protected_files;
      expected_block_unsupported_file_types_ =
          expected_settings.value().block_unsupported_file_types;
      expected_block_large_files_ = expected_settings.value().block_large_files;
    }
  }

 protected:
  absl::optional<AnalysisSettings> ExpectedAnalysisSettings(const char* pref,
                                                            const char* url) {
    if (pref == kEmptySettingsPref || url == kNoTagsUrl)
      return absl::nullopt;

    AnalysisSettings settings;

    settings.block_until_verdict = BlockUntilVerdict::kBlock;
    settings.block_password_protected_files = true;
    settings.block_large_files = true;
    settings.block_unsupported_file_types = true;

    if (url == kDlpAndMalwareUrl)
      settings.tags = {{"dlp", TagSettings()}, {"malware", TagSettings()}};
    else if (url == kOnlyDlpUrl)
      settings.tags = {{"dlp", TagSettings()}};
    else if (url == kOnlyMalwareUrl)
      settings.tags = {{"malware", TagSettings()}};

    // The "local_test" service provider doesn't support the "malware" tag, so
    // remove it from expectations.
    if (pref == kNormalLocalAnalysisSettingsPref)
      settings.tags.erase("malware");
    if (settings.tags.empty())
      return absl::nullopt;

    return settings;
  }

  bool expect_settings_;
};

TEST_P(ConnectorsManagerConnectorPoliciesTest, NormalPref) {
  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
  ScopedConnectorPref scoped_pref(pref_service(), pref(), pref_value());
  SetUpExpectedAnalysisSettings(pref_value());

  // Verify that the expected settings are returned normally.
  auto settings_from_manager =
      manager.GetAnalysisSettings(GURL(url()), connector());
  ASSERT_EQ(expect_settings_, settings_from_manager.has_value());
  if (settings_from_manager.has_value())
    ValidateSettings(settings_from_manager.value());

  // Verify that the expected settings are also returned by the cached settings.
  const auto& cached_settings =
      manager.GetAnalysisConnectorsSettingsForTesting();
  ASSERT_EQ(1u, cached_settings.size());
  ASSERT_EQ(1u, cached_settings.count(connector()));
  ASSERT_EQ(1u, cached_settings.at(connector()).size());

  auto settings_from_cache =
      cached_settings.at(connector()).at(0).GetAnalysisSettings(GURL(url()));
  ASSERT_EQ(expect_settings_, settings_from_cache.has_value());
  if (settings_from_cache.has_value())
    ValidateSettings(settings_from_cache.value());
}

TEST_P(ConnectorsManagerConnectorPoliciesTest, EmptyPref) {
  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  // If the connector's settings list is empty, no analysis settings are ever
  // returned.
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
  ScopedConnectorPref scoped_pref(pref_service(), pref(), kEmptySettingsPref);

  ASSERT_FALSE(
      manager.GetAnalysisSettings(GURL(url()), connector()).has_value());

  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerConnectorPoliciesTest,
    ConnectorsManagerConnectorPoliciesTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::Values(kDlpAndMalwareUrl,
                                     kOnlyDlpUrl,
                                     kOnlyMalwareUrl,
                                     kNoTagsUrl),
                     testing::Values(kNormalCloudAnalysisSettingsPref,
                                     kNormalLocalAnalysisSettingsPref)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
using VolumeInfo = SourceDestinationTestingHelper::VolumeInfo;

namespace {

constexpr char kNormalCloudSourceDestinationSettingsPref[] = R"([{
  "service_provider": "google",
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
  "block_password_protected": true,
  "block_large_files": true,
  "block_unsupported_file_types": true,
  "minimum_data_size": 123,
}])";

constexpr char kNormalLocalSourceDestinationSettingsPref[] = R"([{
  "service_provider": "local_user_agent",
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
  "block_password_protected": true,
  "block_large_files": true,
  "block_unsupported_file_types": true,
  "minimum_data_size": 123,
}])";

constexpr VolumeInfo kRemovableVolumeInfo{
    file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION, absl::nullopt,
    "REMOVABLE"};
constexpr VolumeInfo kProvidedVolumeInfo{file_manager::VOLUME_TYPE_PROVIDED,
                                         absl::nullopt, "PROVIDED"};
constexpr VolumeInfo kMyFilesVolumeInfo{
    file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY, absl::nullopt, "MY_FILES"};
constexpr VolumeInfo kDriveVolumeInfo{file_manager::VOLUME_TYPE_GOOGLE_DRIVE,
                                      absl::nullopt, "GOOGLE_DRIVE"};

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

using SourceDestinationTestingTuple =
    std::tuple<AnalysisConnector,
               const std::pair<VolumeInfo, VolumeInfo>*,
               const char*>;

static auto testingTupleToString = [](const auto& info) {
  // Can use info.param here to generate the test suffix
  std::string name;
  auto [connector, volume_info_pair, pref] = info.param;
  name += volume_info_pair->first.fs_config_string;
  name += "_";
  name += volume_info_pair->second.fs_config_string;
  if (pref == kNormalCloudSourceDestinationSettingsPref) {
    name += "_cloud";
  } else if (pref == kNormalLocalSourceDestinationSettingsPref) {
    name += "_local";
  } else {
    name += "_unknown";
  }
  return name;
};

}  // namespace

class ConnectorsManagerConnectorPoliciesSourceDestinationTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<SourceDestinationTestingTuple> {
 public:
  ConnectorsManagerConnectorPoliciesSourceDestinationTest() {
    source_destination_testing_helper_ =
        std::make_unique<SourceDestinationTestingHelper>(profile_,
                                                         kVolumeInfos);
  }

  ~ConnectorsManagerConnectorPoliciesSourceDestinationTest() override {
    // The testing profile has to be deleted before
    // source_destination_testing_helper_ is destroyed.
    profile_manager_.DeleteAllTestingProfiles();
  }

  AnalysisConnector connector() const { return std::get<0>(GetParam()); }

  storage::FileSystemURL source_volume_url() const {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        std::get<1>(GetParam())->first);
  }
  storage::FileSystemURL destination_volume_url() const {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        std::get<1>(GetParam())->second);
  }

  const char* pref_value() const { return std::get<2>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }

  void SetUpExpectedAnalysisSettings(const char* pref) {
    auto expected_settings =
        ExpectedAnalysisSettings(pref, std::get<1>(GetParam()));
    expect_settings_ = expected_settings.has_value();
    if (expected_settings.has_value()) {
      expected_tags_ = expected_settings.value().tags;
      expected_block_until_verdict_ =
          expected_settings.value().block_until_verdict;
      expected_block_password_protected_files_ =
          expected_settings.value().block_password_protected_files;
      expected_block_unsupported_file_types_ =
          expected_settings.value().block_unsupported_file_types;
      expected_block_large_files_ = expected_settings.value().block_large_files;
    }
  }

 protected:
  absl::optional<AnalysisSettings> ExpectedAnalysisSettings(
      const char* pref,
      const std::pair<VolumeInfo, VolumeInfo>* volume_pair) {
    if (pref == kEmptySettingsPref ||
        volume_pair == &kNoDlpNoMalwareVolumePair1 ||
        volume_pair == &kNoDlpNoMalwareVolumePair2)
      return absl::nullopt;

    AnalysisSettings settings;

    settings.block_until_verdict = BlockUntilVerdict::kBlock;
    settings.block_password_protected_files = true;
    settings.block_large_files = true;
    settings.block_unsupported_file_types = true;

    if (volume_pair == &kDlpMalwareVolumePair1 ||
        volume_pair == &kDlpMalwareVolumePair2) {
      settings.tags = {{"dlp", TagSettings()}, {"malware", TagSettings()}};
    } else if (volume_pair == &kDlpNoMalwareVolumePair1 ||
               volume_pair == &kDlpNoMalwareVolumePair2) {
      settings.tags = {{"dlp", TagSettings()}};
    } else if (volume_pair == &kNoDlpMalwareVolumePair1 ||
               volume_pair == &kNoDlpMalwareVolumePair2) {
      settings.tags = {{"malware", TagSettings()}};
    } else {
      NOTREACHED();
    }

    // The "local_user_agent" service provider doesn't support the "malware"
    // tag, so remove it from expectations.
    if (pref == kNormalLocalSourceDestinationSettingsPref)
      settings.tags.erase("malware");
    if (settings.tags.empty())
      return absl::nullopt;

    return settings;
  }

  std::unique_ptr<SourceDestinationTestingHelper>
      source_destination_testing_helper_;
  bool expect_settings_;
};

TEST_P(ConnectorsManagerConnectorPoliciesSourceDestinationTest, NormalPref) {
  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
  ScopedConnectorPref scoped_pref(pref_service(), pref(), pref_value());
  SetUpExpectedAnalysisSettings(pref_value());

  // Verify that the expected settings are returned normally.
  auto settings_from_manager = manager.GetAnalysisSettings(
      profile_, source_volume_url(), destination_volume_url(), connector());
  ASSERT_EQ(expect_settings_, settings_from_manager.has_value());
  if (settings_from_manager.has_value())
    ValidateSettings(settings_from_manager.value());

  // Verify that the expected settings are also returned by the cached settings.
  const auto& cached_settings =
      manager.GetAnalysisConnectorsSettingsForTesting();
  ASSERT_EQ(1u, cached_settings.size());
  ASSERT_EQ(1u, cached_settings.count(connector()));
  ASSERT_EQ(1u, cached_settings.at(connector()).size());

  auto settings_from_cache =
      cached_settings.at(connector())
          .at(0)
          .GetAnalysisSettings(profile_, source_volume_url(),
                               destination_volume_url());
  ASSERT_EQ(expect_settings_, settings_from_cache.has_value());
  if (settings_from_cache.has_value())
    ValidateSettings(settings_from_cache.value());
}

TEST_P(ConnectorsManagerConnectorPoliciesSourceDestinationTest, EmptyPref) {
  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  // If the connector's settings list is empty, no analysis settings are ever
  // returned.
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
  ScopedConnectorPref scoped_pref(pref_service(), pref(), kEmptySettingsPref);

  ASSERT_FALSE(manager
                   .GetAnalysisSettings(profile_, source_volume_url(),
                                        destination_volume_url(), connector())
                   .has_value());

  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsManagerConnectorPoliciesSourceDestinationTest,
    testing::Combine(
        testing::Values(AnalysisConnector::FILE_TRANSFER),
        testing::Values(&kDlpMalwareVolumePair1,
                        &kDlpMalwareVolumePair2,
                        &kNoDlpNoMalwareVolumePair1,
                        &kNoDlpNoMalwareVolumePair2,
                        &kNoDlpMalwareVolumePair1,
                        &kNoDlpMalwareVolumePair2,
                        &kDlpNoMalwareVolumePair1,
                        &kDlpNoMalwareVolumePair2),
        testing::Values(kNormalCloudSourceDestinationSettingsPref)),
    testingTupleToString);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class ConnectorsManagerAnalysisConnectorsTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<
          std::tuple<AnalysisConnector, const char*>> {
 public:
  AnalysisConnector connector() const { return std::get<0>(GetParam()); }

  const char* pref_value() const { return std::get<1>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }
};

TEST_P(ConnectorsManagerAnalysisConnectorsTest, DynamicPolicies) {
  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  // The cache is initially empty.
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());

  // Once the pref is updated, the settings should be cached, and analysis
  // settings can be obtained.
  {
    ScopedConnectorPref scoped_pref(pref_service(), pref(), pref_value());

    const auto& cached_settings =
        manager.GetAnalysisConnectorsSettingsForTesting();
    ASSERT_FALSE(cached_settings.empty());
    ASSERT_EQ(1u, cached_settings.count(connector()));
    ASSERT_EQ(1u, cached_settings.at(connector()).size());

    auto settings = cached_settings.at(connector())
                        .at(0)
                        .GetAnalysisSettings(GURL(kDlpAndMalwareUrl));
    ASSERT_TRUE(settings.has_value());
    expected_block_until_verdict_ = BlockUntilVerdict::kBlock;
    expected_block_password_protected_files_ = true;
    expected_block_large_files_ = true;
    expected_block_unsupported_file_types_ = true;

    // The "local_test" service provider doesn't support the "malware" tag, so
    // remove it from expectations.
    if (pref_value() == kNormalCloudAnalysisSettingsPref)
      expected_tags_ = {{"dlp", TagSettings()}, {"malware", TagSettings()}};
    else
      expected_tags_ = {{"dlp", TagSettings()}};

    ValidateSettings(settings.value());
  }

  // The cache should be empty again after the pref is reset.
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
}

TEST_P(ConnectorsManagerAnalysisConnectorsTest, NamesAndConfigs) {
  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  ScopedConnectorPref scoped_pref(pref_service(), pref(), pref_value());

  auto names = manager.GetAnalysisServiceProviderNames(connector());
  ASSERT_EQ(1u, names.size());

  auto configs = manager.GetAnalysisServiceConfigs(connector());
  ASSERT_EQ(1u, configs.size());

  if (names[0] == "google") {
    EXPECT_TRUE(configs[0]->url);
    EXPECT_FALSE(configs[0]->local_path);
  } else if (names[0] == "local_user_agent") {
    EXPECT_FALSE(configs[0]->url);
    EXPECT_TRUE(configs[0]->local_path);
  } else {
    NOTREACHED() << "Unexpected service provider name";
  }
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerAnalysisConnectorsTest,
    ConnectorsManagerAnalysisConnectorsTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::Values(kNormalCloudAnalysisSettingsPref,
                                     kNormalLocalAnalysisSettingsPref)));

#if BUILDFLAG(IS_CHROMEOS_ASH)

class ConnectorsManagerAnalysisConnectorsSourceDestinationTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<
          std::tuple<AnalysisConnector, const char*>> {
 public:
  ConnectorsManagerAnalysisConnectorsSourceDestinationTest() {
    source_destination_testing_helper_ =
        std::make_unique<SourceDestinationTestingHelper>(profile_,
                                                         kVolumeInfos);
  }

  ~ConnectorsManagerAnalysisConnectorsSourceDestinationTest() override {
    // The testing profile has to be deleted before
    // source_destination_testing_helper_ is destroyed.
    profile_manager_.DeleteAllTestingProfiles();
  }

  storage::FileSystemURL source_volume_url() const {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        kDlpMalwareVolumePair1.first);
  }
  storage::FileSystemURL destination_volume_url() const {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        kDlpMalwareVolumePair1.second);
  }

  AnalysisConnector connector() const { return std::get<0>(GetParam()); }

  const char* pref_value() const { return std::get<1>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }

 protected:
  std::unique_ptr<SourceDestinationTestingHelper>
      source_destination_testing_helper_;
};

TEST_P(ConnectorsManagerAnalysisConnectorsSourceDestinationTest,
       DynamicPolicies) {
  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  // The cache is initially empty.
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());

  // Once the pref is updated, the settings should be cached, and analysis
  // settings can be obtained.
  {
    ScopedConnectorPref scoped_pref(pref_service(), pref(), pref_value());

    const auto& cached_settings =
        manager.GetAnalysisConnectorsSettingsForTesting();
    ASSERT_FALSE(cached_settings.empty());
    ASSERT_EQ(1u, cached_settings.count(connector()));
    ASSERT_EQ(1u, cached_settings.at(connector()).size());

    auto settings = cached_settings.at(connector())
                        .at(0)
                        .GetAnalysisSettings(profile_, source_volume_url(),
                                             destination_volume_url());
    ASSERT_TRUE(settings.has_value());
    expected_block_until_verdict_ = BlockUntilVerdict::kBlock;
    expected_block_password_protected_files_ = true;
    expected_block_large_files_ = true;
    expected_block_unsupported_file_types_ = true;

    // The "local_test" service provider doesn't support the "malware" tag, so
    // remove it from expectations.
    if (pref_value() == kNormalCloudSourceDestinationSettingsPref)
      expected_tags_ = {{"dlp", TagSettings()}, {"malware", TagSettings()}};
    else
      expected_tags_ = {{"dlp", TagSettings()}};

    ValidateSettings(settings.value());
  }

  // The cache should be empty again after the pref is reset.
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerAnalysisConnectorsSourceDestinationTest,
    ConnectorsManagerAnalysisConnectorsSourceDestinationTest,
    testing::Combine(
        testing::Values(AnalysisConnector::FILE_TRANSFER),
        testing::Values(kNormalCloudSourceDestinationSettingsPref,
                        kNormalLocalSourceDestinationSettingsPref)));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class ConnectorsManagerReportingTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<ReportingConnector> {
 public:
  ReportingConnector connector() const { return GetParam(); }

  const char* pref() const { return ConnectorPref(connector()); }
};

TEST_P(ConnectorsManagerReportingTest, DynamicPolicies) {
  ConnectorsManager manager(
      std::make_unique<BrowserCrashEventRouter>(profile_),
      std::make_unique<ExtensionInstallEventRouter>(profile_), pref_service(),
      GetServiceProviderConfig());
  // The cache is initially empty.
  ASSERT_TRUE(manager.GetReportingConnectorsSettingsForTesting().empty());

  // Once the pref is updated, the settings should be cached, and reporting
  // settings can be obtained.
  {
    ScopedConnectorPref scoped_pref(pref_service(), pref(),
                                    kNormalReportingSettingsPref);

    const auto& cached_settings =
        manager.GetReportingConnectorsSettingsForTesting();
    ASSERT_FALSE(cached_settings.empty());
    ASSERT_EQ(1u, cached_settings.count(connector()));
    ASSERT_EQ(1u, cached_settings.at(connector()).size());

    auto settings =
        cached_settings.at(connector()).at(0).GetReportingSettings();
    ASSERT_TRUE(settings.has_value());
    ValidateSettings(settings.value());
  }

  // The cache should be empty again after the pref is reset.
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
}

INSTANTIATE_TEST_SUITE_P(ConnectorsManagerReportingTest,
                         ConnectorsManagerReportingTest,
                         testing::ValuesIn(kAllReportingConnectors));

}  // namespace enterprise_connectors
