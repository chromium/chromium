// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/password_manager/android/mock_password_checkup_launcher_helper.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/test/test_sync_service.h"
#endif

// All tests related to extension is disabled on Android, because enterprise
// reporting extension is not supported.
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "extensions/browser/test_event_router.h"
#endif

using password_manager::MatchingReusedCredential;
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

#if !BUILDFLAG(IS_ANDROID)
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
               base::WeakPtr<syncer::DataTypeControllerDelegate>());
  MOCK_METHOD1(RecordGaiaPasswordReuse, void(const GaiaPasswordReuse&));
};

namespace safe_browsing {

namespace {

const char kPhishingURL[] = "http://phishing.com/";
const char kTestEmail[] = "foo@example.com";
const char kUserName[] = "username";
const char kRedirectURL[] = "http://redirect.com";
#if !BUILDFLAG(IS_ANDROID)
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
    {RequestOutcome::MATCHED_ALLOWLIST, PasswordReuseLookup::ALLOWLIST_HIT},
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

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}
#endif

}  // namespace

class MockChromePasswordProtectionService
    : public ChromePasswordProtectionService {
 public:
  explicit MockChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      StringProvider sync_password_hash_provider,
      VerdictCacheManager* cache_manager,
      ChangePhishedCredentialsCallback add_phished_credentials,
      ChangePhishedCredentialsCallback remove_phished_credentials)
      : ChromePasswordProtectionService(profile,
                                        ui_manager,
                                        sync_password_hash_provider,
                                        cache_manager,
                                        add_phished_credentials,
                                        remove_phished_credentials),
        is_incognito_(false),
        is_extended_reporting_(false),
        is_syncing_(false),
        is_no_hosted_domain_found_(false),
        is_account_signed_in_(false) {}
#if BUILDFLAG(IS_ANDROID)
  explicit MockChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      StringProvider sync_password_hash_provider,
      VerdictCacheManager* cache_manager,
      ChangePhishedCredentialsCallback add_phished_credentials,
      ChangePhishedCredentialsCallback remove_phished_credentials,
      std::unique_ptr<PasswordCheckupLauncherHelper> checkup_launcher)
      : ChromePasswordProtectionService(profile,
                                        ui_manager,
                                        sync_password_hash_provider,
                                        cache_manager,
                                        add_phished_credentials,
                                        remove_phished_credentials,
                                        std::move(checkup_launcher)),
        is_incognito_(false),
        is_extended_reporting_(false),
        is_syncing_(false),
        is_no_hosted_domain_found_(false),
        is_account_signed_in_(false) {}
