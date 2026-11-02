// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"

#include <tuple>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_testing_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/policy/core/common/policy_types.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/strings/strcat.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "chromeos/components/mgs/managed_guest_session_test_utils.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#endif

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
constexpr char kEmptySettingsPref[] = "[]";

constexpr char kNormalReportingSettingsPref[] = R"([
  {
    "service_provider": "google"
  }
])";
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
constexpr char kWildcardAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ]
  }
])";

constexpr char kCustomMessage[] = "Custom Admin Message";
constexpr char kCustomUrl[] = "https://learn.more.com";
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

constexpr char kFakeDmToken[] = "fake-token";
constexpr char kFakeDeviceId[] = "fake-device-id";
#if BUILDFLAG(ENTERPRISE_WATERMARK)
constexpr char kAffiliationId1[] = "affiliation-id-1";
constexpr char kAffiliationId2[] = "affiliation-id-2";
#endif

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
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
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace

class ConnectorsServiceTestBase : public testing::Test {
 public:
  explicit ConnectorsServiceTestBase(const std::string& profile_name)
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_name_(profile_name) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(profile_name_);
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken(kFakeDmToken));
  }

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    auto owner_key_util = base::MakeRefCounted<ownership::MockOwnerKeyUtil>();
    owner_key_util->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());

    if (!ash::DeviceSettingsService::IsInitialized()) {
      ash::DeviceSettingsService::Initialize();
      initialized_device_settings_ = true;
    }

    ash::DeviceSettingsService::Get()->StartProcessing(
        TestingBrowserProcess::GetGlobal()->local_state(),
        &session_manager_client_, owner_key_util);

    device_policy_.policy_data().set_device_id(kFakeDeviceId);
    device_policy_.Build();
    session_manager_client_.set_device_policy(device_policy_.GetBlob());

    ash::DeviceSettingsService::Get()->Load();
    content::RunAllTasksUntilIdle();
#else
    fake_browser_dm_token_storage_.SetClientId(kFakeDeviceId);
    policy::BrowserDMTokenStorage::SetForTesting(
        &fake_browser_dm_token_storage_);
    fake_browser_dm_token_storage_.ResetForTesting();
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS)
    ash::DeviceSettingsService::Get()->StopProcessing();
    if (initialized_device_settings_) {
      ash::DeviceSettingsService::Shutdown();
    }
#endif
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
#if !BUILDFLAG(IS_CHROMEOS)
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage_;
#else
  ash::FakeSessionManagerClient session_manager_client_;
  policy::DevicePolicyBuilder device_policy_;
  bool initialized_device_settings_ = false;
#endif
  std::string profile_name_;
};

class ConnectorsServiceTest : public ConnectorsServiceTestBase {
 public:
  ConnectorsServiceTest() : ConnectorsServiceTestBase("test-user") {}
};

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
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
  test::SetAnalysisConnector(profile_->GetPrefs(), FILE_DOWNLOADED, pref());
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
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

