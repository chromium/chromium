// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/user_events/fake_user_event_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/test_event_router.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using sync_pb::UserEventSpecifics;
using GaiaPasswordReuse = UserEventSpecifics::GaiaPasswordReuse;
using GaiaPasswordCaptured = UserEventSpecifics::GaiaPasswordCaptured;
using PasswordReuseDialogInteraction =
    GaiaPasswordReuse::PasswordReuseDialogInteraction;
using PasswordReuseLookup = GaiaPasswordReuse::PasswordReuseLookup;
using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;

namespace OnPolicySpecifiedPasswordReuseDetected = extensions::api::
    safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected;
namespace OnPolicySpecifiedPasswordChanged =
    extensions::api::safe_browsing_private::OnPolicySpecifiedPasswordChanged;

namespace safe_browsing {

namespace {

const char kPhishingURL[] = "http://phishing.com/";
const char kPasswordReuseURL[] = "http://login.example.com/";
const char kTestGaiaID[] = "gaia_id";
const char kTestEmail[] = "foo@example.com";
const char kTestGmail[] = "foo@gmail.com";
const char kBasicResponseHeaders[] = "HTTP/1.1 200 OK";
const char kRedirectURL[] = "http://redirect.com";

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
      scoped_refptr<HostContentSettingsMap> content_setting_map,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      StringProvider sync_password_hash_provider)
      : ChromePasswordProtectionService(profile,
                                        content_setting_map,
                                        ui_manager,
                                        sync_password_hash_provider),
        is_incognito_(false),
        is_extended_reporting_(false) {}
  bool IsExtendedReporting() override { return is_extended_reporting_; }
  bool IsIncognito() override { return is_incognito_; }

  // Configures the results returned by IsExtendedReporting(), IsIncognito(),
  // and IsHistorySyncEnabled().
  void ConfigService(bool is_incognito, bool is_extended_reporting) {
    is_incognito_ = is_incognito;
    is_extended_reporting_ = is_extended_reporting;
  }

  SafeBrowsingUIManager* ui_manager() { return ui_manager_.get(); }

 protected:
  friend class ChromePasswordProtectionServiceTest;

 private:
  bool is_incognito_;
  bool is_extended_reporting_;
  std::string mocked_sync_password_hash_;
};

class ChromePasswordProtectionServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordProtectionServiceTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
    profile()->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger,
        PasswordProtectionTrigger::PHISHING_REUSE);
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* incognito */, false /* guest_profile */,
        false /* store_last_modified */,
        false /* migrate_requesting_and_top_level_origin_settings */);

    service_ = NewMockPasswordProtectionService();
    fake_user_event_service_ = static_cast<syncer::FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(browser_context(),
                                      GetFakeUserEventServiceFactory()));
    test_event_router_ =
        extensions::CreateAndUseTestEventRouter(browser_context());
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            browser_context(),
            base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter));
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    service_.reset();
    request_ = nullptr;
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

    return std::make_unique<MockChromePasswordProtectionService>(
        profile(), content_setting_map_,
        new SafeBrowsingUIManager(
            SafeBrowsingService::CreateSafeBrowsingService()),
        sync_password_hash_provider);
  }

  content::BrowserContext* CreateBrowserContext() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeProfileOAuth2TokenService));
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    builder.AddTestingFactory(
        AccountFetcherServiceFactory::GetInstance(),
        base::BindRepeating(&FakeAccountFetcherServiceBuilder::BuildForTests));
    return builder.Build().release();
  }

  syncer::FakeUserEventService* GetUserEventService() {
    return fake_user_event_service_;
  }

  void InitializeRequest(
      LoginReputationClientRequest::TriggerType trigger_type,
      PasswordReuseEvent::ReusedPasswordType reused_password_type) {
    if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
      request_ = new PasswordProtectionRequest(
          web_contents(), GURL(kPhishingURL), GURL(), GURL(),
          PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN,
          std::vector<std::string>({"somedomain.com"}), trigger_type, true,
          service_.get(), 0);
    } else {
      ASSERT_EQ(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                trigger_type);
      request_ = new PasswordProtectionRequest(
          web_contents(), GURL(kPhishingURL), GURL(), GURL(),
          reused_password_type, std::vector<std::string>(), trigger_type, true,
          service_.get(), 0);
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
    service_->RequestFinished(request_.get(), false, std::move(verdict));
  }

  void SetUpSyncAccount(const std::string& hosted_domain,
                        const std::string& gaia_id,
                        const std::string& email) {
    FakeAccountFetcherService* account_fetcher_service =
        static_cast<FakeAccountFetcherService*>(
            AccountFetcherServiceFactory::GetForProfile(profile()));
    AccountTrackerService* account_tracker_service =
        AccountTrackerServiceFactory::GetForProfile(profile());
    account_fetcher_service->FakeUserInfoFetchSuccess(
        account_tracker_service->PickAccountIdForAccount(gaia_id, email), email,
        gaia_id, hosted_domain, "full_name", "given_name", "locale",
        "http://picture.example.com/picture.jpg");
  }

  void PrepareRequest(
      LoginReputationClientRequest::TriggerType trigger_type,
      PasswordReuseEvent::ReusedPasswordType reused_password_type,
      bool is_warning_showing) {
    InitializeRequest(trigger_type, reused_password_type);
    request_->set_is_modal_warning_showing(is_warning_showing);
    service_->pending_requests_.insert(request_);
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateWillStart(
      content::NavigationHandle* test_handle) {
    std::unique_ptr<PasswordProtectionNavigationThrottle> throttle =
        service_->MaybeCreateNavigationThrottle(test_handle);
    if (throttle)
      test_handle->RegisterThrottleForTesting(std::move(throttle));

    return test_handle->CallWillStartRequestForTesting();
  }

  int GetSizeofUnhandledSyncPasswordReuses() {
    DictionaryPrefUpdate unhandled_sync_password_reuses(
        profile()->GetPrefs(), prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
    return unhandled_sync_password_reuses->size();
  }

  size_t GetNumberOfNavigationThrottles() {
    return request_ ? request_->throttles_.size() : 0u;
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  std::unique_ptr<MockChromePasswordProtectionService> service_;
  scoped_refptr<PasswordProtectionRequest> request_;
  std::unique_ptr<LoginReputationClientResponse> verdict_;
  // Owned by KeyedServiceFactory.
  syncer::FakeUserEventService* fake_user_event_service_;
  extensions::TestEventRouter* test_event_router_;
};

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForPasswordOnFocusPing) {
  // Password field on focus pinging is enabled on !incognito && SBER.
  RequestOutcome reason;
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN, &reason));
  EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_USER_POPULATION, reason);

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN, &reason));

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN, &reason));
  EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_INCOGNITO, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN, &reason));
  EXPECT_EQ(RequestOutcome::DISABLED_DUE_TO_INCOGNITO, reason);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForSavedPasswordEntryPing) {
  RequestOutcome reason;
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SAVED_PASSWORD, &reason));

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SAVED_PASSWORD, &reason));

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SAVED_PASSWORD, &reason));

  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SAVED_PASSWORD, &reason));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyUserPopulationForSyncPasswordEntryPing) {
  // If user is not signed in, no ping should be sent.
  RequestOutcome reason;
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));
  EXPECT_EQ(RequestOutcome::USER_NOT_SIGNED_IN, reason);

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));
  EXPECT_EQ(RequestOutcome::USER_NOT_SIGNED_IN, reason);

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));
  EXPECT_EQ(RequestOutcome::USER_NOT_SIGNED_IN, reason);

  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));
  EXPECT_EQ(RequestOutcome::USER_NOT_SIGNED_IN, reason);

  SigninManagerFactory::GetForProfile(profile())->SetAuthenticatedAccountInfo(
      kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID), std::string(kTestEmail));

  // Sync password entry pinging is enabled by default.
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));

  service_->ConfigService(true /*incognito*/, true /*SBER*/);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));

  // If sync password entry pinging is disabled by policy,
  // |IsPingingEnabled(..)| should return false.
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_PROTECTION_OFF);
  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));
  EXPECT_EQ(RequestOutcome::TURNED_OFF_BY_ADMIN, reason);

  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &reason));
  EXPECT_EQ(RequestOutcome::PASSWORD_ALERT_MODE, reason);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPingingIsSkippedIfMatchEnterpriseWhitelist) {
  RequestOutcome reason = RequestOutcome::UNKNOWN;
  ASSERT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kSafeBrowsingWhitelistDomains));

  // If there's no whitelist, IsURLWhitelistedForPasswordEntry(_,_) should
  // return false.
  EXPECT_FALSE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://www.mydomain.com"), &reason));
  EXPECT_EQ(RequestOutcome::UNKNOWN, reason);

  // Verify if match enterprise whitelist.
  base::ListValue whitelist;
  whitelist.AppendString("mydomain.com");
  whitelist.AppendString("mydomain.net");
  profile()->GetPrefs()->Set(prefs::kSafeBrowsingWhitelistDomains, whitelist);
  EXPECT_TRUE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://www.mydomain.com"), &reason));
  EXPECT_EQ(RequestOutcome::MATCHED_ENTERPRISE_WHITELIST, reason);

  // Verify if matches enterprise change password url.
  profile()->GetPrefs()->ClearPref(prefs::kSafeBrowsingWhitelistDomains);
  reason = RequestOutcome::UNKNOWN;
  EXPECT_FALSE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://www.mydomain.com"), &reason));
  EXPECT_EQ(RequestOutcome::UNKNOWN, reason);

  profile()->GetPrefs()->SetString(prefs::kPasswordProtectionChangePasswordURL,
                                   "https://mydomain.com/change_password.html");
  EXPECT_TRUE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://mydomain.com/change_password.html#ref?user_name=alice"),
      &reason));
  EXPECT_EQ(RequestOutcome::MATCHED_ENTERPRISE_CHANGE_PASSWORD_URL, reason);

  // Verify if matches enterprise login url.
  profile()->GetPrefs()->ClearPref(prefs::kSafeBrowsingWhitelistDomains);
  profile()->GetPrefs()->ClearPref(prefs::kPasswordProtectionChangePasswordURL);
  reason = RequestOutcome::UNKNOWN;
  EXPECT_FALSE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://www.mydomain.com"), &reason));
  EXPECT_EQ(RequestOutcome::UNKNOWN, reason);
  base::ListValue login_urls;
  login_urls.AppendString("https://mydomain.com/login.html");
  profile()->GetPrefs()->Set(prefs::kPasswordProtectionLoginURLs, login_urls);
  EXPECT_TRUE(service_->IsURLWhitelistedForPasswordEntry(
      GURL("https://mydomain.com/login.html#ref?user_name=alice"), &reason));
  EXPECT_EQ(RequestOutcome::MATCHED_ENTERPRISE_LOGIN_URL, reason);
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetSyncAccountTypeGmail) {
  EXPECT_EQ(PasswordReuseEvent::NOT_SIGNED_IN, service_->GetSyncAccountType());
  EXPECT_TRUE(
      service_->GetOrganizationName(PasswordReuseEvent::SIGN_IN_PASSWORD)
          .empty());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestGmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID),
                   std::string(kTestGmail /*foo@gmail.com*/));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());
  EXPECT_EQ(
      "", service_->GetOrganizationName(PasswordReuseEvent::SIGN_IN_PASSWORD));
  EXPECT_EQ("", service_->GetOrganizationName(
                    PasswordReuseEvent::ENTERPRISE_PASSWORD));

  // Verify GetSyncAccountType() for incognito profile.
  service_->ConfigService(true /*is_incognito*/,
                          false /*is_extended_reporting*/);
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetSyncAccountTypeGSuite) {
  EXPECT_EQ(PasswordReuseEvent::NOT_SIGNED_IN, service_->GetSyncAccountType());
  EXPECT_TRUE(
      service_->GetOrganizationName(PasswordReuseEvent::SIGN_IN_PASSWORD)
          .empty());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());

  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount("example.com", std::string(kTestGaiaID),
                   std::string(kTestEmail /*foo@example.com*/));
  EXPECT_EQ(PasswordReuseEvent::GSUITE, service_->GetSyncAccountType());
  EXPECT_EQ("example.com", service_->GetOrganizationName(
                               PasswordReuseEvent::SIGN_IN_PASSWORD));
  EXPECT_EQ("", service_->GetOrganizationName(
                    PasswordReuseEvent::ENTERPRISE_PASSWORD));

  // Verify GetSyncAccountType() for incognito profile.
  service_->ConfigService(true /*is_incognito*/,
                          false /*is_extended_reporting*/);
  EXPECT_EQ(PasswordReuseEvent::GSUITE, service_->GetSyncAccountType());
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
  service_->CacheVerdict(
      url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      PasswordReuseEvent::SIGN_IN_PASSWORD, &verdict_proto, base::Time::Now());

  service_->UpdateSecurityState(SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE,
                                PasswordReuseEvent::SIGN_IN_PASSWORD,
                                web_contents());
  ASSERT_TRUE(service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE, current_threat_type);

  service_->UpdateSecurityState(safe_browsing::SB_THREAT_TYPE_SAFE,
                                PasswordReuseEvent::SIGN_IN_PASSWORD,
                                web_contents());
  current_threat_type = SB_THREAT_TYPE_UNUSED;
  service_->ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      url, false, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type);
  EXPECT_EQ(SB_THREAT_TYPE_UNUSED, current_threat_type);
  LoginReputationClientResponse verdict;
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            service_->GetCachedVerdict(
                url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                PasswordReuseEvent::SIGN_IN_PASSWORD, &verdict));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseUserEventNotRecordedDueToNotSignedIn) {
  // Feature not enabled so nothing should be logged.
  NavigateAndCommit(GURL("https:www.example.com/"));

  // PasswordReuseDetected
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
  service_->MaybeLogPasswordReuseLookupEvent(
      web_contents(), RequestOutcome::MATCHED_WHITELIST, nullptr);
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());

  // PasswordReuseLookup
  unsigned long t = 0;
  for (const auto& it : kTestCasesWithoutVerdict) {
    service_->MaybeLogPasswordReuseLookupEvent(web_contents(),
                                               it.request_outcome, nullptr);
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
       VerifyPasswordReuseUserEventNotRecordedDueToIncognito) {
  // Configure sync account type to GMAIL.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID), std::string(kTestEmail));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());
  service_->ConfigService(true /*is_incognito*/,
                          false /*is_extended_reporting*/);
  ASSERT_TRUE(service_->IsIncognito());

  // Nothing should be logged because of incognito.
  NavigateAndCommit(GURL("https:www.example.com/"));

  // PasswordReuseDetected
  service_->MaybeLogPasswordReuseDetectedEvent(web_contents());
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());
  service_->MaybeLogPasswordReuseLookupEvent(
      web_contents(), RequestOutcome::MATCHED_WHITELIST, nullptr);
  EXPECT_TRUE(GetUserEventService()->GetRecordedUserEvents().empty());

  // PasswordReuseLookup
  unsigned long t = 0;
  for (const auto& it : kTestCasesWithoutVerdict) {
    service_->MaybeLogPasswordReuseLookupEvent(web_contents(),
                                               it.request_outcome, nullptr);
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
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID), std::string(kTestEmail));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());

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