#endif
  bool IsExtendedReporting() override { return is_extended_reporting_; }
  bool IsIncognito() override { return is_incognito_; }
  bool IsPrimaryAccountSyncingHistory() const override { return is_syncing_; }
  bool IsPrimaryAccountSignedIn() const override {
    return is_account_signed_in_;
  }

  AccountInfo GetAccountInfoForUsername(
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
  void SetAccountInfo(const std::string& username,
                      const std::string& hosted_domain) {
    AccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId("gaia");
    account_info.email = username;
    account_info.gaia = "gaia";
    account_info.hosted_domain = hosted_domain;
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

    password_store_ = base::WrapRefCounted(
        static_cast<password_manager::MockPasswordStoreInterface*>(
            ProfilePasswordStoreFactory::GetForProfile(
                profile(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));
    account_password_store_ = base::WrapRefCounted(
        static_cast<password_manager::MockPasswordStoreInterface*>(
            AccountPasswordStoreFactory::GetForProfile(
                profile(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));

    profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
    profile()->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger,
        PasswordProtectionTrigger::PHISHING_REUSE);
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, /*is_off_the_record=*/false,
        /*store_last_modified=*/false, /*restore_session=*/false,
        /*should_record_metrics=*/false);

    cache_manager_ = std::make_unique<VerdictCacheManager>(
        nullptr, content_setting_map_.get(), &test_pref_service_, nullptr);

    service_ = NewMockPasswordProtectionService();
    fake_user_event_service_ = static_cast<syncer::FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(browser_context(),
                                      GetFakeUserEventServiceFactory()));
#if !BUILDFLAG(IS_ANDROID)
    test_event_router_ =
        extensions::CreateAndUseTestEventRouter(browser_context());
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            browser_context(),
            base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter));
    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(browser_context(),
                            base::BindRepeating(&BuildRealtimeReportingClient));
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

#if !BUILDFLAG(IS_ANDROID)
    return std::make_unique<MockChromePasswordProtectionService>(
        profile(),
        new SafeBrowsingUIManager(
            std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
            std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
            GURL(chrome::kChromeUINewTabURL)),
        sync_password_hash_provider, cache_manager_.get(),
        mock_add_callback_.Get(), mock_remove_callback_.Get());
#else
    auto checkup_launcher =
        std::make_unique<MockPasswordCheckupLauncherHelper>();
    mock_checkup_launcher_ = checkup_launcher.get();
    return std::make_unique<MockChromePasswordProtectionService>(
        profile(),
        new SafeBrowsingUIManager(
            std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
            std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
            GURL(chrome::kChromeUINewTabURL)),
        sync_password_hash_provider, cache_manager_.get(),
        mock_add_callback_.Get(), mock_remove_callback_.Get(),
        std::move(checkup_launcher));
#endif
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        ProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            content::BrowserContext,
                            password_manager::MockPasswordStoreInterface>));
    // It's fine to override unconditionally, GetForProfile() will still return
    // null if account storage is disabled.
    // TODO(crbug.com/41489644): Remove the comment above when the account store
    // is always non-null.
    factories.emplace_back(
        AccountPasswordStoreFactory::GetInstance(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            content::BrowserContext,
                            password_manager::MockPasswordStoreInterface>));
    return factories;
  }

  syncer::FakeUserEventService* GetUserEventService() {
    return fake_user_event_service_;
  }

  void InitializeRequest(LoginReputationClientRequest::TriggerType trigger_type,
                         PasswordType reused_password_type) {
    std::vector<password_manager::MatchingReusedCredential> credentials = {
        {"somedomain.com"}};
    if (trigger_type == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
      request_ = new PasswordProtectionRequestContent(
          web_contents(), GURL(kPhishingURL), GURL(), GURL(),
          web_contents()->GetContentsMimeType(), kUserName,
          PasswordType::PASSWORD_TYPE_UNKNOWN, credentials, trigger_type, true,
          service_.get(), 0);
    } else {
      ASSERT_EQ(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                trigger_type);
      request_ = new PasswordProtectionRequestContent(
          web_contents(), GURL(kPhishingURL), GURL(), GURL(),
          web_contents()->GetContentsMimeType(), kUserName,
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
      LoginReputationClientResponse::VerdictType verdict_type,
      RequestOutcome request_outcome = RequestOutcome::SUCCEEDED) {
    std::unique_ptr<LoginReputationClientResponse> verdict =
        std::make_unique<LoginReputationClientResponse>();
    verdict->set_verdict_type(verdict_type);
    service_->RequestFinished(request_.get(), request_outcome,
                              std::move(verdict));
  }

  CoreAccountInfo SetPrimaryAccount(const std::string& email) {
    identity_test_env()->MakeAccountAvailable(email);
    return identity_test_env()->SetPrimaryAccount(
        email, signin::ConsentLevel::kSignin);
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
    return profile()
        ->GetPrefs()
        ->GetDict(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses)
        .size();
  }

  size_t GetNumberOfDeferredNavigations() {
    return request_ ? request_->deferred_navigations_.size() : 0u;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  std::unique_ptr<MockChromePasswordProtectionService> service_;
  scoped_refptr<PasswordProtectionRequestContent> request_;
  std::unique_ptr<LoginReputationClientResponse> verdict_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  raw_ptr<MockSecurityEventRecorder, DanglingUntriaged>
      security_event_recorder_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_;
  scoped_refptr<password_manager::MockPasswordStoreInterface>
      account_password_store_;
  // Owned by KeyedServiceFactory.
  raw_ptr<syncer::FakeUserEventService, DanglingUntriaged>
      fake_user_event_service_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<extensions::TestEventRouter, DanglingUntriaged> test_event_router_;
#endif
#if BUILDFLAG(IS_ANDROID)
  raw_ptr<MockPasswordCheckupLauncherHelper> mock_checkup_launcher_;
#endif
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  ScopedTestingLocalState local_state_;
  base::MockCallback<
      ChromePasswordProtectionService::ChangePhishedCredentialsCallback>
      mock_add_callback_;
  base::MockCallback<
      ChromePasswordProtectionService::ChangePhishedCredentialsCallback>
      mock_remove_callback_;
  base::test::ScopedFeatureList feature_list_;
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

  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_PROTECTION_OFF);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(false /*incognito*/, true /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  service_->ConfigService(false /*incognito*/, false /*SBER*/);
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PHISHING_REUSE);

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
  EXPECT_FALSE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));

  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  EXPECT_TRUE(service_->IsPingingEnabled(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPingingIsSkippedIfMatchEnterpriseAllowlist) {
  ASSERT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kSafeBrowsingAllowlistDomains));

  // If there's no allowlist, IsURLAllowlistedForPasswordEntry(_) should
  // return false.
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  // Verify if match enterprise allowlist.
  base::Value::List allowlist;
  allowlist.Append("mydomain.com");
  allowlist.Append("mydomain.net");
  profile()->GetPrefs()->SetList(prefs::kSafeBrowsingAllowlistDomains,
                                 std::move(allowlist));
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  // Verify if matches enterprise change password url.
  profile()->GetPrefs()->ClearPref(prefs::kSafeBrowsingAllowlistDomains);
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));

  profile()->GetPrefs()->SetString(prefs::kPasswordProtectionChangePasswordURL,
                                   "https://mydomain.com/change_password.html");
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://mydomain.com/change_password.html#ref?user_name=alice")));

  // Verify if matches enterprise login url.
  profile()->GetPrefs()->ClearPref(prefs::kSafeBrowsingAllowlistDomains);
  profile()->GetPrefs()->ClearPref(prefs::kPasswordProtectionChangePasswordURL);
  EXPECT_FALSE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://www.mydomain.com")));
  base::Value::List login_urls;
  login_urls.Append("https://mydomain.com/login.html");
  profile()->GetPrefs()->SetList(prefs::kPasswordProtectionLoginURLs,
                                 std::move(login_urls));
  EXPECT_TRUE(service_->IsURLAllowlistedForPasswordEntry(
      GURL("https://mydomain.com/login.html#ref?user_name=alice")));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyPersistPhishedSavedPasswordCredential) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test"}, {"http://2.example.com"}};

  EXPECT_CALL(mock_add_callback_, Run(password_store_.get(), credentials[0]));
  EXPECT_CALL(mock_add_callback_, Run(password_store_.get(), credentials[1]));
  service_->PersistPhishedSavedPasswordCredential(credentials);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyRemovePhishedSavedPasswordCredential) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username1"},
      {"http://2.example.test", u"username2"}};

  EXPECT_CALL(mock_remove_callback_,
              Run(password_store_.get(), credentials[0]));
  EXPECT_CALL(mock_remove_callback_,
              Run(password_store_.get(), credentials[1]));

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// prefs::kEnterpriseCustomLabel is only registered on Windows, Mac, and Linux.
TEST_F(ChromePasswordProtectionServiceTest, VerifyGetOrganizationPrefEmpty) {
  feature_list_.InitWithFeatures(
      {safe_browsing::kEnterprisePasswordReuseUiRefresh}, {});
  ReusedPasswordAccountType reused_password_type;
  EXPECT_TRUE((service_->GetOrganizationName(reused_password_type).empty()));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetOrganizationPrefNonEmpty) {
  feature_list_.InitWithFeatures(
      {safe_browsing::kEnterprisePasswordReuseUiRefresh}, {});
  profile()->GetPrefs()->SetString(prefs::kEnterpriseCustomLabel,
                                   "Mini Corp Ltd");
  ReusedPasswordAccountType reused_password_type;
  EXPECT_EQ("Mini Corp Ltd",
            service_->GetOrganizationName(reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetOrganizationTypeGmail) {
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  reused_password_type.set_is_account_syncing(true);
  EXPECT_TRUE(service_->GetOrganizationName(reused_password_type).empty());
  EXPECT_EQ("", service_->GetOrganizationName(reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetOrganizationTypeGSuite) {
  profile()->GetPrefs()->SetString(prefs::kEnterpriseCustomLabel,
                                   "example.com");
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount("example.com", account_info);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  reused_password_type.set_is_account_syncing(true);
  EXPECT_EQ("example.com", service_->GetOrganizationName(reused_password_type));
}
#endif

TEST_F(ChromePasswordProtectionServiceTest, VerifyUpdateSecurityState) {
  using enum SBThreatType;

  GURL url("http://password_reuse_url.com");
  NavigateAndCommit(url);
  SBThreatType current_threat_type = SBThreatType::SB_THREAT_TYPE_UNUSED;
  ASSERT_FALSE(service_->ui_manager()->IsUrlAllowlistedOrPendingForWebContents(
      url, web_contents()->GetController().GetLastCommittedEntry(),
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
  ASSERT_TRUE(service_->ui_manager()->IsUrlAllowlistedOrPendingForWebContents(
      url, web_contents()->GetController().GetLastCommittedEntry(),
      web_contents(), false, &current_threat_type));
  EXPECT_EQ(SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE, current_threat_type);

  service_->UpdateSecurityState(SB_THREAT_TYPE_SAFE, reused_password_type,
                                web_contents());
  current_threat_type = SB_THREAT_TYPE_UNUSED;
  service_->ui_manager()->IsUrlAllowlistedOrPendingForWebContents(
      url, web_contents()->GetController().GetLastCommittedEntry(),
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
      web_contents(), RequestOutcome::MATCHED_ALLOWLIST,
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
  identity_test_env()->SetPrimaryAccount(kTestEmail,
                                         signin::ConsentLevel::kSignin);
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
#if !BUILDFLAG(IS_ANDROID)
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
  base::TimeDelta delay = base::Days(13);
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

  {
    auto response = std::make_unique<LoginReputationClientResponse>();
    response->set_verdict_token("token3");
    response->set_verdict_type(LoginReputationClientResponse::PHISHING);
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
    EXPECT_EQ(PasswordReuseLookup::PHISHING, reuse_lookup.verdict()) << t;
    EXPECT_EQ("token3", reuse_lookup.verdict_token()) << t;
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
  std::unique_ptr<PasswordProtectionCommitDeferringCondition> condition =
      service_->MaybeCreateCommitDeferringCondition(test_handle);
  EXPECT_EQ(nullptr, condition);
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
  std::unique_ptr<PasswordProtectionCommitDeferringCondition> condition =
      service_->MaybeCreateCommitDeferringCondition(test_handle);
  ASSERT_NE(nullptr, condition);

  // Verify navigation get deferred.
  EXPECT_EQ(content::CommitDeferringCondition::Result::kDefer,
            condition->WillCommitNavigation(base::BindLambdaForTesting(
                [&]() { was_navigation_resumed = true; })));
  base::RunLoop().RunUntilIdle();

  // Simulate receiving a SAFE verdict.
  SimulateRequestFinished(LoginReputationClientResponse::SAFE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(true, was_navigation_resumed);
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyNavigationDuringModalWarningDeferred) {
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
  std::unique_ptr<PasswordProtectionCommitDeferringCondition> condition =
      service_->MaybeCreateCommitDeferringCondition(test_handle);

  // Verify that navigation gets deferred.
  EXPECT_EQ(content::CommitDeferringCondition::Result::kDefer,
            condition->WillCommitNavigation(base::DoNothing()));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyCommitDeferringConditionRemovedWhenNavigationHandleIsGone) {
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
  std::unique_ptr<PasswordProtectionCommitDeferringCondition> condition =
      service_->MaybeCreateCommitDeferringCondition(test_handle);

  // Verify navigation gets deferred.
  EXPECT_EQ(content::CommitDeferringCondition::Result::kDefer,
            condition->WillCommitNavigation(base::DoNothing()));

  EXPECT_EQ(1u, GetNumberOfDeferredNavigations());

  // Simulate the deletion of the PasswordProtectionCommitDeferringCondition.
  condition.reset();
  base::RunLoop().RunUntilIdle();

  // Expect no navigation condition kept by |request_|.
  EXPECT_EQ(0u, GetNumberOfDeferredNavigations());

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

  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
  update->Set(Origin::Create(url_a).Serialize(),
              base::Value("navigation_id_a"));
  update->Set(Origin::Create(url_b).Serialize(),
              base::Value("navigation_id_b"));
  update->Set(Origin::Create(url_c).Serialize(),
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
#if !BUILDFLAG(IS_ANDROID)
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

TEST_F(
    ChromePasswordProtectionServiceTest,
    VerifyTriggerOnPolicySpecifiedPasswordReuseDetectedForEnterprisePasswordWithAlertMode) {
  TestExtensionEventObserver event_observer(test_event_router_);
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  NavigateAndCommit(GURL(kPasswordReuseURL));
  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordType::ENTERPRISE_PASSWORD,
                 /*is_warning_showing=*/false);
  SimulateRequestFinished(LoginReputationClientResponse::SAFE,
                          RequestOutcome::PASSWORD_ALERT_MODE);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
}

TEST_F(
    ChromePasswordProtectionServiceTest,
    VerifyTriggerOnPolicySpecifiedPasswordReuseDetectedForEnterprisePasswordOnChromeExtension) {
  TestExtensionEventObserver event_observer(test_event_router_);
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  service_->MaybeStartProtectedPasswordEntryRequest(
      web_contents(),
      /*main_frame_url=*/GURL("chrome-extension://some-fab-extension"),
      /*username=*/"enterprise_user", PasswordType::ENTERPRISE_PASSWORD,
      /*matching_reused_credentials=*/{}, /*password_field_exists=*/false);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyTriggerOnPolicySpecifiedPasswordReuseDetectedForGsuiteUser) {
  TestExtensionEventObserver event_observer(test_event_router_);
  CoreAccountInfo account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount("example.com", account_info);
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  NavigateAndCommit(GURL(kPasswordReuseURL));

  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordType::SAVED_PASSWORD,
                 /*is_warning_showing=*/false);
  service_->MaybeReportPasswordReuseDetected(
      web_contents()->GetLastCommittedURL(), kUserName,
      PasswordType::ENTERPRISE_PASSWORD,
      /*is_phishing_url =*/true,
      /*warning_shown =*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
  const auto captured_args =
      std::move(event_observer.PassEventArgs().GetList()[0].GetDict());
  EXPECT_EQ(kPasswordReuseURL, *captured_args.FindString("url"));
  EXPECT_EQ(kUserName, *captured_args.FindString("userName"));
  EXPECT_TRUE(*captured_args.FindBool("isPhishingUrl"));

  // If the reused password is possibly a consumer account password, no event
  // should be sent.
  service_->SetAccountInfo(kUserName, /*hosted_domain=*/"");
  service_->SetIsAccountSignedIn(true);
  service_->MaybeReportPasswordReuseDetected(
      request_->main_frame_url(), kUserName, PasswordType::OTHER_GAIA_PASSWORD,
      /*is_phishing_url =*/true,
      /*warning_shown =*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If the reused password is not Enterprise password but the account is
  // GSuite, event should be sent.
  service_->SetAccountInfo(kUserName, "example.com");
  service_->MaybeReportPasswordReuseDetected(
      request_->main_frame_url(), kUserName, PasswordType::OTHER_GAIA_PASSWORD,
      /*is_phishing_url =*/true,
      /*warning_shown =*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If no password is used , no event should be sent.
  service_->MaybeReportPasswordReuseDetected(
      request_->main_frame_url(), kUserName,
      PasswordType::PASSWORD_TYPE_UNKNOWN,
      /*is_phishing_url =*/true, /*warning_shown =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If user is in incognito mode, no event should be sent.
  service_->ConfigService(true /*incognito*/, false /*SBER*/);
  service_->MaybeReportPasswordReuseDetected(
      request_->main_frame_url(), kUserName, PasswordType::ENTERPRISE_PASSWORD,
      /*is_phishing_url =*/true,
      /*warning_shown =*/true);
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

  PrepareRequest(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 PasswordType::SAVED_PASSWORD,
                 /*is_warning_showing=*/false);
  service_->MaybeReportPasswordReuseDetected(
      request_->main_frame_url(), kUserName, PasswordType::ENTERPRISE_PASSWORD,
      /*is_phishing_url =*/true,
      /*warning_shown =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If user is a Gmail user and not an enterprise password is used , no event
  // should be sent.
  service_->MaybeReportPasswordReuseDetected(
      request_->main_frame_url(), kUserName, PasswordType::OTHER_GAIA_PASSWORD,
      /*is_phishing_url =*/true,
      /*warning_shown =*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));

  // If user is a Gmail user and no password is used , no event should be sent.
  service_->MaybeReportPasswordReuseDetected(
      request_->main_frame_url(), kUserName,
      PasswordType::PASSWORD_TYPE_UNKNOWN,
      /*is_phishing_url =*/true, /*warning_shown*/ true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnPolicySpecifiedPasswordReuseDetected::kEventName));
}
#endif

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetWarningDetailTextSaved) {
  std::u16string warning_text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);
  EXPECT_EQ(warning_text, service_->GetWarningDetailText(reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyGetWarningDetailTextEnterprise) {
  std::u16string warning_text_non_sync = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SIGNED_IN_NON_SYNC);
  std::u16string generic_enterprise_warning_text = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE);
  std::u16string warning_text_with_org_name = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE_WITH_ORG_NAME,
      u"example.com");
  std::u16string warning_text_sync =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SYNC);

  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  reused_password_type.set_is_account_syncing(true);
  EXPECT_EQ(warning_text_sync,
            service_->GetWarningDetailText(reused_password_type));
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  reused_password_type.set_is_account_syncing(false);
  EXPECT_EQ(generic_enterprise_warning_text,
            service_->GetWarningDetailText(reused_password_type));

  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(warning_text_non_sync,
            service_->GetWarningDetailText(reused_password_type));

  reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  reused_password_type.set_is_account_syncing(true);
  CoreAccountInfo core_account_info = SetPrimaryAccount(kTestEmail);
  SetUpSyncAccount(std::string("example.com"), core_account_info);
  EXPECT_EQ(warning_text_with_org_name,
            service_->GetWarningDetailText(reused_password_type));
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_EQ(generic_enterprise_warning_text,
            service_->GetWarningDetailText(reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyGetWarningDetailTextGmail) {
  std::u16string warning_text_non_sync = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SIGNED_IN_NON_SYNC);
  std::u16string warning_text_sync =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SYNC);
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  EXPECT_EQ(warning_text_non_sync,
            service_->GetWarningDetailText(reused_password_type));
  reused_password_type.set_is_account_syncing(true);
  EXPECT_EQ(warning_text_sync,
            service_->GetWarningDetailText(reused_password_type));
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyCanShowInterstitial) {
  // Do not show interstitial if policy not set for password_alert.
  ASSERT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kSafeBrowsingAllowlistDomains));
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
  // Show interstitial if user is a Enterprise user and the policy is set to
  // password_alert.
  reused_password_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  reused_password_type.set_is_account_syncing(false);
  profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                    PASSWORD_REUSE);
  EXPECT_TRUE(service_->CanShowInterstitial(reused_password_type, trigger_url));

  // Add |trigger_url| to enterprise allowlist.
  base::Value::List allowlisted_domains;
  allowlisted_domains.Append(trigger_url.host());
  profile()->GetPrefs()->SetList(prefs::kSafeBrowsingAllowlistDomains,
                                 std::move(allowlisted_domains));
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
    reused_password_type.set_account_type(
        ReusedPasswordAccountType::SAVED_PASSWORD);
    EXPECT_EQ(RequestOutcome::TURNED_OFF_BY_ADMIN,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                  GURL("about:blank"), reused_password_type));
  }
  {
    // Allowlisted by policy.
    ReusedPasswordAccountType reused_password_type;
    service_->ConfigService(false /*incognito*/, false /*SBER*/);
    reused_password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
    profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                      PHISHING_REUSE);
    base::Value::List allowlist;
    allowlist.Append("mydomain.com");
    allowlist.Append("mydomain.net");
    profile()->GetPrefs()->SetList(prefs::kSafeBrowsingAllowlistDomains,
                                   std::move(allowlist));
    EXPECT_EQ(RequestOutcome::MATCHED_ENTERPRISE_ALLOWLIST,
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
    profile()->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                      PASSWORD_PROTECTION_OFF);
  }
  {
    // Internal URL
    ReusedPasswordAccountType reused_password_type;
    service_->ConfigService(false /*incognito*/, true /*SBER*/);
    EXPECT_EQ(RequestOutcome::URL_NOT_VALID_FOR_REPUTATION_COMPUTING,
              service_->GetPingNotSentReason(
                  LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                  GURL("http://192.168.1.1/"), reused_password_type));
  }
}

