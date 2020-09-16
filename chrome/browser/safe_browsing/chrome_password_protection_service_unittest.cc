// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/content/password_protection/password_protection_request.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// All tests related to extension is disabled on Android, because enterprise
// reporting extension is not supported.
#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "extensions/browser/test_event_router.h"
#endif

using sync_pb::UserEventSpecifics;
using GaiaPasswordReuse = sync_pb::GaiaPasswordReuse;
using GaiaPasswordCaptured = UserEventSpecifics::GaiaPasswordCaptured;
using PasswordReuseDialogInteraction =
    GaiaPasswordReuse::PasswordReuseDialogInteraction;
using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;
using PasswordReuseLookup = GaiaPasswordReuse::PasswordReuseLookup;
using ::testing::_;
using ::testing::Return;
using ::testing::WithArg;

#if !defined(OS_ANDROID)
namespace OnPolicySpecifiedPasswordReuseDetected = extensions::api::
    safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected;
namespace OnPolicySpecifiedPasswordChanged =
    extensions::api::safe_browsing_private::OnPolicySpecifiedPasswordChanged;
#endif

class MockSecurityEventRecorder : public SecurityEventRecorder {
 public:
  MockSecurityEventRecorder() = default;
  ~MockSecurityEventRecorder() override = default;

  static BrowserContextKeyedServiceFactory::TestingFactory Create() {
    return base::BindRepeating(
        [](content::BrowserContext* context) -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockSecurityEventRecorder>();
        });
  }

  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<syncer::ModelTypeControllerDelegate>());
  MOCK_METHOD1(RecordGaiaPasswordReuse, void(const GaiaPasswordReuse&));
};

namespace safe_browsing {

namespace {

const char kPhishingURL[] = "http://phishing.com/";
const char kTestEmail[] = "foo@example.com";
const char kUserName[] = "username";
const char kRedirectURL[] = "http://redirect.com";
#if !defined(OS_ANDROID)
const char kPasswordReuseURL[] = "http://login.example.com/";
const char kTestGmail[] = "foo@gmail.com";
#endif

BrowserContextKeyedServiceFactory::TestingFactory
GetFakeUserEventServiceFactory() {
  return base::BindRepeating(
      [](content::BrowserContext* context) -> std::unique_ptr<KeyedService> {
        return std::make_unique<syncer::FakeUserEventService>();
      });
}

constexpr struct {
  // The response from the password protection service.
  RequestOutcome request_outcome;
  // The enum to log in the user event for that response.
  PasswordReuseLookup::LookupResult lookup_result;
} kTestCasesWithoutVerdict[]{
    {RequestOutcome::MATCHED_WHITELIST, PasswordReuseLookup::WHITELIST_HIT},
    {RequestOutcome::URL_NOT_VALID_FOR_REPUTATION_COMPUTING,
     PasswordReuseLookup::URL_UNSUPPORTED},
    {RequestOutcome::CANCELED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::TIMEDOUT, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::DISABLED_DUE_TO_INCOGNITO,
     PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::REQUEST_MALFORMED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::FETCH_FAILED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::RESPONSE_MALFORMED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::SERVICE_DESTROYED, PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::DISABLED_DUE_TO_FEATURE_DISABLED,
     PasswordReuseLookup::REQUEST_FAILURE},
    {RequestOutcome::DISABLED_DUE_TO_USER_POPULATION,
     PasswordReuseLookup::REQUEST_FAILURE}};

}  // namespace

class MockChromePasswordProtectionService
    : public ChromePasswordProtectionService {
 public:
  explicit MockChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      StringProvider sync_password_hash_provider,
      VerdictCacheManager* cache_manager)
      : ChromePasswordProtectionService(profile,
                                        ui_manager,
                                        sync_password_hash_provider,
                                        cache_manager),
        is_incognito_(false),
        is_extended_reporting_(false),
        is_syncing_(false),
        is_no_hosted_domain_found_(false),
        is_account_signed_in_(false) {}
  bool IsExtendedReporting() override { return is_extended_reporting_; }
  bool IsIncognito() override { return is_incognito_; }
  bool IsPrimaryAccountSyncing() const override { return is_syncing_; }
  bool IsPrimaryAccountSignedIn() const override {
    return is_account_signed_in_;
  }
  bool IsPrimaryAccountGmail() const override {
    return is_no_hosted_domain_found_;
  }
  AccountInfo GetSignedInNonSyncAccount(
      const std::string& username) const override {
    return account_info_;
  }

  safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment
  GetUrlDisplayExperiment() const override {
    return safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment();
  }

  // Configures the results returned by IsExtendedReporting(), IsIncognito(),
  // and IsHistorySyncEnabled().
  void ConfigService(bool is_incognito, bool is_extended_reporting) {
    is_incognito_ = is_incognito;
    is_extended_reporting_ = is_extended_reporting;
  }

  void SetIsSyncing(bool is_syncing) { is_syncing_ = is_syncing; }
  void SetIsNoHostedDomainFound(bool is_no_hosted_domain_found) {
    is_no_hosted_domain_found_ = is_no_hosted_domain_found;
  }
  void SetIsAccountSignedIn(bool is_account_signed_in) {
    is_account_signed_in_ = is_account_signed_in;
  }
  void SetAccountInfo(const std::string& username) {
    AccountInfo account_info;
    account_info.account_id = CoreAccountId("account_id");
    account_info.email = username;
    account_info.gaia = "gaia";
    account_info_ = account_info;
  }

  SafeBrowsingUIManager* ui_manager() { return ui_manager_.get(); }

 protected:
  friend class ChromePasswordProtectionServiceTest;

 private:
  bool is_incognito_;
  bool is_extended_reporting_;
  bool is_syncing_;
  bool is_no_hosted_domain_found_;
  bool is_account_signed_in_;
  AccountInfo account_info_;
  std::string mocked_sync_password_hash_;
};

class ChromePasswordProtectionServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordProtectionServiceTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    password_store_ =
        base::WrapRefCounted(static_cast<password_manager::MockPasswordStore*>(
            PasswordStoreFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating(&password_manager::BuildPasswordStore<
                                        content::BrowserContext,
                                        password_manager::MockPasswordStore>))
                .get()));

    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      account_password_store_ = base::WrapRefCounted(
          static_cast<password_manager::MockPasswordStore*>(
              AccountPasswordStoreFactory::GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(),
                      base::BindRepeating(&password_manager::BuildPasswordStore<
                                          content::BrowserContext,
                                          password_manager::MockPasswordStore>))
                  .get()));
    }

    profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
    profile()->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger,
        PasswordProtectionTrigger::PHISHING_REUSE);
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, /*is_off_the_record=*/false,
        /*store_last_modified=*/false, /*restore_session=*/false);

    cache_manager_ = std::make_unique<VerdictCacheManager>(
        nullptr, content_setting_map_.get());

    service_ = NewMockPasswordProtectionService();
    fake_user_event_service_ = static_cast<syncer::FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(browser_context(),
                                      GetFakeUserEventServiceFactory()));
#if !defined(OS_ANDROID)
    test_event_router_ =
        extensions::CreateAndUseTestEventRouter(browser_context());
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            browser_context(),
            base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter));
