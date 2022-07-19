// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

constexpr AnalysisConnector kAllAnalysisConnectors[] = {
    AnalysisConnector::FILE_DOWNLOADED, AnalysisConnector::FILE_ATTACHED,
    AnalysisConnector::BULK_DATA_ENTRY, AnalysisConnector::PRINT};

constexpr ReportingConnector kAllReportingConnectors[] = {
    ReportingConnector::SECURITY_EVENT};

constexpr FileSystemConnector kAllFileSystemConnectors[] = {
    FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD,
};

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
    "service_provider": "local_test",
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

constexpr char kNormalSendDownloadToCloudPolicy[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": ["*"],
        "mime_types": ["text/plain", "image/png", "application/zip"],
      },
    ],
    "disable": [
      {
        "url_list": ["no.text.com", "no.text.no.image.com"],
        "mime_types": ["text/plain"],
      },
      {
        "url_list": ["no.image.com", "no.text.no.image.com"],
        "mime_types": ["image/png"],
      },
    ],
  },
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

  void ValidateSettings(const FileSystemSettings& settings) {
    // Mime types are the only setting affect by the policy, the rest are
    // just copied from the service provider comfig.  So only need to validate
    // this in tests.
    ASSERT_EQ(settings.mime_types, expected_mime_types_);
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
  GURL url_ = GURL("https://google.com");

  // Set to the default value of their legacy policy.
  std::map<std::string, TagSettings> expected_tags_ = {};
  BlockUntilVerdict expected_block_until_verdict_ = BlockUntilVerdict::kNoBlock;
  bool expected_block_password_protected_files_ = false;
  bool expected_block_large_files_ = false;
  bool expected_block_unsupported_file_types_ = false;

  std::set<std::string> expected_mime_types_;
};

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
  ConnectorsManager manager(ExtensionInstallEventRouter(profile_),
                            pref_service(), GetServiceProviderConfig());
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
  ConnectorsManager manager(ExtensionInstallEventRouter(profile_),
                            pref_service(), GetServiceProviderConfig());
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

class ConnectorsManagerAnalysisConnectorsTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<
          std::tuple<AnalysisConnector, const char*>> {
 public:
  explicit ConnectorsManagerAnalysisConnectorsTest(bool enable = true) {
    if (enable) {
      scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
    }
  }

  AnalysisConnector connector() const { return std::get<0>(GetParam()); }

  const char* pref_value() const { return std::get<1>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }
};

TEST_P(ConnectorsManagerAnalysisConnectorsTest, DynamicPolicies) {
  ConnectorsManager manager(ExtensionInstallEventRouter(profile_),
                            pref_service(), GetServiceProviderConfig());
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

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerAnalysisConnectorsTest,
    ConnectorsManagerAnalysisConnectorsTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::Values(kNormalCloudAnalysisSettingsPref,
                                     kNormalLocalAnalysisSettingsPref)));

class ConnectorsManagerReportingTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<ReportingConnector> {
 public:
  ConnectorsManagerReportingTest() {
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
  }

  ReportingConnector connector() const { return GetParam(); }

  const char* pref() const { return ConnectorPref(connector()); }
};

TEST_P(ConnectorsManagerReportingTest, DynamicPolicies) {
  ConnectorsManager manager(ExtensionInstallEventRouter(profile_),
                            pref_service(), GetServiceProviderConfig());
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

class ConnectorsManagerFileSystemTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<FileSystemConnector> {
 public:
  ConnectorsManagerFileSystemTest() {
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
  }

  FileSystemConnector connector() const { return GetParam(); }

  const char* pref() const { return ConnectorPref(connector()); }
};

TEST_P(ConnectorsManagerFileSystemTest, DynamicPolicies) {
  ConnectorsManager manager(ExtensionInstallEventRouter(profile_),
                            pref_service(), GetServiceProviderConfig());
  // The cache is initially empty.
  ASSERT_TRUE(manager.GetFileSystemConnectorsSettingsForTesting().empty());

  // Once the pref is updated, the settings should be cached, and reporting
  // settings can be obtained.
  {
    ScopedConnectorPref scoped_pref(pref_service(), pref(),
                                    kNormalSendDownloadToCloudPolicy);

    const auto& cached_settings =
        manager.GetFileSystemConnectorsSettingsForTesting();
    ASSERT_FALSE(cached_settings.empty());
    ASSERT_EQ(1u, cached_settings.count(connector()));
    ASSERT_EQ(1u, cached_settings.at(connector()).size());

    expected_mime_types_ = {"text/plain", "image/png", "application/zip"};

    auto settings = cached_settings.at(connector())
                        .at(0)
                        .GetSettings(GURL("https://any.com"));
    ASSERT_TRUE(settings.has_value());

    ValidateSettings(settings.value());
  }

  // The cache should be empty again after the pref is reset.
  ASSERT_TRUE(manager.GetAnalysisConnectorsSettingsForTesting().empty());
}

INSTANTIATE_TEST_SUITE_P(ConnectorsManagerFileSystemTest,
                         ConnectorsManagerFileSystemTest,
                         testing::ValuesIn(kAllFileSystemConnectors));

}  // namespace enterprise_connectors