TEST_F(ChromePasswordProtectionServiceTest, VerifyPageLoadToken) {
  auto request = std::make_unique<LoginReputationClientRequest>();
  service_->FillUserPopulation(GURL("https:www.example.com/"), request.get());
  ASSERT_EQ(1, request->population().page_load_tokens_size());
}

namespace {

class ChromePasswordProtectionServiceWithAccountPasswordStoreTest
    : public ChromePasswordProtectionServiceTest {
 public:
  ChromePasswordProtectionServiceWithAccountPasswordStoreTest() {
#if BUILDFLAG(IS_ANDROID)
    // Override the GMS version to be big enough for local UPM support, so these
    // tests still pass in bots with an outdated version.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(password_manager::GetLocalUpmMinGmsVersion()));
#endif
  }
};

TEST_F(ChromePasswordProtectionServiceWithAccountPasswordStoreTest,
       VerifyPersistPhishedSavedPasswordCredential) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {.signon_realm = "http://example.test",
       .in_store = password_manager::PasswordForm::Store::kAccountStore},
      {.signon_realm = "http://2.example.test",
       .in_store = password_manager::PasswordForm::Store::kAccountStore}};

  EXPECT_CALL(mock_add_callback_,
              Run(account_password_store_.get(), credentials[0]));
  EXPECT_CALL(mock_add_callback_,
              Run(account_password_store_.get(), credentials[1]));
  service_->PersistPhishedSavedPasswordCredential(credentials);
}