#endif

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    security_event_recorder_ = static_cast<MockSecurityEventRecorder*>(
        SecurityEventRecorderFactory::GetInstance()->SetTestingFactoryAndUse(
            browser_context(), MockSecurityEventRecorder::Create()));
    // To make sure we are not accidentally calling the SecurityEventRecorder.
    EXPECT_CALL(*security_event_recorder_, RecordGaiaPasswordReuse(_)).Times(0);
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    service_.reset();
    request_ = nullptr;
    if (account_password_store_)
      account_password_store_->ShutdownOnUIThread();
    password_store_->ShutdownOnUIThread();
    identity_test_env_profile_adaptor_.reset();
    cache_manager_.reset();
    content_setting_map_->ShutdownOnUIThread();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // This can be called whenever to reset service_, if initialition
  // conditions have changed.
  std::unique_ptr<MockChromePasswordProtectionService>
  NewMockPasswordProtectionService(
      const std::string& sync_password_hash = std::string()) {
    StringProvider sync_password_hash_provider =
        base::BindLambdaForTesting([=] { return sync_password_hash; });

    // TODO(crbug/925153): Port consumers of the SafeBrowsingService
    // to use the interface in components/safe_browsing, and remove this
    // cast.
    return std::make_unique<MockChromePasswordProtectionService>(
        profile(),
        new SafeBrowsingUIManager(
            static_cast<safe_browsing::SafeBrowsingService*>(
                SafeBrowsingService::CreateSafeBrowsingService())),
        sync_password_hash_provider, cache_manager_.get());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  syncer::FakeUserEventService* GetUserEventService() {
    return fake_user_event_service_;
  }

  void InitializeRequest(LoginReputationClientRequest::TriggerType trigger_type,
                         PasswordType reused_password_type) {
    std::vector<password_manager::MatchingReusedCredential> credentials = {
        {"somedomain.com"}};
    if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
      request_ = new PasswordProtectionRequest(
          web_contents(), GURL(kPhishingURL), GURL(), GURL(), kUserName,
          PasswordType::PASSWORD_TYPE_UNKNOWN, credentials, trigger_type, true,
          service_.get(), 0);
    } else {
      ASSERT_EQ(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                trigger_type);
      request_ = new PasswordProtectionRequest(
          web_contents(), GURL(kPhishingURL), GURL(), GURL(), kUserName,
          reused_password_type, credentials, trigger_type,
          /* password_field_exists*/ true, service_.get(),
          /*request_timeout_in_ms=*/0);
    }
  }

  void InitializeVerdict(LoginReputationClientResponse::VerdictType type) {
    verdict_ = std::make_unique<LoginReputationClientResponse>();
    verdict_->set_verdict_type(type);
  }

  void SimulateRequestFinished(
      LoginReputationClientResponse::VerdictType verdict_type) {
    std::unique_ptr<LoginReputationClientResponse> verdict =
        std::make_unique<LoginReputationClientResponse>();
    verdict->set_verdict_type(verdict_type);
    service_->RequestFinished(request_.get(), RequestOutcome::SUCCEEDED,
                              std::move(verdict));
  }

  CoreAccountInfo SetPrimaryAccount(const std::string& email) {
    identity_test_env()->MakeAccountAvailable(email);
    return identity_test_env()->SetPrimaryAccount(email);
  }

  void SetUpSyncAccount(const std::string& hosted_domain,
                        const CoreAccountInfo& account_info) {
    identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        hosted_domain, "full_name", "given_name", "locale",
        "http://picture.example.com/picture.jpg");
  }

  void PrepareRequest(LoginReputationClientRequest::TriggerType trigger_type,
                      PasswordType reused_password_type,
                      bool is_warning_showing) {
    InitializeRequest(trigger_type, reused_password_type);
    request_->set_is_modal_warning_showing(is_warning_showing);
    service_->pending_requests_.insert(request_);
  }

  int GetSizeofUnhandledSyncPasswordReuses() {
    DictionaryPrefUpdate unhandled_sync_password_reuses(
        profile()->GetPrefs(), prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
    return unhandled_sync_password_reuses->size();
  }

  size_t GetNumberOfNavigationThrottles() {
    return request_ ? request_->throttles_.size() : 0u;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  std::unique_ptr<MockChromePasswordProtectionService> service_;
  scoped_refptr<PasswordProtectionRequest> request_;
  std::unique_ptr<LoginReputationClientResponse> verdict_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  MockSecurityEventRecorder* security_event_recorder_;
  scoped_refptr<password_manager::MockPasswordStore> password_store_;
  scoped_refptr<password_manager::MockPasswordStore> account_password_store_;
  // Owned by KeyedServiceFactory.
  syncer::FakeUserEventService* fake_user_event_service_;
#if !defined(OS_ANDROID)
  extensions::TestEventRouter* test_event_router_;
#endif
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  ScopedTestingLocalState local_state_;
};

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForPasswordOnFocusPing) {
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);

  // Password field on focus pinging is enabled on !incognito && (SBER ||
  // enhanced protection).
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_type));

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_type));

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_type));

  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForSavedPasswordEntryPing) {
  base::test::ScopedFeatureList feature_list;

  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);

  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForSyncPasswordEntryPing) {
  // Sets up the account as a gmail account as there is no hosted domain.
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  reused_password_type.set_is_account_syncing(true);

  // Sync password entry pinging is enabled by default.
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
// Sync password pings are gated by SBER on Android, because warnings are
// disabled.
#if defined(OS_ANDROID)
  EXPECT_FALSE(service_->IsPingingEnabled(
#else
  EXPECT_TRUE(service_->IsPingingEnabled(
#endif
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
// Sync password pings are gated by SBER on Android, because warnings are
// disabled.
#if defined(OS_ANDROID)
  EXPECT_FALSE(service_->IsPingingEnabled(
#else
  EXPECT_TRUE(service_->IsPingingEnabled(
#endif
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  // Even if sync password entry pinging is disabled by policy,
  // |IsPingingEnabled(..)| should still default to true if the
  // the password reuse type is syncing Gmail account.
  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  service_->SetIsNoHostedDomainFound(true);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_PROTECTION_OFF);
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
// Sync password pings are gated by SBER on Android, because warnings are
// disabled.
#if defined(OS_ANDROID)
  EXPECT_FALSE(service_->IsPingingEnabled(
#else
  EXPECT_TRUE(service_->IsPingingEnabled(
#endif
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
// Sync password pings are gated by SBER on Android, because warnings are
// disabled.
#if defined(OS_ANDROID)
  EXPECT_FALSE(service_->IsPingingEnabled(
#else
  EXPECT_TRUE(service_->IsPingingEnabled(
#endif
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPingingIsSkippedIfMatchEnterpriseWhitelist) {
  ASSERT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kSafeBrowsingWhitelistDomains));

  // If there's no whitelist, IsURLWhitelistedForPasswordEntry(_) should
  // return false.
  EXPECT_FALSE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  // Verify if match enterprise whitelist.
  base::ListValue whitelist;
  whitelist.AppendString("mydomain.com");
  whitelist.AppendString("mydomain.net");
  profile()->GetPrefs()->Set(prefs::kSafeBrowsingWhitelistDomains, whitelist);
  EXPECT_TRUE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  // Verify if matches enterprise change password url.
  profile()->GetPrefs()->ClearPref(prefs::kSafeBrowsingWhitelistDomains);
  EXPECT_FALSE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  profile()->GetPrefs()->SetString(prefs::kPasswordProtectionChangePasswordURL,
                                   "https://mydomain.com/change_password.html");
  EXPECT_TRUE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://mydomain.com/change_password.html#ref?user_name=alice")));

  // Verify if matches enterprise login url.
  profile()->GetPrefs()->ClearPref(prefs::kSafeBrowsingWhitelistDomains);
  profile()->GetPrefs()->ClearPref(prefs::kPasswordProtectionChangePasswordURL);
  EXPECT_FALSE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://www.mydomain.com")));
  base::ListValue login_urls;
  login_urls.AppendString("https://mydomain.com/login.html");
  profile()->GetPrefs()->Set(prefs::kPasswordProtectionLoginURLs, login_urls);
  EXPECT_TRUE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://mydomain.com/login.html#ref?user_name=alice")));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPersistPhishedSavedPasswordCredential) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test"}, {"http://2.example.com"}};

  EXPECT_CALL(*password_store_, AddCompromisedCredentialsImpl(_)).Times(2);
  service_->PersistPhishedSavedPasswordCredential(credentials);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyRemovePhishedSavedPasswordCredential) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", base::ASCIIToUTF16("username1")},
      {"http://2.example.test", base::ASCIIToUTF16("username2")}};

  EXPECT_CALL(*password_store_,
              RemoveCompromisedCredentialsImpl(
                  _, _,
                  password_manager::RemoveCompromisedCredentialsReason::
                      kMarkSiteAsLegitimate))
      .Times(2);
  service_->RemovePhishedSavedPasswordCredential(credentials);
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyCanSendSamplePing) {
  // Experiment is on by default.
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  service_->set_bypass_probability_for_tests(true);
  EXPECT_TRUE(service_->CanSendSamplePing());

  // If not SBER, do not send sample ping.
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/false);
  EXPECT_FALSE(service_->CanSendSamplePing());

  // If incognito, do not send sample ping.
  service_->ConfigService(/*is_incognito=*/true,
                          /*is_extended_reporting=*/true);
  EXPECT_FALSE(service_->CanSendSamplePing());

  service_->ConfigService(/*is_incognito=*/true,
                          /*is_extended_reporting=*/false);
  EXPECT_FALSE(service_->CanSendSamplePing());

  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/false);
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetOrganizationTypeGmail) {
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  reused_password_type.set_is_account_syncing(true);
  EXPECT_TRUE(service_->GetOrganizationName(reused_password_type).empty());
  EXPECT_EQ("", service_->GetOrganizationName(reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetOrganizationTypeGSuite) {
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount("example.com", account_info);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  reused_password_type.set_is_account_syncing(true);
  EXPECT_EQ("example.com", service_->GetOrganizationName(reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyUpdateSecurityState) {
  GURL url("http://password_reuse_url.com");
  NavigateAndCommit(url);
  SBThreatType current_threat_type = SB_THREAT_TYPE_UNUSED;
  ASSERT_FALSE(service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_UNUSED, current_threat_type);

  // Cache a verdict for this URL.
  LoginReputationClientResponse verdict_proto;
  verdict_proto.set_verdict_type(LoginReputationClientResponse::PHISHING);
  verdict_proto.set_cache_duration_sec(600);
  verdict_proto.set_cache_expression("password_reuse_url.com/");
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  reused_password_type.set_is_account_syncing(true);
  service_->CacheVerdict(
      url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type, verdict_proto, base::Time::Now());

  service_->UpdateSecurityState(SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE,
                                reused_password_type, web_contents());
  ASSERT_TRUE(service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE, current_threat_type);

  service_->UpdateSecurityState(safe_browsing::SB_THREAT_TYPE_SAFE,
                                reused_password_type, web_contents());
  current_threat_type = SB_THREAT_TYPE_UNUSED;
  service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type);
  EXPECT_EQ(SB_THREAT_TYPE_UNUSED, current_threat_type);
  LoginReputationClientResponse verdict;
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            service_->GetCachedVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_type, &verdict));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseUserEventNotRecordedDueToIncognito) {
  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);
  service_->ConfigService(true /*is_incognito*/,
                          false /*is_extended_reporting*/);
  ASSERT_TRUE(service_->IsIncognito());

  // Nothing should be logged because of incognito.
  NavigateAndCommit(GURL("https:www.example.com/"));

  // PasswordReuseDetected
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
  service_->MaybeLogPasswordReuseLookupEvent(
      web_contents(), RequestOutcome::MATCHED_WHITELIST,
      PasswordType::PRIMARY_ACCOUNT_PASSWORD, nullptr);
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());

  // PasswordReuseLookup
  unsigned long t = 0;
  for (const auto& it : kTestCasesWithoutVerdict) {
    service_->MaybeLogPasswordReuseLookupEvent(
        web_contents(), it.request_outcome,
        PasswordType::PRIMARY_ACCOUNT_PASSWORD, nullptr);
    ASSERT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty()) << t;
    t++;
  }

  // PasswordReuseDialogInteraction
  service_->MaybeLogPasswordReuseDialogInteraction(
      1000 /* navigation_id */,
      PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN);
  ASSERT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseDetectedUserEventRecorded) {
  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);
  service_->SetIsAccountSignedIn(true);
  NavigateAndCommit(GURL("https://www.example.com/"));

  // Case 1: safe_browsing_enabled = true
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  ASSERT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());
  GaiaPasswordReuse event = GetUserEventService()
                                ->GetRecordedUserEvents()[0]
                                .gaia_password_reuse_event();
  EXPECT_TRUE(event.reuse_detected().status().enabled());

  // Case 2: safe_browsing_enabled = false
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  ASSERT_EQ(2ul, GetUserEventService()->GetRecordedUserEvents().size());
  event = GetUserEventService()
              ->GetRecordedUserEvents()[1]
              .gaia_password_reuse_event();
  EXPECT_FALSE(event.reuse_detected().status().enabled());

  // Not checking for the extended_reporting_level since that requires setting
  // multiple prefs and doesn't add much verification value.
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseDetectedSecurityEventRecorded) {
  identity_test_env()->SetPrimaryAccount(kTestEmail);
  service_->set_username_for_last_shown_warning(kTestEmail);
  EXPECT_CALL(*security_event_recorder_, RecordGaiaPasswordReuse(_))
      .WillOnce(WithArg<0>([&](const auto& message) {
        EXPECT_EQ(PasswordReuseLookup::REQUEST_SUCCESS,
                  message.reuse_lookup().lookup_result());
        EXPECT_EQ(PasswordReuseLookup::SAFE, message.reuse_lookup().verdict());
        EXPECT_EQ("verdict_token", message.reuse_lookup().verdict_token());
      }));
  service_->MaybeLogPasswordReuseLookupResultWithVerdict(
      web_contents(), PasswordType::OTHER_GAIA_PASSWORD,
      PasswordReuseLookup::REQUEST_SUCCESS, PasswordReuseLookup::SAFE,
      "verdict_token");
}

// The following tests are disabled on Android, because password capture events
// are not enabled on Android.
#if !defined(OS_ANDROID)
// Check that the PasswordCapturedEvent timer is set for 1 min if password
// hash is saved and no timer pref is set yet.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordCaptureEventScheduledOnStartup) {
  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);

  // Case 1: Check that the timer is not set in the ctor if no password hash is
  // saved.
  service_ = NewMockPasswordProtectionService(
      /*sync_password_hash=*/"");
  EXPECT_FALSE(service_->log_password_capture_timer_.IsRunning());

  // Case 1: Timer is set to 60 sec by default.
  service_ = NewMockPasswordProtectionService(
      /*sync_password_hash=*/"some-hash-value");
  EXPECT_TRUE(service_->log_password_capture_timer_.IsRunning());
  EXPECT_EQ(
      60, service_->log_password_capture_timer_.GetCurrentDelay().InSeconds());
}

// Check that the timer is set for prescribed time based on pref.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordCaptureEventScheduledFromPref) {
  // Pick a delay and store the deadline in the pref
  base::TimeDelta delay = base::TimeDelta::FromDays(13);
  SetDelayInPref(profile()->GetPrefs(),
                 prefs::kSafeBrowsingNextPasswordCaptureEventLogTime, delay);

  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);

  service_ = NewMockPasswordProtectionService(
      /*sync_password_hash=*/"");
  // Check that the timer is not set if no password hash is saved.
  EXPECT_EQ(
      0, service_->log_password_capture_timer_.GetCurrentDelay().InSeconds());

  // Save a password hash
  service_ = NewMockPasswordProtectionService(
      /*sync_password_hash=*/"some-hash-value");

  // Verify the delay is approx correct (not exact since we're not controlling
  // the clock).
  base::TimeDelta cur_delay =
      service_->log_password_capture_timer_.GetCurrentDelay();
  EXPECT_GE(14, cur_delay.InDays());
  EXPECT_LE(12, cur_delay.InDays());
}

// Check that we do log the event
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordCaptureEventRecorded) {
  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);

  // Case 1: Default service_ ctor has an empty password hash. Should not log.
  service_->MaybeLogPasswordCapture(/*did_log_in=*/false);
  ASSERT_EQ(0ul, GetUserEventService()->GetRecordedUserEvents().size());

  // Cases 2 and 3: With a password hash. Should log.
  service_ = NewMockPasswordProtectionService(
      /*sync_password_hash=*/"some-hash-value");
  service_->SetIsAccountSignedIn(true);
  service_->MaybeLogPasswordCapture(/*did_log_in=*/false);
  ASSERT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());
  GaiaPasswordCaptured event = GetUserEventService()
                                   ->GetRecordedUserEvents()[0]
                                   .gaia_password_captured_event();
  EXPECT_EQ(event.event_trigger(), GaiaPasswordCaptured::EXPIRED_28D_TIMER);

  service_->MaybeLogPasswordCapture(/*did_log_in=*/true);
  ASSERT_EQ(2ul, GetUserEventService()->GetRecordedUserEvents().size());
  event = GetUserEventService()
              ->GetRecordedUserEvents()[1]
              .gaia_password_captured_event();
  EXPECT_EQ(event.event_trigger(), GaiaPasswordCaptured::USER_LOGGED_IN);
}

