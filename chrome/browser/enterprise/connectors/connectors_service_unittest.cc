// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kEmptySettingsPref[] = "[]";

constexpr char kNormalAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ],
    "disable": [
      {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
      {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
           "tags": ["malware"]}
    ],
    "block_until_verdict": 1,
    "block_password_protected": true,
    "block_large_files": true,
    "block_unsupported_file_types": true
  }
])";

constexpr char kNormalReportingSettingsPref[] = R"([
  {
    "service_provider": "google"
  }
])";

constexpr char kNormalSendDownloadToCloudPref[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": ["*"],
        "mime_types": ["text/plain", "image/png", "application/zip"]
      }
    ],
    "disable": [
      {
        "url_list": ["no.text.com", "no.text.no.image.com"],
        "mime_types": ["text/plain"]
      },
      {
        "url_list": ["no.image.com", "no.text.no.image.com"],
        "mime_types": ["image/png"]
      }
    ]
  }
])";

constexpr char kDlpAndMalwareUrl[] = "https://foo.com";
constexpr char kOnlyDlpUrl[] = "https://no.malware.com";
constexpr char kOnlyMalwareUrl[] = "https://no.dlp.com";
constexpr char kNoTagsUrl[] = "https://no.dlp.or.malware.ca";

}  // namespace
class ConnectorsServiceTest : public testing::Test {
 public:
  ConnectorsServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("fake-token"));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
};

class ConnectorsServiceAnalysisNoFeatureTest
    : public ConnectorsServiceTest,
      public testing::WithParamInterface<AnalysisConnector> {
 public:
  ConnectorsServiceAnalysisNoFeatureTest() {
    scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
  }

  AnalysisConnector connector() { return GetParam(); }
};

TEST_P(ConnectorsServiceAnalysisNoFeatureTest, AnalysisConnectors) {
  profile_->GetPrefs()->Set(
      ConnectorPref(connector()),
      *base::JSONReader::Read(kNormalAnalysisSettingsPref));
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);
  for (const char* url :
       {kDlpAndMalwareUrl, kOnlyDlpUrl, kOnlyMalwareUrl, kNoTagsUrl}) {
    // Only base::nullopt should be returned when the feature is disabled,
    // regardless of what Connector or URL is used.
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_FALSE(settings.has_value());
  }

  // No cached settings imply the connector value was never read.
  ASSERT_TRUE(service->ConnectorsManagerForTesting()
                  ->GetAnalysisConnectorsSettingsForTesting()
                  .empty());
}

INSTANTIATE_TEST_CASE_P(,
                        ConnectorsServiceAnalysisNoFeatureTest,
                        testing::Values(FILE_ATTACHED,
                                        FILE_DOWNLOADED,
                                        BULK_DATA_ENTRY));

// Tests to make sure getting reporting settings work with both the feature flag
// and the OnSecurityEventEnterpriseConnector policy. The parameter for these
// tests is a tuple of:
//
//   enum class ReportingConnector[]: array of all reporting connectors.
//   bool: enable feature flag.
//   int: policy value.  0: don't set, 1: set to normal, 2: set to empty.
class ConnectorsServiceReportingFeatureTest
    : public ConnectorsServiceTest,
      public testing::WithParamInterface<
          std::tuple<ReportingConnector, bool, int>> {
 public:
  ConnectorsServiceReportingFeatureTest() {
    if (enable_feature_flag()) {
      scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
    }
  }

  ReportingConnector connector() const { return std::get<0>(GetParam()); }
  bool enable_feature_flag() const { return std::get<1>(GetParam()); }
  int policy_value() const { return std::get<2>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }

  const char* scope_pref() const { return ConnectorScopePref(connector()); }

  const char* pref_value() const {
    switch (policy_value()) {
      case 1:
        return kNormalReportingSettingsPref;
      case 2:
        return kEmptySettingsPref;
    }
    NOTREACHED();
    return nullptr;
  }

  bool reporting_enabled() const {
    return enable_feature_flag() && policy_value() == 1;
  }

  void ValidateSettings(const ReportingSettings& settings) {
    // For now, the URL is the same for both legacy and new policies, so
    // checking the specific URL here.  When service providers become
    // configurable this will change.
    ASSERT_EQ(GURL("https://chromereporting-pa.googleapis.com/v1/events"),
              settings.reporting_url);
  }
};