TEST_F(ChromePasswordProtectionServiceWithAccountPasswordStoreTest,
       VerifyRemovePhishedSavedPasswordCredential) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username1",
       password_manager::PasswordForm::Store::kAccountStore},
      {"http://2.example.test", u"username2",
       password_manager::PasswordForm::Store::kAccountStore}};

  EXPECT_CALL(mock_remove_callback_,
              Run(account_password_store_.get(), credentials[0]));
  EXPECT_CALL(mock_remove_callback_,
              Run(account_password_store_.get(), credentials[1]));

  service_->RemovePhishedSavedPasswordCredential(credentials);
}

#if BUILDFLAG(IS_ANDROID)
class PasswordCheckupWithPhishGuardTest
    : public ChromePasswordProtectionServiceTest {
 protected:
  void SetUpSyncService(bool is_syncing_passwords) {
    // Setting up the syncing account.
    CoreAccountInfo account;
    account.email = profile()->GetProfileUserName();
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&CreateTestSyncService)));
    sync_service_->SetSignedIn(signin::ConsentLevel::kSync, account);
    ASSERT_TRUE(sync_service_->IsSyncFeatureEnabled());
    sync_service_->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPasswords, is_syncing_passwords);
  }

  // Simulates clicking "Change Password" button on the modal dialog.
  void SimulateChangePasswordDialogAction(bool is_syncing) {
    ReusedPasswordAccountType password_account_type;
    password_account_type.set_account_type(
        ReusedPasswordAccountType::SAVED_PASSWORD);
    password_account_type.set_is_account_syncing(is_syncing);

    service_->OnUserAction(
        web_contents(), password_account_type, RequestOutcome::UNKNOWN,
        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED, "unused_token",
        WarningUIType::MODAL_DIALOG, WarningAction::CHANGE_PASSWORD);
  }

  raw_ptr<syncer::TestSyncService> sync_service_ = nullptr;
};