// Check that we reschedule after logging.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordCaptureEventReschedules) {
  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);

  // Case 1: Default service_ ctor has an empty password hash, so we don't log
  // or reschedule the logging.
  service_->MaybeLogPasswordCapture(/*did_log_in=*/false);
  EXPECT_FALSE(service_->log_password_capture_timer_.IsRunning());

  // Case 2: A non-empty password hash.
  service_ = NewMockPasswordProtectionService(
      /*sync_password_hash=*/"some-hash-value");
  service_->SetIsAccountSignedIn(true);

  service_->MaybeLogPasswordCapture(/*did_log_in=*/false);

  // Verify the delay is approx correct. Will be 24-28 days, +- clock drift.
  EXPECT_TRUE(service_->log_password_capture_timer_.IsRunning());
  base::TimeDelta cur_delay =
      service_->log_password_capture_timer_.GetCurrentDelay();
  EXPECT_GT(29, cur_delay.InDays());
  EXPECT_LT(23, cur_delay.InDays());
}
#endif

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseLookupUserEventRecorded) {
  // Configure sync account type to GMAIL.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(kNoHostedDomainFound, account_info);

  NavigateAndCommit(GURL("https://www.example.com/"));

  unsigned long t = 0;
  for (const auto& it : kTestCasesWithoutVerdict) {
    service_->MaybeLogPasswordReuseLookupEvent(
        web_contents(), it.request_outcome,
        PasswordType::PRIMARY_ACCOUNT_PASSWORD, nullptr);
    ASSERT_EQ(t + 1, GetUserEventService()->GetRecordedUserEvents().size())
        << t;
    PasswordReuseLookup reuse_lookup = GetUserEventService()
                                           ->GetRecordedUserEvents()[t]
                                           .gaia_password_reuse_event()
                                           .reuse_lookup();
    EXPECT_EQ(it.lookup_result, reuse_lookup.lookup_result()) << t;
    t++;
  }

  {
    auto response = std::make_unique<LoginReputationClientResponse>();
    response->set_verdict_token("token1");
    response->set_verdict_type(LoginReputationClientResponse::LOW_REPUTATION);
    service_->MaybeLogPasswordReuseLookupEvent(
        web_contents(), RequestOutcome::RESPONSE_ALREADY_CACHED,
        PasswordType::PRIMARY_ACCOUNT_PASSWORD, response.get());
    ASSERT_EQ(t + 1, GetUserEventService()->GetRecordedUserEvents().size())
        << t;
    PasswordReuseLookup reuse_lookup = GetUserEventService()
                                           ->GetRecordedUserEvents()[t]
                                           .gaia_password_reuse_event()
                                           .reuse_lookup();
    EXPECT_EQ(PasswordReuseLookup::CACHE_HIT, reuse_lookup.lookup_result())
        << t;
    EXPECT_EQ(PasswordReuseLookup::LOW_REPUTATION, reuse_lookup.verdict()) << t;
    EXPECT_EQ("token1", reuse_lookup.verdict_token()) << t;
    t++;
  }

  {
    auto response = std::make_unique<LoginReputationClientResponse>();
    response->set_verdict_token("token2");
    response->set_verdict_type(LoginReputationClientResponse::SAFE);
    service_->MaybeLogPasswordReuseLookupEvent(
        web_contents(), RequestOutcome::SUCCEEDED,
        PasswordType::PRIMARY_ACCOUNT_PASSWORD, response.get());
    ASSERT_EQ(t + 1, GetUserEventService()->GetRecordedUserEvents().size())
        << t;
    PasswordReuseLookup reuse_lookup = GetUserEventService()
                                           ->GetRecordedUserEvents()[t]
                                           .gaia_password_reuse_event()
                                           .reuse_lookup();
    EXPECT_EQ(PasswordReuseLookup::REQUEST_SUCCESS,
              reuse_lookup.lookup_result())
        << t;
    EXPECT_EQ(PasswordReuseLookup::SAFE, reuse_lookup.verdict()) << t;
    EXPECT_EQ("token2", reuse_lookup.verdict_token()) << t;
    t++;
  }
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetDefaultChangePasswordURL) {
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount("example.com", account_info);
  EXPECT_EQ(GURL("https://accounts.google.com/"
                 "AccountChooser?Email=foo%40example.com&continue=https%3A%2F%"
                 "2Fmyaccount.google.com%2Fsigninoptions%2Fpassword%3Futm_"
                 "source%3DGoogle%26utm_campaign%3DPhishGuard&hl=en"),
            service_->GetDefaultChangePasswordURL());
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetEnterprisePasswordURL) {
  // If enterprise change password url is not set. This will return the default
  // GAIA change password url.
  EXPECT_TRUE(service_->GetEnterpriseChangePasswordURL().DomainIs(
      "accounts.google.com"));

  // Sets enterprise change password url.
  GURL enterprise_change_password_url("https://changepassword.example.com");
  profile()->GetPrefs()->SetString(prefs::kPasswordProtectionChangePasswordURL,
                                   enterprise_change_password_url.spec());
  EXPECT_EQ(enterprise_change_password_url,
            service_->GetEnterpriseChangePasswordURL());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringPasswordOnFocusPingNotBlocked) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  PrepareRequest(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                 PasswordType::PASSWORD_TYPE_UNKNOWN,
                 /*is_warning_showing=*/false);
  GURL redirect_url(kRedirectURL);
  content::MockNavigationHandle test_handle(redirect_url, main_rfh());
  std::unique_ptr<PasswordProtectionNavigationThrottle> throttle =
      service_->MaybeCreateNavigationThrottle(&test_handle);
  EXPECT_EQ(nullptr, throttle);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringPasswordReusePingDeferred) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  service_->SetIsSyncing(true);
  service_->SetIsAccountSignedIn(true);

  // Simulate a on-going password reuse request that hasn't received
  // verdict yet.
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordType::SAVED_PASSWORD,
                 /*is_warning_showing=*/false);

  GURL redirect_url(kRedirectURL);
  bool was_navigation_resumed = false;
  content::MockNavigationHandle test_handle(redirect_url, main_rfh());
  std::unique_ptr<PasswordProtectionNavigationThrottle> throttle =
      service_->MaybeCreateNavigationThrottle(&test_handle);
  ASSERT_NE(nullptr, throttle);
  throttle->set_resume_callback_for_testing(
      base::BindLambdaForTesting([&]() { was_navigation_resumed = true; }));

  // Verify navigation get deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER, throttle->WillStartRequest());
  base::RunLoop().RunUntilIdle();

  // Simulate receiving a SAFE verdict.
  SimulateRequestFinished(LoginReputationClientResponse::SAFE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(true, was_navigation_resumed);

  // Verify that navigation can be resumed.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringModalWarningCanceled) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  // Simulate a password reuse request, whose verdict is triggering a modal
  // warning.
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordType::PRIMARY_ACCOUNT_PASSWORD,
                 /*is_warning_showing=*/true);
  base::RunLoop().RunUntilIdle();

  // Simulate receiving a phishing verdict.
  SimulateRequestFinished(LoginReputationClientResponse::PHISHING);
  base::RunLoop().RunUntilIdle();

  GURL redirect_url(kRedirectURL);
  content::MockNavigationHandle test_handle(redirect_url, main_rfh());
  std::unique_ptr<PasswordProtectionNavigationThrottle> throttle =
      service_->MaybeCreateNavigationThrottle(&test_handle);

  // Verify that navigation gets canceled.
  EXPECT_EQ(content::NavigationThrottle::CANCEL, throttle->WillStartRequest());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationThrottleRemovedWhenNavigationHandleIsGone) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  service_->SetIsSyncing(true);
  service_->SetIsAccountSignedIn(true);
  // Simulate a on-going password reuse request that hasn't received
  // verdict yet.
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordType::SAVED_PASSWORD,
                 /*is_warning_showing=*/false);

  GURL redirect_url(kRedirectURL);
  content::MockNavigationHandle test_handle(redirect_url, main_rfh());
  std::unique_ptr<PasswordProtectionNavigationThrottle> throttle =
      service_->MaybeCreateNavigationThrottle(&test_handle);

  // Verify navigation get deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER, throttle->WillStartRequest());

  EXPECT_EQ(1u, GetNumberOfNavigationThrottles());

  // Simulate the deletion of the PasswordProtectionNavigationThrottle.
  throttle.reset();
  base::RunLoop().RunUntilIdle();

  // Expect no navigation throttle kept by |request_|.
  EXPECT_EQ(0u, GetNumberOfNavigationThrottles());

  // Simulate receiving a SAFE verdict.
  SimulateRequestFinished(LoginReputationClientResponse::SAFE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUnhandledSyncPasswordReuseUponClearHistoryDeletion) {
  ASSERT_EQ(0, GetSizeofUnhandledSyncPasswordReuses());
  GURL url_a("https://www.phishinga.com");
  GURL url_b("https://www.phishingb.com");
  GURL url_c("https://www.phishingc.com");

  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
  update->SetKey(Origin::Create(url_a).Serialize(),
                 base::Value("navigation_id_a"));
  update->SetKey(Origin::Create(url_b).Serialize(),
                 base::Value("navigation_id_b"));
  update->SetKey(Origin::Create(url_c).Serialize(),
                 base::Value("navigation_id_c"));

  // Delete a https://www.phishinga.com URL.
  history::URLRows deleted_urls;
  deleted_urls.push_back(history::URLRow(url_a));
  deleted_urls.push_back(history::URLRow(GURL("https://www.notinlist.com")));

  service_->RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      /*all_history=*/false, deleted_urls);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, GetSizeofUnhandledSyncPasswordReuses());

  service_->RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      /*all_history=*/true, {});
  EXPECT_EQ(0, GetSizeofUnhandledSyncPasswordReuses());
}