// Check that the PaswordCapturedEvent timer is set for 1 min if password
// hash is saved and no timer pref is set yet.
TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordCaptureEventScheduledOnStartup) {
  // Configure sync account type to GMAIL.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID), std::string(kTestEmail));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());

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
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID), std::string(kTestEmail));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());

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
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID), std::string(kTestEmail));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());

  // Case 1: Default service_ ctor has an empty password hash. Should not log.
  service_->MaybeLogPasswordCapture(/*did_log_in=*/false);
  ASSERT_EQ(0ul, GetUserEventService()->GetRecordedUserEvents().size());

  // Cases 2 and 3: With a password hash. Should log.
  service_ = NewMockPasswordProtectionService(
      /*sync_password_hash=*/"some-hash-value");

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
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID), std::string(kTestEmail));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());

  // Case 1: Default service_ ctor has an empty password hash, so we don't log
  // or reschedule the logging.
  service_->MaybeLogPasswordCapture(/*did_log_in=*/false);
  EXPECT_FALSE(service_->log_password_capture_timer_.IsRunning());

  // Case 2: A non-empty password hash.
  service_ = NewMockPasswordProtectionService(
      /*sync_password_hash=*/"some-hash-value");

  service_->MaybeLogPasswordCapture(/*did_log_in=*/false);

  // Verify the delay is approx correct. Will be 24-28 days, +- clock drift.
  EXPECT_TRUE(service_->log_password_capture_timer_.IsRunning());
  base::TimeDelta cur_delay =
      service_->log_password_capture_timer_.GetCurrentDelay();
  EXPECT_GT(29, cur_delay.InDays());
  EXPECT_LT(23, cur_delay.InDays());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPasswordReuseLookupUserEventRecorded) {
  // Configure sync account type to GMAIL.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID), std::string(kTestEmail));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());

  NavigateAndCommit(GURL("https://www.example.com/"));

  unsigned long t = 0;
  for (const auto& it : kTestCasesWithoutVerdict) {
    service_->MaybeLogPasswordReuseLookupEvent(web_contents(),
                                               it.request_outcome, nullptr);
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
        response.get());
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
        web_contents(), RequestOutcome::SUCCEEDED, response.get());
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
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount("example.com", std::string(kTestGaiaID),
                   std::string(kTestEmail));
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
                 PasswordReuseEvent::REUSED_PASSWORD_TYPE_UNKNOWN,
                 /*is_warning_showing=*/false);
  GURL redirect_url(kRedirectURL);
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(redirect_url,
                                                                  main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateWillStart(test_handle.get()));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringPasswordReusePingDeferred) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  // Simulate a on-going password reuse request that hasn't received
  // verdict yet.
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordReuseEvent::SIGN_IN_PASSWORD,
                 /*is_warning_showing=*/false);

  GURL redirect_url(kRedirectURL);
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(redirect_url,
                                                                  main_rfh());
  // Verify navigation get deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            SimulateWillStart(test_handle.get()));
  EXPECT_FALSE(test_handle->HasCommitted());
  base::RunLoop().RunUntilIdle();

  // Simulate receiving a SAFE verdict.
  SimulateRequestFinished(LoginReputationClientResponse::SAFE);
  base::RunLoop().RunUntilIdle();

  // Verify that navigation can be resumed.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            test_handle->CallWillProcessResponseForTesting(
                main_rfh(),
                net::HttpUtil::AssembleRawHeaders(
                    kBasicResponseHeaders, strlen(kBasicResponseHeaders)),
                false, net::ProxyServer::Direct()));
  test_handle->CallDidCommitNavigationForTesting(redirect_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_handle->HasCommitted());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringModalWarningCanceled) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  // Simulate a password reuse request, whose verdict is triggering a modal
  // warning.
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordReuseEvent::SIGN_IN_PASSWORD,
                 /*is_warning_showing=*/true);
  base::RunLoop().RunUntilIdle();

  // Simulate receiving a phishing verdict.
  SimulateRequestFinished(LoginReputationClientResponse::PHISHING);
  base::RunLoop().RunUntilIdle();

  GURL redirect_url(kRedirectURL);
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(redirect_url,
                                                                  main_rfh());
  // Verify that navigation gets canceled.
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            SimulateWillStart(test_handle.get()));
  EXPECT_FALSE(test_handle->HasCommitted());
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationThrottleRemovedWhenNavigationHandleIsGone) {
  GURL trigger_url(kPhishingURL);
  NavigateAndCommit(trigger_url);
  // Simulate a on-going password reuse request that hasn't received
  // verdict yet.
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordReuseEvent::SIGN_IN_PASSWORD,
                 /*is_warning_showing=*/false);

  GURL redirect_url(kRedirectURL);
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(redirect_url,
                                                                  main_rfh());
  // Verify navigation get deferred.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            SimulateWillStart(test_handle.get()));

  EXPECT_EQ(1u, GetNumberOfNavigationThrottles());

  // Simulate the deletion of NavigationHandle.
  test_handle.reset();
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
                              prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
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

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyOnPolicySpecifiedPasswordChangedEvent) {
  TestExtensionEventObserver event_observer(test_event_router_);

  // Preparing sync account.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount("example.com", std::string(kTestGaiaID),
                   std::string(kTestEmail));

  // Simulates change password.
  service_->OnGaiaPasswordChanged();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordChanged::kEventName));

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("foo@example.com", captured_args.GetString());

  // If user is in incognito mode, no event should be sent.
  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  service_->OnGaiaPasswordChanged();
  base::RunLoop().RunUntilIdle();
  // Event count should be unchanged.
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordChanged::kEventName));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyOnPolicySpecifiedPasswordReuseDetectedEventForPasswordReuse) {
  TestExtensionEventObserver event_observer(test_event_router_);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount("example.com", std::string(kTestGaiaID),
                   std::string(kTestEmail));
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  NavigateAndCommit(GURL(kPasswordReuseURL));

  service_->MaybeStartProtectedPasswordEntryRequest(
      web_contents(), web_contents()->GetLastCommittedURL(),
      PasswordReuseEvent::SIGN_IN_PASSWORD, {},
      /*password_field_exists =*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ(kPasswordReuseURL, captured_args.FindKey("url")->GetString());
  EXPECT_EQ("foo@example.com", captured_args.FindKey("userName")->GetString());
  EXPECT_FALSE(captured_args.FindKey("isPhishingUrl")->GetBool());

  // If user is in incognito mode, no event should be sent.
  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  service_->MaybeStartProtectedPasswordEntryRequest(
      web_contents(), web_contents()->GetLastCommittedURL(),
      PasswordReuseEvent::SIGN_IN_PASSWORD, {},
      /*password_field_exists =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyOnPolicySpecifiedPasswordReuseDetectedEventForPhishingReuse) {
  TestExtensionEventObserver event_observer(test_event_router_);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount("example.com", std::string(kTestGaiaID),
                   std::string(kTestEmail));
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  NavigateAndCommit(GURL(kPhishingURL));
  service_->OnModalWarningShownForSignInPassword(web_contents(),
                                                 "verdict_token");
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ(kPhishingURL, captured_args.FindKey("url")->GetString());
  EXPECT_EQ("foo@example.com", captured_args.FindKey("userName")->GetString());
  EXPECT_TRUE(captured_args.FindKey("isPhishingUrl")->GetBool());

  // If user is in incognito mode, no event should be sent.
  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  service_->OnModalWarningShownForSignInPassword(web_contents(),
                                                 "verdict_token");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyGetWarningDetailTextEnterprise) {
  base::string16 default_warning_text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS);
  base::string16 generic_enterprise_warning_text = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE);
  base::string16 warning_text_with_org_name = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE_WITH_ORG_NAME,
      base::UTF8ToUTF16("example.com"));

  EXPECT_EQ(default_warning_text, service_->GetWarningDetailText(
                                      PasswordReuseEvent::SIGN_IN_PASSWORD));
  EXPECT_EQ(
      generic_enterprise_warning_text,
      service_->GetWarningDetailText(PasswordReuseEvent::ENTERPRISE_PASSWORD));

  // Signs in as a GSuite user.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestEmail);
  SetUpSyncAccount(std::string("example.com"), std::string(kTestGaiaID),
                   std::string(kTestEmail /*foo@example.com*/));
  EXPECT_EQ(PasswordReuseEvent::GSUITE, service_->GetSyncAccountType());
  EXPECT_EQ(
      warning_text_with_org_name,
      service_->GetWarningDetailText(PasswordReuseEvent::SIGN_IN_PASSWORD));
  EXPECT_EQ(
      generic_enterprise_warning_text,
      service_->GetWarningDetailText(PasswordReuseEvent::ENTERPRISE_PASSWORD));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetWarningDetailTextGmail) {
  base::string16 default_warning_text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS);
  base::string16 generic_enterprise_warning_text = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE);

  EXPECT_EQ(default_warning_text, service_->GetWarningDetailText(
                                      PasswordReuseEvent::SIGN_IN_PASSWORD));
  EXPECT_EQ(
      generic_enterprise_warning_text,
      service_->GetWarningDetailText(PasswordReuseEvent::ENTERPRISE_PASSWORD));

  // Signs in as a Gmail user.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedAccountInfo(kTestGaiaID, kTestGmail);
  SetUpSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                   std::string(kTestGaiaID),
                   std::string(kTestGmail /*foo@gmail.com*/));
  EXPECT_EQ(PasswordReuseEvent::GMAIL, service_->GetSyncAccountType());
  EXPECT_EQ(default_warning_text, service_->GetWarningDetailText(
                                      PasswordReuseEvent::SIGN_IN_PASSWORD));
  EXPECT_EQ(
      generic_enterprise_warning_text,
      service_->GetWarningDetailText(PasswordReuseEvent::ENTERPRISE_PASSWORD));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyCanShowInterstitial) {
  ASSERT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kSafeBrowsingWhitelistDomains));
  GURL trigger_url = GURL(kPhishingURL);
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::TURNED_OFF_BY_ADMIN, PasswordReuseEvent::SAVED_PASSWORD,
      trigger_url));
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::TURNED_OFF_BY_ADMIN, PasswordReuseEvent::SIGN_IN_PASSWORD,
      trigger_url));
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::TURNED_OFF_BY_ADMIN,
      PasswordReuseEvent::OTHER_GAIA_PASSWORD, trigger_url));
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::TURNED_OFF_BY_ADMIN,
      PasswordReuseEvent::ENTERPRISE_PASSWORD, trigger_url));
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::PASSWORD_ALERT_MODE, PasswordReuseEvent::SAVED_PASSWORD,
      trigger_url));
  EXPECT_TRUE(service_->CanShowInterstitial(
      RequestOutcome::PASSWORD_ALERT_MODE, PasswordReuseEvent::SIGN_IN_PASSWORD,
      trigger_url));
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::PASSWORD_ALERT_MODE,
      PasswordReuseEvent::OTHER_GAIA_PASSWORD, trigger_url));
  EXPECT_TRUE(service_->CanShowInterstitial(
      RequestOutcome::PASSWORD_ALERT_MODE,
      PasswordReuseEvent::ENTERPRISE_PASSWORD, trigger_url));

  // Add |trigger_url| to enterprise whitelist.
  base::ListValue whitelisted_domains;
  whitelisted_domains.AppendString(trigger_url.host());
  profile()->GetPrefs()->Set(prefs::kSafeBrowsingWhitelistDomains,
                             whitelisted_domains);
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::PASSWORD_ALERT_MODE, PasswordReuseEvent::SAVED_PASSWORD,
      trigger_url));
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::PASSWORD_ALERT_MODE, PasswordReuseEvent::SIGN_IN_PASSWORD,
      trigger_url));
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::PASSWORD_ALERT_MODE,
      PasswordReuseEvent::OTHER_GAIA_PASSWORD, trigger_url));
  EXPECT_FALSE(service_->CanShowInterstitial(
      RequestOutcome::PASSWORD_ALERT_MODE,
      PasswordReuseEvent::ENTERPRISE_PASSWORD, trigger_url));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifySendsPingForAboutBlank) {
  RequestOutcome reason;
  EXPECT_TRUE(service_->CanSendPing(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT, GURL("about:blank"),
      PasswordReuseEvent::SAVED_PASSWORD, &reason));
}

}  // namespace safe_browsing