class PasswordCheckupWithPhishGuardAfterPasswordStoreSplitAndroidTest
    : public PasswordCheckupWithPhishGuardTest {
 public:
  void SetUp() override {
    // Override the GMS version to be big enough for local UPM support, so these
    // tests still pass in bots with an outdated version.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(password_manager::GetLocalUpmMinGmsVersion()));
    PasswordCheckupWithPhishGuardTest::SetUp();
  }
};

TEST_F(PasswordCheckupWithPhishGuardAfterPasswordStoreSplitAndroidTest,
       VerifyPhishGuardDialogOpensPasswordCheckupForAccountStoreSyncing) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username",
       password_manager::PasswordForm::Store::kAccountStore}};
  service_->set_saved_passwords_matching_reused_credentials(credentials);

  SetUpSyncService(/*is_syncing_passwords=*/true);

  EXPECT_CALL(
      *mock_checkup_launcher_,
      LaunchCheckupOnDevice(
          _, profile(), web_contents()->GetTopLevelNativeWindow(),
          password_manager::PasswordCheckReferrerAndroid::kPhishedWarningDialog,
          TestingProfile::kDefaultProfileUserName));

  SimulateChangePasswordDialogAction(/*is_syncing=*/true);
}

TEST_F(PasswordCheckupWithPhishGuardAfterPasswordStoreSplitAndroidTest,
       VerifyPhishGuardDialogOpensPasswordCheckupForProfileStoreSyncing) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username",
       password_manager::PasswordForm::Store::kProfileStore}};
  service_->set_saved_passwords_matching_reused_credentials(credentials);

  SetUpSyncService(/*is_syncing_passwords=*/true);

  EXPECT_CALL(
      *mock_checkup_launcher_,
      LaunchCheckupOnDevice(
          _, profile(), web_contents()->GetTopLevelNativeWindow(),
          password_manager::PasswordCheckReferrerAndroid::kPhishedWarningDialog,
          /*account=*/""));

  SimulateChangePasswordDialogAction(/*is_syncing=*/true);
}

