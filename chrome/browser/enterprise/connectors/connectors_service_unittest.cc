// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"

#include <tuple>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile_testing_helper.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/strcat.h"
#include "extensions/common/constants.h"
#endif

namespace enterprise_connectors {

namespace {

constexpr char kEmptySettingsPref[] = "[]";

constexpr char kNormalCloudAnalysisSettingsPref[] = R"([
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

constexpr char kNormalLocalAnalysisSettingsPref[] = R"([
  {
    "service_provider": "local_user_agent",
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

constexpr char kWildcardAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ]
  }
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
constexpr char kCustomMessage[] = "Custom Admin Message";
constexpr char kCustomUrl[] = "https://learn.more.com";
constexpr char kDlpTag[] = "dlp";

std::string CreateCustomUIPref(const char* custom_message,
                               const char* custom_url,
                               bool bypass_enabled) {
  std::string custom_messages_section;

  if (custom_message || custom_url) {
    std::string message_section =
        custom_message
            ? base::StringPrintf(R"("message": "%s" ,)", custom_message)
            : "";
    std::string learn_more_url_section =
        custom_url
            ? base::StringPrintf(R"("learn_more_url": "%s" ,)", custom_url)
            : "";

    custom_messages_section = base::StringPrintf(
        R"( "custom_messages": [
          { "language": "default",
            %s
            %s
            "tag": "dlp"
          } ] ,)",
        message_section.c_str(), learn_more_url_section.c_str());
  }

  std::string bypass_enabled_section;
  if (bypass_enabled) {
    bypass_enabled_section = R"("require_justification_tags": [ "dlp"],)";
  }

  std::string pref = base::StringPrintf(
      R"({  "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
            %s
            %s
            "service_provider": "google"
          })",
      custom_messages_section.c_str(), bypass_enabled_section.c_str());
  return pref;
}
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
  raw_ptr<TestingProfile> profile_;
};

class ConnectorsServiceAnalysisNoFeatureTest
    : public ConnectorsServiceTest,
      public testing::WithParamInterface<
          std::tuple<const char*, AnalysisConnector>> {
 public:
  ConnectorsServiceAnalysisNoFeatureTest() {
    scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
  }

  std::string pref_value() { return std::get<0>(GetParam()); }
  AnalysisConnector connector() { return std::get<1>(GetParam()); }
};

TEST_P(ConnectorsServiceAnalysisNoFeatureTest, AnalysisConnectors) {
  profile_->GetPrefs()->Set(ConnectorPref(connector()),
                            *base::JSONReader::Read(pref_value()));
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);
  for (const char* url :
       {kDlpAndMalwareUrl, kOnlyDlpUrl, kOnlyMalwareUrl, kNoTagsUrl}) {
    // Only absl::nullopt should be returned when the feature is disabled,
    // regardless of what Connector or URL is used.
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_FALSE(settings.has_value());
  }

  // No cached settings imply the connector value was never read.
  ASSERT_TRUE(service->ConnectorsManagerForTesting()
                  ->GetAnalysisConnectorsSettingsForTesting()
                  .empty());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceAnalysisNoFeatureTest,
    testing::Combine(testing::Values(kNormalCloudAnalysisSettingsPref,
                                     kNormalLocalAnalysisSettingsPref),
                     testing::Values(FILE_ATTACHED,
                                     FILE_DOWNLOADED,
                                     BULK_DATA_ENTRY,
                                     PRINT)));

#if BUILDFLAG(IS_CHROMEOS_ASH)

constexpr char kNormalSourceDestinationCloudAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {
        "source_destination_list": [
          {
            "sources": [
              {"file_system_type": "ANY"}
            ],
            "destinations": [
              {"file_system_type": "ANY"}
            ]
          }
        ],
        "tags": ["dlp", "malware"]
      }
    ],
    "block_until_verdict": 1,
    "block_password_protected": true,
    "block_large_files": true,
    "block_unsupported_file_types": true
  }
])";

constexpr char kNormalSourceDestinationLocalAnalysisSettingsPref[] = R"([
  {
    "service_provider": "local_user_agent",
    "enable": [
      {
        "source_destination_list": [
          {
            "sources": [
              {"file_system_type": "ANY"}
            ],
            "destinations": [
              {"file_system_type": "ANY"}
            ]
          }
        ],
        "tags": ["dlp", "malware"]
      }
    ],
    "block_until_verdict": 1,
    "block_password_protected": true,
    "block_large_files": true,
    "block_unsupported_file_types": true
  }
])";

using ConnectorsServiceAnalysisSourceDestinationNoFeatureTest =
    ConnectorsServiceAnalysisNoFeatureTest;
TEST_P(ConnectorsServiceAnalysisSourceDestinationNoFeatureTest,
       AnalysisConnectors) {
  profile_->GetPrefs()->Set(ConnectorPref(connector()),
                            *base::JSONReader::Read(pref_value()));
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);

  // Only absl::nullopt should be returned when the feature is disabled.
  storage::FileSystemURL source;
  storage::FileSystemURL destination;
  auto settings =
      service->GetAnalysisSettings(source, destination, connector());
  ASSERT_FALSE(settings.has_value());

  // No cached settings imply the connector value was never read.
  ASSERT_TRUE(service->ConnectorsManagerForTesting()
                  ->GetAnalysisConnectorsSettingsForTesting()
                  .empty());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceAnalysisSourceDestinationNoFeatureTest,
    testing::Combine(
        testing::Values(kNormalSourceDestinationCloudAnalysisSettingsPref,
                        kNormalSourceDestinationLocalAnalysisSettingsPref),
        testing::Values(FILE_TRANSFER)));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Test to make sure that HasExtraUiToDisplay returns the right value to