// The following tests are disabled on Android, because enterprise reporting
// extension is not supported.
#if !defined(OS_ANDROID)
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyOnPolicySpecifiedPasswordChangedEvent) {
  TestExtensionEventObserver event_observer(test_event_router_);

  // Preparing sync account.
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount("example.com", account_info);
  service_->SetIsAccountSignedIn(true);

  // Simulates change password.
  service_->OnGaiaPasswordChanged("foo@example.com", false);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordChanged::kEventName));

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("foo@example.com", captured_args.GetString());

  // If user is in incognito mode, no event should be sent.
  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  service_->OnGaiaPasswordChanged("foo@example.com", false);
  base::RunLoop().RunUntilIdle();
  // Event count should be unchanged.
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordChanged::kEventName));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyTriggerOnPolicySpecifiedPasswordReuseDetectedForGsuiteUser) {
  TestExtensionEventObserver event_observer(test_event_router_);
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount("example.com", account_info);
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  NavigateAndCommit(GURL(kPasswordReuseURL));

  service_->MaybeReportPasswordReuseDetected(web_contents(), kUserName,
                                             PasswordType::ENTERPRISE_PASSWORD,
                                             /*is_phishing_url =*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ(kPasswordReuseURL, captured_args.FindKey("url")->GetString());
  EXPECT_EQ(kUserName, captured_args.FindKey("userName")->GetString());
  EXPECT_TRUE(captured_args.FindKey("isPhishingUrl")->GetBool());

  // If the reused password is not Enterprise password but the account is
  // GSuite, event should be sent.
  service_->SetAccountInfo(kUserName);
  service_->SetIsAccountSignedIn(true);
  service_->MaybeReportPasswordReuseDetected(web_contents(), kUserName,
                                             PasswordType::OTHER_GAIA_PASSWORD,
                                             /*is_phishing_url =*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If no password is used , no event should be sent.
  service_->MaybeReportPasswordReuseDetected(
      web_contents(), kUserName, PasswordType::PASSWORD_TYPE_UNKNOWN,
      /*is_phishing_url =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If user is in incognito mode, no event should be sent.
  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  service_->MaybeReportPasswordReuseDetected(web_contents(), kUserName,
                                             PasswordType::ENTERPRISE_PASSWORD,
                                             /*is_phishing_url =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyTriggerOnPolicySpecifiedPasswordReuseDetectedForGmailUser) {
  TestExtensionEventObserver event_observer(test_event_router_);

  // If user is a Gmail user and enterprise password is used, event should be
  // sent.
  CoreAccountInfo gmail_account_info = SetPrimaryAccount(kTestGmail);
  SetUpSyncAccount(kNoHostedDomainFound, gmail_account_info);
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  NavigateAndCommit(GURL(kPasswordReuseURL));

  service_->MaybeReportPasswordReuseDetected(web_contents(), kUserName,
                                             PasswordType::ENTERPRISE_PASSWORD,
                                             /*is_phishing_url =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If user is a Gmail user and not an enterprise password is used , no event
  // should be sent.
  service_->MaybeReportPasswordReuseDetected(web_contents(), kUserName,
                                             PasswordType::OTHER_GAIA_PASSWORD,
                                             /*is_phishing_url =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If user is a Gmail user and no password is used , no event should be sent.
  service_->MaybeReportPasswordReuseDetected(
      web_contents(), kUserName, PasswordType::PASSWORD_TYPE_UNKNOWN,
      /*is_phishing_url =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
}
#endif

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetWarningDetailTextSaved) {
  base::string16 warning_text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  std::vector<size_t> placeholder_offsets;
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyGetWarningDetailTextSavedDomains) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {}, {password_manager::features::kPasswordCheck});
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  std::vector<std::string> domains{"www.example.com"};
  service_->set_saved_passwords_matching_domains(domains);
  base::string16 warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED_1_DOMAIN,
      base::UTF8ToUTF16(domains[0]));
  std::vector<size_t> placeholder_offsets;
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
  // GetWarningDetailText shouldCall GetWarningDetailTextForSavedPasswords, so
  // we can check to see if the placeholder offset values are the same.
  // Hardcoding the offset in the expected value cannot be done as it's
  // different for different OS.
  std::vector<size_t> expected_placeholder_offsets;
  service_->GetWarningDetailTextForSavedPasswords(
      &expected_placeholder_offsets);
  EXPECT_EQ(expected_placeholder_offsets, placeholder_offsets);

  placeholder_offsets.clear();
  domains.push_back("www.2.example.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED_2_DOMAINS,
      base::UTF8ToUTF16(domains[0]), base::UTF8ToUTF16(domains[1]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
  expected_placeholder_offsets.clear();
  service_->GetWarningDetailTextForSavedPasswords(
      &expected_placeholder_offsets);
  EXPECT_EQ(expected_placeholder_offsets, placeholder_offsets);

  placeholder_offsets.clear();
  domains.push_back("www.3.example.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED_3_DOMAINS,
      base::UTF8ToUTF16(domains[0]), base::UTF8ToUTF16(domains[1]),
      base::UTF8ToUTF16(domains[2]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
  expected_placeholder_offsets.clear();
  service_->GetWarningDetailTextForSavedPasswords(
      &expected_placeholder_offsets);
  EXPECT_EQ(expected_placeholder_offsets, placeholder_offsets);

  // Default domains should be prioritzed over other domains.
  placeholder_offsets.clear();
  domains.push_back("yahoo.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED_3_DOMAINS,
      base::UTF8ToUTF16("yahoo.com"), base::UTF8ToUTF16(domains[0]),
      base::UTF8ToUTF16(domains[1]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
  expected_placeholder_offsets.clear();
  service_->GetWarningDetailTextForSavedPasswords(
      &expected_placeholder_offsets);
  EXPECT_EQ(expected_placeholder_offsets, placeholder_offsets);

  // Matching domains that have a suffix of a default domains should be
  // prioritzed over other non common spoofed domains.
  placeholder_offsets.clear();
  domains.push_back("login.amazon.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED_3_DOMAINS,
      base::UTF8ToUTF16("yahoo.com"), base::UTF8ToUTF16("login.amazon.com"),
      base::UTF8ToUTF16(domains[0]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
  expected_placeholder_offsets.clear();
  service_->GetWarningDetailTextForSavedPasswords(
      &expected_placeholder_offsets);
  EXPECT_EQ(expected_placeholder_offsets, placeholder_offsets);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyGetWarningDetailTextCheckSavedDomains) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(password_manager::features::kPasswordCheck);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  std::vector<std::string> domains{"www.example.com"};
  service_->set_saved_passwords_matching_domains(domains);
  base::string16 warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_1_DOMAIN,
      base::UTF8ToUTF16(domains[0]));
  std::vector<size_t> placeholder_offsets;
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));

  placeholder_offsets.clear();
  domains.push_back("www.2.example.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_2_DOMAIN,
      base::UTF8ToUTF16(domains[0]), base::UTF8ToUTF16(domains[1]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));

  placeholder_offsets.clear();
  domains.push_back("www.3.example.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_3_DOMAIN,
      base::UTF8ToUTF16(domains[0]), base::UTF8ToUTF16(domains[1]),
      base::UTF8ToUTF16(domains[2]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
  // Default domains should be prioritzed over other domains.
  placeholder_offsets.clear();
  domains.push_back("amazon.com");
  service_->set_saved_passwords_matching_domains(domains);
  warning_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_3_DOMAIN,
      base::UTF8ToUTF16("amazon.com"), base::UTF8ToUTF16(domains[0]),
      base::UTF8ToUTF16(domains[1]));
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type,
                                                         &placeholder_offsets));
}
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyGetPlaceholdersForSavedPasswordWarningText) {
  std::vector<std::string> domains{"www.example.com"};
  domains.push_back("www.2.example.com");
  domains.push_back("www.3.example.com");
  domains.push_back("amazon.com");
  service_->set_saved_passwords_matching_domains(domains);
  // Default domains should be prioritzed over other domains.
  std::vector<base::string16> expected_placeholders{
      base::UTF8ToUTF16("amazon.com"), base::UTF8ToUTF16(domains[0]),
      base::UTF8ToUTF16(domains[1])};
  EXPECT_EQ(expected_placeholders,
            service_->GetPlaceholdersForSavedPasswordWarningText());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyGetWarningDetailTextEnterprise) {
  base::string16 warning_text_non_sync = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SIGNED_IN_NON_SYNC);
  base::string16 generic_enterprise_warning_text = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE);
  base::string16 warning_text_with_org_name = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE_WITH_ORG_NAME,
      base::UTF8ToUTF16("example.com"));
  base::string16 warning_text_sync =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SYNC);

  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  reused_password_type.set_is_account_syncing(true);
  std::vector<size_t> placeholder_offsets;
  EXPECT_EQ(warning_text_sync, service_->GetWarningDetailText(
                                   reused_password_type, &placeholder_offsets));
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  reused_password_type.set_is_account_syncing(false);
  EXPECT_EQ(generic_enterprise_warning_text,
            service_->GetWarningDetailText(reused_password_type,
                                           &placeholder_offsets));
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kPasswordProtectionForSignedInUsers);
    reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
    EXPECT_EQ(warning_text_non_sync,
              service_->GetWarningDetailText(reused_password_type,
                                             &placeholder_offsets));
  }

  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  reused_password_type.set_is_account_syncing(true);
  CoreAccountInfo core_account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(std::string("example.com"), core_account_info);
  EXPECT_EQ(warning_text_with_org_name,
            service_->GetWarningDetailText(reused_password_type,
                                           &placeholder_offsets));
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_EQ(generic_enterprise_warning_text,
            service_->GetWarningDetailText(reused_password_type,
                                           &placeholder_offsets));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetWarningDetailTextGmail) {
  base::string16 warning_text_non_sync = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SIGNED_IN_NON_SYNC);
  base::string16 warning_text_sync =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SYNC);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kPasswordProtectionForSignedInUsers);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  std::vector<size_t> placeholder_offsets;
  EXPECT_EQ(warning_text_non_sync,
            service_->GetWarningDetailText(reused_password_type,
                                           &placeholder_offsets));
  reused_password_type.set_is_account_syncing(true);
  EXPECT_EQ(warning_text_sync, service_->GetWarningDetailText(
                                   reused_password_type, &placeholder_offsets));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyCanShowInterstitial) {
  // Do not show interstitial if policy not set for password_alert.
  ASSERT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kSafeBrowsingWhitelistDomains));
  GURL trigger_url = GURL(kPhishingURL);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  EXPECT_FALSE(
      service_->CanShowInterstitial(reused_password_type, trigger_url));
  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  reused_password_type.set_is_account_syncing(true);
  EXPECT_FALSE(
      service_->CanShowInterstitial(reused_password_type, trigger_url));
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kPasswordProtectionForSignedInUsers);
    service_->SetAccountInfo(kUserName);
    reused_password_type.set_is_account_syncing(false);
    EXPECT_FALSE(
        service_->CanShowInterstitial(reused_password_type, trigger_url));
  }

  reused_password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  reused_password_type.set_is_account_syncing(false);
  EXPECT_FALSE(
      service_->CanShowInterstitial(reused_password_type, trigger_url));
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  EXPECT_FALSE(
      service_->CanShowInterstitial(reused_password_type, trigger_url));
  // Show interstitial if user is a syncing GSuite user and the policy is set to
  // password_alert.
  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  reused_password_type.set_is_account_syncing(true);
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  EXPECT_TRUE(service_->CanShowInterstitial(reused_password_type, trigger_url));
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        safe_browsing::kPasswordProtectionForSignedInUsers);
    service_->SetAccountInfo(kUserName);
    reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
    reused_password_type.set_is_account_syncing(false);
    profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                      PASSWORD_REUSE);
    EXPECT_TRUE(
        service_->CanShowInterstitial(reused_password_type, trigger_url));
  }
  // Show interstitial if user is a Enterprise user and the policy is set to
  // password_alert.
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  reused_password_type.set_is_account_syncing(false);
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  EXPECT_TRUE(service_->CanShowInterstitial(reused_password_type, trigger_url));

  // Add |trigger_url| to enterprise whitelist.
  base::ListValue whitelisted_domains;
  whitelisted_domains.AppendString(trigger_url.host());
  profile()->GetPrefs()->Set(prefs::kSafeBrowsingWhitelistDomains,
                             whitelisted_domains);
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  reused_password_type.set_is_account_syncing(false);
  EXPECT_FALSE(
      service_->CanShowInterstitial(reused_password_type, trigger_url));
  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  reused_password_type.set_is_account_syncing(true);
  EXPECT_FALSE(
      service_->CanShowInterstitial(reused_password_type, trigger_url));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifySendsPingForAboutBlank) {
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(
      service_->CanSendPing(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                            GURL("about:blank"), reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetPingNotSentReason) {
  {
    // SBER disabled.
    ReusedPasswordAccountType reused_password_type;
    service_->ConfigService(false /*incognito*/, false /*SBER*/);
    EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_USER_POPULATION,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                  GURL("about:blank"), reused_password_type));
    reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
    EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_USER_POPULATION,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("about:blank"), reused_password_type));
  }
  {
    // In Incognito.
    ReusedPasswordAccountType reused_password_type;
    service_->ConfigService(true /*incognito*/, true /*SBER*/);
    EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_INCOGNITO,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                  GURL("about:blank"), reused_password_type));
  }
  {
    // Turned off by admin.
    ReusedPasswordAccountType reused_password_type;
    service_->ConfigService(false /*incognito*/, false /*SBER*/);
    reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
    profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                      PASSWORD_PROTECTION_OFF);
    EXPECT_EQ(RequestOutcome::TURNED_OFF_BY_ADMIN,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("about:blank"), reused_password_type));
  }
  {
    // Whitelisted by policy.
    ReusedPasswordAccountType reused_password_type;
    service_->ConfigService(false /*incognito*/, false /*SBER*/);
    reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
    profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                      PHISHING_REUSE);
    base::ListValue whitelist;
    whitelist.AppendString("mydomain.com");
    whitelist.AppendString("mydomain.net");
    profile()->GetPrefs()->Set(prefs::kSafeBrowsingWhitelistDomains, whitelist);
    EXPECT_EQ(RequestOutcome::MATCHED_ENTERPRISE_WHITELIST,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("https://www.mydomain.com"), reused_password_type));
  }
  {
    // Password alert mode.
    ReusedPasswordAccountType reused_password_type;
    service_->ConfigService(false /*incognito*/, false /*SBER*/);
    reused_password_type.set_account_type(ReusedPasswordAccountType::UNKNOWN);
    profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                      PASSWORD_REUSE);
    EXPECT_EQ(RequestOutcome::PASSWORD_ALERT_MODE,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("about:blank"), reused_password_type));
  }
}