TEST_F(PasswordCheckupWithPhishGuardAfterPasswordStoreSplitAndroidTest,
       VerifyPhishGuardDialogOpensPasswordCheckupForProfileStoreNotSyncing) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username",
       password_manager::PasswordForm::Store::kProfileStore}};
  service_->set_saved_passwords_matching_reused_credentials(credentials);

  SetUpSyncService(/*is_syncing_passwords=*/false);

  EXPECT_CALL(
      *mock_checkup_launcher_,
      LaunchCheckupOnDevice(
          _, profile(), web_contents()->GetTopLevelNativeWindow(),
          password_manager::PasswordCheckReferrerAndroid::kPhishedWarningDialog,
          /*account=*/""));

  SimulateChangePasswordDialogAction(/*is_syncing=*/false);
}

TEST_F(PasswordCheckupWithPhishGuardAfterPasswordStoreSplitAndroidTest,
       VerifyPhishGuardDialogOpensSafetyCheckMenuForBothStoresSyncing) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username",
       password_manager::PasswordForm::Store::kProfileStore},
      {"http://2.example.test", u"username",
       password_manager::PasswordForm::Store::kAccountStore}};
  service_->set_saved_passwords_matching_reused_credentials(credentials);

  SetUpSyncService(/*is_syncing_passwords=*/true);

  EXPECT_CALL(*mock_checkup_launcher_,
              LaunchSafetyCheck(_, web_contents()->GetTopLevelNativeWindow()));

  SimulateChangePasswordDialogAction(/*is_syncing=*/true);
}