// Tests to make sure getting reporting settings work with both the feature flag
// and the OnSecurityEventEnterpriseConnector policy. The parameter for these
// tests is a tuple of:
//
//   enum class ReportingConnector[]: array of all reporting connectors.
//   bool: enable feature flag.
//   int: policy value.  0: don't set, 1: set to normal, 2: set to empty.
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
class ConnectorsServiceReportingFeatureTest
    : public ConnectorsServiceTest,
      public testing::WithParamInterface<const char*> {
 public:
  const char* pref_value() const { return GetParam(); }

  const char* pref() const { return kOnSecurityEventPref; }

  const char* scope_pref() const { return kOnSecurityEventScopePref; }

  PrefService* pref_service() const { return profile_->GetPrefs(); }

  bool reporting_enabled() const {
    return pref_value() == kNormalReportingSettingsPref;
  }
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(ConnectorsServiceReportingFeatureTest,
       ChromeOsManagedGuestSessionFlagSetInMgs) {
  // A fake Managed Guest Session that gets destroyed at the end of the test.
  chromeos::FakeManagedGuestSession fake_mgs;

  if (pref_value()) {
    profile_->GetPrefs()->Set(
        pref(), *base::JSONReader::Read(pref_value(),
                                        base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    profile_->GetPrefs()->SetInteger(scope_pref(),
                                     policy::POLICY_SCOPE_MACHINE);
  }

  EXPECT_TRUE(ConnectorsServiceFactory::GetForBrowserContext(profile_)
                  ->BuildClientMetadata(/*is_cloud=*/true)
                  ->is_chrome_os_managed_guest_session());

  // The flag is currently not included for local content scanning.
  EXPECT_FALSE(ConnectorsServiceFactory::GetForBrowserContext(profile_)
                   ->BuildClientMetadata(/*is_cloud=*/false)
                   ->is_chrome_os_managed_guest_session());
}

TEST_P(ConnectorsServiceReportingFeatureTest,
       ChromeOsManagedGuestSessionFlagNotSetInUserSession) {
  if (pref_value()) {
    profile_->GetPrefs()->Set(
        pref(), *base::JSONReader::Read(pref_value(),
                                        base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    profile_->GetPrefs()->SetInteger(scope_pref(),
                                     policy::POLICY_SCOPE_MACHINE);
  }

  EXPECT_FALSE(ConnectorsServiceFactory::GetForBrowserContext(profile_)
                   ->BuildClientMetadata(/*is_cloud=*/true)
                   ->is_chrome_os_managed_guest_session());

  EXPECT_FALSE(ConnectorsServiceFactory::GetForBrowserContext(profile_)
                   ->BuildClientMetadata(/*is_cloud=*/false)
                   ->is_chrome_os_managed_guest_session());
}
#endif

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
TEST_P(ConnectorsServiceReportingFeatureTest, CheckTelemetryPolicyObserver) {
  ConnectorsService* connectors_service =
      ConnectorsServiceFactory::GetForBrowserContext(profile_);
  ConnectorsManagerBase* connectors_manager_base =
      connectors_service->ConnectorsManagerBaseForTesting();

  base::test::TestFuture<void> future;
  connectors_service->ObserveTelemetryReporting(future.GetRepeatingCallback());

  ASSERT_FALSE(connectors_manager_base->GetTelemetryObserverCallbackForTesting()
                   .is_null());
  // Cache initially empty
  ASSERT_TRUE(
      connectors_manager_base->GetReportingConnectorsSettingsForTesting()
          .empty());

  // Enable browser crash event
  test::SetOnSecurityEventReporting(pref_service(), true, {kBrowserCrashEvent},
                                    {});
  EXPECT_TRUE(future.WaitAndClear());

  // Clear enabled events (not cached when cleared)
  test::SetOnSecurityEventReporting(pref_service(), false, {}, {});
  ASSERT_TRUE(
      connectors_manager_base->GetReportingConnectorsSettingsForTesting()
          .empty());
  EXPECT_TRUE(future.WaitAndClear());

  // Enable telemetry event
  test::SetOnSecurityEventReporting(pref_service(), true,
                                    {kExtensionTelemetryEvent}, {});
  EXPECT_TRUE(future.WaitAndClear());
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

INSTANTIATE_TEST_SUITE_P(,
                         ConnectorsServiceReportingFeatureTest,
                         testing::Values(nullptr,
                                         kNormalReportingSettingsPref,
                                         kEmptySettingsPref));

#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

TEST_F(ConnectorsServiceTest, RealtimeURLCheck) {
  profile_->GetPrefs()->SetInteger(
      kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile_->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                   policy::POLICY_SCOPE_MACHINE);

  auto maybe_dm_token = ConnectorsServiceFactory::GetForBrowserContext(profile_)
                            ->GetDMTokenForRealTimeUrlCheck();
  EXPECT_TRUE(maybe_dm_token.has_value());
  EXPECT_EQ(kFakeDmToken, maybe_dm_token.value());

  std::string identifier =
      ConnectorsServiceFactory::GetForBrowserContext(profile_)
          ->GetRealTimeUrlCheckIdentifier();
  EXPECT_EQ(identifier, kFakeDeviceId);

  policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());

  maybe_dm_token = ConnectorsServiceFactory::GetForBrowserContext(profile_)
                       ->GetDMTokenForRealTimeUrlCheck();
  ASSERT_EQ(
      maybe_dm_token.error(),
      ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::kNoDmToken);
  EXPECT_FALSE(maybe_dm_token.has_value());

  identifier = ConnectorsServiceFactory::GetForBrowserContext(profile_)
                   ->GetRealTimeUrlCheckIdentifier();
  EXPECT_TRUE(identifier.empty());
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
class ConnectorsServiceExemptURLsTest
    : public ConnectorsServiceTest,
      public testing::WithParamInterface<AnalysisConnector> {
 public:
  ConnectorsServiceExemptURLsTest() = default;

  void SetUp() override {
    ConnectorsServiceTest::SetUp();
    profile_->GetPrefs()->Set(
        AnalysisConnectorPref(connector()),
        *base::JSONReader::Read(kWildcardAnalysisSettingsPref,
                                base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    profile_->GetPrefs()->SetInteger(AnalysisConnectorScopePref(connector()),
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

TEST_P(ConnectorsServiceExemptURLsTest, DevTools) {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);

  for (const char* url :
       {"devtools://fake_id", "devtools://fake_id/background",
        "devtools://devtools/main.html",
        "devtools://devtools/bundled/main.html?param=value"}) {
    ASSERT_TRUE(GURL(url).is_valid());
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_NE(settings.has_value(),
              connector() == AnalysisConnector::BULK_DATA_ENTRY ||
                  connector() == AnalysisConnector::FILE_ATTACHED);
  }
}

TEST_P(ConnectorsServiceExemptURLsTest, BlobAndFilesystem) {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile_);

  // Test against wildcard policy.
  for (const char* url_string :
       {"blob:https://foo.com", "blob:ftp://foo.com/with/path",
        "filesystem:http://foo.com/with.extension",
        "filesystem:http://foo.com/with/path"}) {
    GURL url(url_string);
    ASSERT_TRUE(url.is_valid());
    ASSERT_TRUE(url.SchemeIsFileSystem() || url.SchemeIsBlob());
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_TRUE(settings.has_value());
  }

  // Test against a specific pattern policy to validate the correct inner URL is
  // used.
  profile_->GetPrefs()->Set(
      AnalysisConnectorPref(connector()),
      *base::JSONReader::Read(R"([
        {
          "service_provider": "google",
          "enable": [
            {"url_list": ["foo.com"], "tags": ["dlp", "malware"]}
          ]
        }
      ])",
                              base::JSON_PARSE_CHROMIUM_EXTENSIONS));

  for (const char* url_string :
       {"blob:https://foo.com", "blob:ftp://foo.com/with/path",
        "filesystem:http://foo.com/with.extension",
        "filesystem:http://foo.com/with/path"}) {
    GURL url(url_string);
    ASSERT_TRUE(url.is_valid());
    ASSERT_TRUE(url.SchemeIsFileSystem() || url.SchemeIsBlob());
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_TRUE(settings.has_value());
  }
  for (const char* url_string :
       {"blob:https://notfoo.com", "blob:ftp://notfoo.com/with/path",
        "filesystem:http://notfoo.com/with.extension",
        "filesystem:http://notfoo.com/with/path"}) {
    GURL url(url_string);
    ASSERT_TRUE(url.is_valid());
    ASSERT_TRUE(url.SchemeIsFileSystem() || url.SchemeIsBlob());
    auto settings = service->GetAnalysisSettings(GURL(url), connector());
    ASSERT_FALSE(settings.has_value());
  }
}

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(
    ,
    ConnectorsServiceExemptURLsTest,
    testing::Values(FILE_ATTACHED, FILE_DOWNLOADED, BULK_DATA_ENTRY, PRINT));
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

class ConnectorsServiceProfileTypeBrowserTest : public testing::Test {
 public:
 protected:
  TestingProfile* regular_profile() {
    return profile_testing_helper_.regular_profile();
  }
  Profile* incognito_profile() {
    return profile_testing_helper_.incognito_profile();
  }

  TestingProfile* isolated_mode_parent_profile() {
    return profile_testing_helper_.isolated_mode_parent_profile();
  }
  Profile* isolated_mode_profile() {
    return profile_testing_helper_.isolated_mode_profile();
  }

  TestingProfile* guest_profile() {
    return profile_testing_helper_.guest_profile();
  }
  Profile* guest_profile_otr() {
    return profile_testing_helper_.guest_profile_otr();
  }

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  TestingProfile* system_profile() {
    return profile_testing_helper_.system_profile();
  }
  Profile* system_profile_otr() {
    return profile_testing_helper_.system_profile_otr();
  }
#elif BUILDFLAG(IS_CHROMEOS)
  TestingProfile* signin_profile() {
    return profile_testing_helper_.signin_profile();
  }
  Profile* signin_profile_otr() {
    return profile_testing_helper_.signin_profile_otr();
  }

  TestingProfile* lockscreen_profile() {
    return profile_testing_helper_.lockscreen_profile();
  }
  Profile* lockscreen_profile_otr() {
    return profile_testing_helper_.lockscreen_profile_otr();
  }
#endif

  std::unique_ptr<ConnectorsService> CreateService(Profile* profile) {
    auto manager = std::make_unique<ConnectorsManager>(
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
  EXPECT_TRUE(CreateService(isolated_mode_profile())->ConnectorsEnabled());

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(CreateService(guest_profile())->ConnectorsEnabled());
#else
  EXPECT_FALSE(CreateService(guest_profile())->ConnectorsEnabled());
#endif
  EXPECT_TRUE(CreateService(guest_profile_otr())->ConnectorsEnabled());

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(CreateService(system_profile())->ConnectorsEnabled());
  EXPECT_FALSE(CreateService(system_profile_otr())->ConnectorsEnabled());
#elif BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(CreateService(signin_profile())->ConnectorsEnabled());
  EXPECT_FALSE(CreateService(signin_profile_otr())->ConnectorsEnabled());

  EXPECT_TRUE(CreateService(lockscreen_profile())->ConnectorsEnabled());
  EXPECT_FALSE(CreateService(lockscreen_profile_otr())->ConnectorsEnabled());
#endif
}

#if BUILDFLAG(ENTERPRISE_WATERMARK)

struct IdentifierTestParams {
  std::string profile_affiliation_id;
  std::string device_affiliation_id;
  policy::PolicyScope policy_scope;

  // DM Token is not known at the point of defining the test cases.
  bool has_device_token;
  bool has_profile_email;
} kIdentifierTestCases[] = {
    // Affiliated cases
    {.profile_affiliation_id = kAffiliationId1,
     .device_affiliation_id = kAffiliationId1,
     .policy_scope = policy::POLICY_SCOPE_MACHINE,
     .has_device_token = true,
     .has_profile_email = true},
    // Unaffiliated cases
    {.profile_affiliation_id = kAffiliationId1,
     .device_affiliation_id = kAffiliationId2,
     .policy_scope = policy::POLICY_SCOPE_MACHINE,
     .has_device_token = true,
     .has_profile_email = false}};

class ConnectorsServiceRealTimeURLIdentifierTest
    : public ConnectorsServiceTestBase,
      public testing::WithParamInterface<IdentifierTestParams> {
 public:
  ConnectorsServiceRealTimeURLIdentifierTest()
      : ConnectorsServiceTestBase("user@example.com") {
#if BUILDFLAG(IS_CHROMEOS)
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<
        user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>>(
        std::move(fake_user_manager));
#endif
  }

  void SetUp() override {
    ConnectorsServiceTestBase::SetUp();

#if BUILDFLAG(IS_CHROMEOS)
    const AccountId account_id = AccountId::FromUserEmail("user@example.com");
    bool is_affiliated =
        GetParam().profile_affiliation_id == GetParam().device_affiliation_id;
    scoped_user_manager_->Get()->AddUserWithAffiliationAndTypeAndProfile(
        account_id, is_affiliated, user_manager::UserType::kRegular, profile_);
    scoped_user_manager_->Get()->LoginUser(account_id);
#endif
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<
      user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>>
      scoped_user_manager_;
#endif
};

TEST_P(ConnectorsServiceRealTimeURLIdentifierTest, ReturnsCorrectIdentifier) {
#if !BUILDFLAG(IS_CHROMEOS)
  // Set Affiliation IDs, matching affiliation ids => affiliated profile
  profile_->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
      {GetParam().profile_affiliation_id});
  TestingBrowserProcess::GetGlobal()
      ->browser_policy_connector()
      ->SetDeviceAffiliatedIdsForTesting({GetParam().device_affiliation_id});
#endif

  // Set the profile email.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                      signin::ConsentLevel::kSignin);

  profile_->GetPrefs()->SetInteger(
      kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile_->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                   GetParam().policy_scope);

  std::string identifier =
      ConnectorsServiceFactory::GetForBrowserContext(profile_)
          ->GetRealTimeUrlCheckIdentifier();

  // Check identifier
  EXPECT_EQ(GetParam().has_device_token,
            identifier.find(kFakeDeviceId) != std::string::npos);
  EXPECT_EQ(GetParam().has_profile_email,
            identifier.find("user@example.com") != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(,
                         ConnectorsServiceRealTimeURLIdentifierTest,
                         testing::ValuesIn(kIdentifierTestCases));

#endif  // BUILDFLAG(ENTERPRISE_WATERMARK)

}  // namespace enterprise_connectors