namespace {

class ChromePasswordProtectionServiceWithAccountPasswordStoreTest
    : public ChromePasswordProtectionServiceTest {
 public:
  ChromePasswordProtectionServiceWithAccountPasswordStoreTest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kEnablePasswordsAccountStorage);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ChromePasswordProtectionServiceWithAccountPasswordStoreTest,
       VerifyPersistPhishedSavedPasswordCredential) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {.signon_realm = "http://example.test",
       .in_store = autofill::PasswordForm::Store::kAccountStore},
      {.signon_realm = "http://2.example.test",
       .in_store = autofill::PasswordForm::Store::kAccountStore}};

  EXPECT_CALL(*account_password_store_, AddCompromisedCredentialsImpl(_))
      .Times(2);
  service_->PersistPhishedSavedPasswordCredential(credentials);
}

TEST_F(ChromePasswordProtectionServiceWithAccountPasswordStoreTest,
       VerifyRemovePhishedSavedPasswordCredential) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", base::ASCIIToUTF16("username1"),
       autofill::PasswordForm::Store::kAccountStore},
      {"http://2.example.test", base::ASCIIToUTF16("username2"),
       autofill::PasswordForm::Store::kAccountStore}};

  EXPECT_CALL(*account_password_store_,
              RemoveCompromisedCredentialsImpl(
                  _, _,
                  password_manager::RemoveCompromisedCredentialsReason::
                      kMarkSiteAsLegitimate))
      .Times(2);
  service_->RemovePhishedSavedPasswordCredential(credentials);
}

}  // namespace

}  // namespace safe_browsing