class PasswordCheckupWithPhishGuardUPMBeforeStoreSplitAndroidTest
    : public PasswordCheckupWithPhishGuardTest {
 public:
  void SetUp() override {
    // Force split stores to be off by faking an outdated GmsCore version.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test("0");
    PasswordCheckupWithPhishGuardTest::SetUp();
  }
};

TEST_F(
    PasswordCheckupWithPhishGuardUPMBeforeStoreSplitAndroidTest,
    VerifyPhishGuardDialogOpensPasswordCheckupEmptyAccountForNonSyncingUser) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username",
       password_manager::PasswordForm::Store::kProfileStore}};
  service_->set_saved_passwords_matching_reused_credentials(credentials);

  SetUpSyncService(/*is_syncing_passwords=*/false);

  EXPECT_CALL(
      *mock_checkup_launcher_,
      LaunchCheckupOnDevice(
          _, profile(), web_contents()->GetTopLevelNativeWindow(),
          password_manager::PasswordCheckReferrerAndroid::kPhishedWarningDialog,
          /*account=*/""));

  SimulateChangePasswordDialogAction(/*is_syncing=*/false);
}

TEST_F(PasswordCheckupWithPhishGuardUPMBeforeStoreSplitAndroidTest,
       VerifyPhishGuardDialogOpensPasswordCheckupWithAnAccountForSyncingUser) {
  service_->ConfigService(/*is_incognito=*/false,
                          /*is_extended_reporting=*/true);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"http://example.test", u"username",
       password_manager::PasswordForm::Store::kProfileStore}};
  service_->set_saved_passwords_matching_reused_credentials(credentials);

  SetUpSyncService(/*is_syncing_passwords=*/true);

  EXPECT_CALL(
      *mock_checkup_launcher_,
      LaunchCheckupOnDevice(
          _, profile(), web_contents()->GetTopLevelNativeWindow(),
          password_manager::PasswordCheckReferrerAndroid::kPhishedWarningDialog,
          TestingProfile::kDefaultProfileUserName));

  SimulateChangePasswordDialogAction(/*is_syncing=*/true);
}
#endif

}  // namespace

}  // namespace safe_browsing