// show the extra UI from opt in features like custom message, URL and bypass
// on Download.
class ConnectorsServiceHasExtraUiTest
    : public ConnectorsServiceTest,
      public testing::WithParamInterface<std::tuple<std::string, bool>> {
 public:
  std::string pref() { return std::get<0>(GetParam()); }
  bool has_extra_ui() { return std::get<1>(GetParam()); }
};

TEST_P(ConnectorsServiceHasExtraUiTest, AnalysisConnectors) {
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_DOWNLOADED,
                                      pref());
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);
  bool show_extra_ui = service->HasExtraUiToDisplay(FILE_DOWNLOADED, kDlpTag);
  ASSERT_EQ(show_extra_ui, has_extra_ui());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceHasExtraUiTest,
    testing::Values(
        std::make_tuple(CreateCustomUIPref(kCustomMessage, kCustomUrl, true),
                        true),
        std::make_tuple(CreateCustomUIPref(kCustomMessage, kCustomUrl, false),
                        true),
        std::make_tuple(CreateCustomUIPref(kCustomMessage, nullptr, true),
                        true),
        std::make_tuple(CreateCustomUIPref(kCustomMessage, nullptr, false),
                        true),
        std::make_tuple(CreateCustomUIPref(nullptr, kCustomUrl, true), true),
        std::make_tuple(CreateCustomUIPref(nullptr, kCustomUrl, false), true),
        std::make_tuple(CreateCustomUIPref(nullptr, nullptr, true), true),
        std::make_tuple(CreateCustomUIPref(nullptr, nullptr, false), false)));

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

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceReportingFeatureTest,
    testing::Combine(testing::Values(ReportingConnector::SECURITY_EVENT),
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

class ConnectorsServiceExemptURLsTest
    : public ConnectorsServiceTest,
      public testing::WithParamInterface<AnalysisConnector> {
 public:
  ConnectorsServiceExemptURLsTest() = default;

  void SetUp() override {
    profile_->GetPrefs()->Set(
        ConnectorPref(connector()),
        *base::JSONReader::Read(kWildcardAnalysisSettingsPref));
    profile_->GetPrefs()->SetInteger(ConnectorScopePref(connector()),
                                     policy::POLICY_SCOPE_MACHINE);
  }

  AnalysisConnector connector() { return GetParam(); }
};

TEST_P(ConnectorsServiceExemptURLsTest, WebUI) {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);
  for (const char* url :
       {"chrome://settings", "chrome://help-app/background",
        "chrome://foo/bar/baz.html", "chrome://foo/bar/baz.html?param=value"}) {
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_FALSE(settings.has_value());
  }
}

TEST_P(ConnectorsServiceExemptURLsTest, ThirdPartyExtensions) {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);

  for (const char* url :
       {"chrome-extension://fake_id", "chrome-extension://fake_id/background",
        "chrome-extension://fake_id/main.html",
        "chrome-extension://fake_id/main.html?param=value"}) {
    ASSERT_TRUE(GURL(url).is_valid());
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_TRUE(settings.has_value());
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_P(ConnectorsServiceExemptURLsTest, FirstPartyExtensions) {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);

  for (const std::string& suffix :
       {"/", "/background", "/main.html", "/main.html?param=value"}) {
    std::string url = base::StrCat(
        {"chrome-extension://", extension_misc::kFilesManagerAppId, suffix});
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_FALSE(settings.has_value());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

INSTANTIATE_TEST_SUITE_P(,
                         ConnectorsServiceExemptURLsTest,
                         testing::Values(FILE_ATTACHED,
                                         FILE_DOWNLOADED,
                                         BULK_DATA_ENTRY));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class ConnectorsServiceProfileTypeBrowserTest : public testing::Test {
 public:
  ConnectorsServiceProfileTypeBrowserTest() {
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
  }

 protected:
  TestingProfile* regular_profile() {
    return profile_testing_helper_.regular_profile();
  }
  Profile* incognito_profile() {
    return profile_testing_helper_.incognito_profile();
  }

  TestingProfile* guest_profile() {
    return profile_testing_helper_.guest_profile();
  }
  Profile* guest_profile_otr() {
    return profile_testing_helper_.guest_profile_otr();
  }

  TestingProfile* system_profile() {
    return profile_testing_helper_.system_profile();
  }
  Profile* system_profile_otr() {
    return profile_testing_helper_.system_profile_otr();
  }

  std::unique_ptr<ConnectorsService> CreateService(Profile* profile) {
    auto manager = std::make_unique<ConnectorsManager>(
        std::make_unique<BrowserCrashEventRouter>(profile),
        std::make_unique<ExtensionInstallEventRouter>(profile),
        profile->GetPrefs(), GetServiceProviderConfig(), false);

    return std::make_unique<ConnectorsService>(profile, std::move(manager));
  }

 private:
  void SetUp() override {
    testing::Test::SetUp();
    profile_testing_helper_.SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  ProfileTestingHelper profile_testing_helper_;
};

TEST_F(ConnectorsServiceProfileTypeBrowserTest, IsEnabled) {
  EXPECT_TRUE(CreateService(regular_profile())->ConnectorsEnabled());
  EXPECT_FALSE(CreateService(incognito_profile())->ConnectorsEnabled());

  EXPECT_FALSE(CreateService(guest_profile())->ConnectorsEnabled());
  EXPECT_TRUE(CreateService(guest_profile_otr())->ConnectorsEnabled());

  EXPECT_FALSE(CreateService(system_profile())->ConnectorsEnabled());
  EXPECT_FALSE(CreateService(system_profile_otr())->ConnectorsEnabled());
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace enterprise_connectors