TEST_P(ConnectorsServiceReportingFeatureTest, Test) {
  if (policy_value() != 0) {
    profile_->GetPrefs()->Set(pref(), *base::JSONReader::Read(pref_value()));
    profile_->GetPrefs()->SetInteger(scope_pref(),
                                     policy::POLICY_SCOPE_MACHINE);
  }

  auto settings = ConnectorsServiceFactory::GetForBrowserContext(profile_)
                      ->GetReportingSettings(connector());
  EXPECT_EQ(reporting_enabled(), settings.has_value());
  if (settings.has_value())
    ValidateSettings(settings.value());

  EXPECT_EQ(enable_feature_flag() && policy_value() == 1,
            !ConnectorsServiceFactory::GetForBrowserContext(profile_)
                 ->ConnectorsManagerForTesting()
                 ->GetReportingConnectorsSettingsForTesting()
                 .empty());
}

INSTANTIATE_TEST_CASE_P(
    ,
    ConnectorsServiceReportingFeatureTest,
    testing::Combine(testing::Values(ReportingConnector::SECURITY_EVENT),
                     testing::Bool(),
                     testing::ValuesIn({0, 1, 2})));

// Tests to make sure file system settings work with both the feature flag
// and the policy. The parameter for these tests is a tuple of:
//
//   enum class FileSystemConnector[]: array of all file system connectors.
//   bool: enable feature flag.
//   int: policy value.  0: don't set, 1: set to normal, 2: set to empty.
class ConnectorsServiceFileSystemFeatureTest
    : public ConnectorsServiceTest,
      public testing::WithParamInterface<
          std::tuple<FileSystemConnector, bool, int>> {
 public:
  ConnectorsServiceFileSystemFeatureTest() {
    if (enable_feature_flag()) {
      scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
    }
  }

  FileSystemConnector connector() const { return std::get<0>(GetParam()); }
  bool enable_feature_flag() const { return std::get<1>(GetParam()); }
  int policy_value() const { return std::get<2>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }

  const char* pref_value() const {
    switch (policy_value()) {
      case 1:
        return kNormalSendDownloadToCloudPref;
      case 2:
        return kEmptySettingsPref;
    }
    NOTREACHED();
    return nullptr;
  }

  bool file_system_enabled() const {
    return enable_feature_flag() && policy_value() == 1;
  }

  void ValidateSettings(const FileSystemSettings& settings) {
    // Mime types are the only setting affect by the policy, the rest are
    // just copied from the service provider comfig.  So only need to validate
    // this in tests.
    ASSERT_EQ(settings.mime_types, expected_mime_types_);
  }

  void set_expected_mime_types(std::set<std::string> expected_mime_types) {
    expected_mime_types_ = std::move(expected_mime_types);
  }

 private:
  std::set<std::string> expected_mime_types_;
};

TEST_P(ConnectorsServiceFileSystemFeatureTest, Test) {
  if (policy_value() != 0) {
    profile_->GetPrefs()->Set(pref(), *base::JSONReader::Read(pref_value()));
  }

  auto settings =
      ConnectorsServiceFactory::GetForBrowserContext(profile_)
          ->GetFileSystemSettings(GURL("https://any.com"), connector());
  EXPECT_EQ(file_system_enabled(), settings.has_value());
  if (settings.has_value()) {
    set_expected_mime_types({"text/plain", "image/png", "application/zip"});
    ValidateSettings(settings.value());
  }

  EXPECT_EQ(enable_feature_flag() && policy_value() == 1,
            !ConnectorsServiceFactory::GetForBrowserContext(profile_)
                 ->ConnectorsManagerForTesting()
                 ->GetFileSystemConnectorsSettingsForTesting()
                 .empty());
}

INSTANTIATE_TEST_CASE_P(
    ,
    ConnectorsServiceFileSystemFeatureTest,
    testing::Combine(
        testing::Values(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        testing::Bool(),
        testing::ValuesIn({0, 1, 2})));

TEST_F(ConnectorsServiceTest, RealtimeURLCheck) {
  profile_->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
      safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile_->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  auto maybe_dm_token = ConnectorsServiceFactory::GetForBrowserContext(profile_)
                            ->GetDMTokenForRealTimeUrlCheck();
  EXPECT_TRUE(maybe_dm_token.has_value());
  EXPECT_EQ("fake-token", maybe_dm_token.value());

  policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyTokenForTesting());

  maybe_dm_token = ConnectorsServiceFactory::GetForBrowserContext(profile_)
                       ->GetDMTokenForRealTimeUrlCheck();
  EXPECT_FALSE(maybe_dm_token.has_value());
}

}  // namespace enterprise_connectors
