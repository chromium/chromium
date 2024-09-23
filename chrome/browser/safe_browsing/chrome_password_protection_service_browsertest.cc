// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/ui/password_check_referrer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_test_util.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/user_names.h"
#include "components/variations/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

using password_manager::FakePasswordStoreBackend;
using password_manager::PasswordForm;
using password_manager::PasswordStoreInterface;
using ::testing::_;
using ::testing::ElementsAre;

namespace {

const char kLoginPageUrl[] = "/safe_browsing/login_page.html";
const char kChangePasswordUrl[] = "/safe_browsing/change_password_page.html";

PasswordForm CreatePasswordFormWithPhishedEntry(std::string signon_realm,
                                                std::u16string username) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.username_value = username;
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;
  form.password_issues = {
      {password_manager::InsecureType::kPhished,
       password_manager::InsecurityMetadata(
           base::Time::FromTimeT(1), password_manager::IsMuted(false),
           password_manager::TriggerBackendNotification(false))}};

  return form;
}

void AddFormToStore(PasswordStoreInterface* password_store,
                    const PasswordForm& form) {
  password_store->AddLogin(form);
  base::RunLoop().RunUntilIdle();
  FakePasswordStoreBackend* fake_backend =
      static_cast<FakePasswordStoreBackend*>(
          password_store->GetBackendForTesting());
  ASSERT_THAT(fake_backend->stored_passwords().at(form.signon_realm),
              ElementsAre(form));
}

}  // namespace

namespace safe_browsing {

class ChromePasswordProtectionServiceBrowserTest : public InProcessBrowserTest {
 public:
  ChromePasswordProtectionServiceBrowserTest() {}

  ChromePasswordProtectionServiceBrowserTest(
      const ChromePasswordProtectionServiceBrowserTest&) = delete;
  ChromePasswordProtectionServiceBrowserTest& operator=(
      const ChromePasswordProtectionServiceBrowserTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void TearDownOnMainThread() override { identity_test_env_adaptor_.reset(); }

  ChromePasswordProtectionService* GetService(bool is_incognito) {
    return ChromePasswordProtectionService::GetPasswordProtectionService(
        is_incognito ? browser()->profile()->GetPrimaryOTRProfile(
                           /*create_if_needed=*/true)
                     : browser()->profile());
  }

  void SimulateGaiaPasswordChange(const std::string& new_password) {
    password_manager::PasswordReuseManager* reuse_manager =
        PasswordReuseManagerFactory::GetForProfile(browser()->profile());
    reuse_manager->SaveGaiaPasswordHash(
        user_manager::kStubUserEmail, base::UTF8ToUTF16(new_password),
        /*is_primary_account=*/true,
        password_manager::metrics_util::GaiaPasswordHashChange::
            CHANGED_IN_CONTENT_AREA);
  }

  void SimulateGaiaPasswordChanged(ChromePasswordProtectionService* service,
                                   const std::string& username,
                                   bool is_other_gaia_password) {
    service->OnGaiaPasswordChanged(username, is_other_gaia_password);
  }

  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(web_contents);
    return helper->GetSecurityLevel();
  }

  std::unique_ptr<security_state::VisibleSecurityState> GetVisibleSecurityState(
      content::WebContents* web_contents) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(web_contents);
    return helper->GetVisibleSecurityState();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ChromePasswordProtectionServiceBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  // Makes user signed-in with the stub account's email and |hosted_domain|.
  void SetUpPrimaryAccountWithHostedDomain(const std::string& hosted_domain) {
    // Ensure that the stub user is signed in.

    CoreAccountInfo account_info =
        identity_test_env()->MakePrimaryAccountAvailable(
            user_manager::kStubUserEmail, signin::ConsentLevel::kSync);

    ASSERT_EQ(account_info.email, user_manager::kStubUserEmail);

    identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        hosted_domain, "full_name", "given_name", "locale",
        "http://picture.example.com/picture.jpg");
  }

  void ConfigureEnterprisePasswordProtection(
      bool is_gsuite,
      PasswordProtectionTrigger trigger_type) {
    if (is_gsuite)
      SetUpPrimaryAccountWithHostedDomain("example.com");
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger, trigger_type);
    browser()->profile()->GetPrefs()->SetString(
        prefs::kPasswordProtectionChangePasswordURL,
        embedded_test_server()->GetURL(kChangePasswordUrl).spec());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyIsInExcludedCountry) {
  const std::string non_excluded_countries[] = {"be", "br", "ca", "de", "es",
                                                "fr", "ie", "in", "jp", "nl",
                                                "ru", "se", "us"};
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  for (auto country : non_excluded_countries) {
    g_browser_process->local_state()->SetString(
        variations::prefs::kVariationsCountry, country);
    EXPECT_FALSE(service->IsInExcludedCountry());
  }
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "cn");
  EXPECT_TRUE(service->IsInExcludedCountry());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       SuccessfullyChangeSignInPassword) {
  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Initialize and verify initial state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  account_type.set_is_account_syncing(true);
  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(
      security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
      GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking "Change Password" button on the modal dialog.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::MODAL_DIALOG,
                        WarningAction::CHANGE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  // Verify myaccount.google.com or Google signin page should be opened in a
  // new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetVisibleURL()
                  .DomainIs("google.com"));

  // Simulates user finished changing password.
  SimulateGaiaPasswordChanged(service, user_manager::kStubUserEmail,
                              /*is_other_gaia_password=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       SuccessfullyShowWarningIncognito) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Initialize and verify initial state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::ENTERPRISE_PASSWORD));
  ASSERT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);

  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
class ChromePasswordProtectionServiceBrowserWithFakeBackendPasswordStoreTest
    : public ChromePasswordProtectionServiceBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    ChromePasswordProtectionServiceBrowserTest::
        SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating(
                          &password_manager::BuildPasswordStoreWithFakeBackend<
                              content::BrowserContext>));
                }));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceBrowserWithFakeBackendPasswordStoreTest,
    SavedPassword) {
  base::HistogramTester histograms;
  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Initialize and verify initial state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::SAVED_PASSWORD));
  ASSERT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::SAVED_PASSWORD);
  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking "Check Passwords" button on the modal dialog.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::MODAL_DIALOG,
                        WarningAction::CHANGE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  // Verify chrome://settings/passwords/check page should be opened in a new
  // foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(
      GURL(chrome::kChromeUIPasswordManagerCheckupURL),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  histograms.ExpectUniqueSample(
      password_manager::kPasswordCheckReferrerHistogram,
      password_manager::PasswordCheckReferrer::kPhishGuardDialog, 1);

  // Simulate removing the compromised credentials on mark site as legitimate
  // action.
  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);

  // In order to test removal, we need to make sure it was added first.
  const std::string kSignonRealm = "https://example.test";
  const std::u16string kUsername = u"username1";
  password_manager::PasswordForm form =
      CreatePasswordFormWithPhishedEntry(kSignonRealm, kUsername);
  AddFormToStore(password_store.get(), form);

  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {kSignonRealm, kUsername}};

  service->set_saved_passwords_matching_reused_credentials({credentials});

  // Simulates clicking on "Mark site legitimate". Site is no longer dangerous.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::PAGE_INFO,
                        WarningAction::MARK_AS_LEGITIMATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::SAVED_PASSWORD));
  EXPECT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
  FakePasswordStoreBackend* fake_backend =
      static_cast<FakePasswordStoreBackend*>(
          password_store->GetBackendForTesting());
  EXPECT_TRUE(fake_backend->stored_passwords()
                  .at(kSignonRealm)
                  .at(0)
                  .password_issues.empty());
}
#endif

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       MarkSiteAsLegitimate) {
  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Initialize and verify initial state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Shows modal dialog on current web_contents.
  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  account_type.set_is_account_syncing(true);
  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(
      security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
      GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking "Ignore" button on the modal dialog.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::MODAL_DIALOG,
                        WarningAction::IGNORE_WARNING);
  base::RunLoop().RunUntilIdle();
  // No new tab opens. Security info doesn't change.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(
      security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
      GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking on "Mark site legitimate". Site is no longer dangerous.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::PAGE_INFO,
                        WarningAction::MARK_AS_LEGITIMATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  EXPECT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OpenChromeSettingsViaPageInfo) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  account_type.set_is_account_syncing(true);
  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
  // Simulates clicking "Ignore" to close dialog.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::MODAL_DIALOG,
                        WarningAction::IGNORE_WARNING);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(
      security_state::MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
      GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking on "Change Password" in the page info bubble.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::PAGE_INFO,
                        WarningAction::CHANGE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  // Verify myaccount.google.com or Google signin page should be opened in a
  // new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetVisibleURL()
                  .DomainIs("google.com"));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyUnhandledPasswordReuse) {
  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  // Prepare sync account will trigger a password change.
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  ASSERT_TRUE(service);
  Profile* profile = browser()->profile();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));
  ASSERT_TRUE(profile->GetPrefs()
                  ->GetDict(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses)
                  .empty());

  base::HistogramTester histograms;
  // Shows modal dialog on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  account_type.set_is_account_syncing(true);
  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, profile->GetPrefs()
                    ->GetDict(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses)
                    .size());

  // Opens a new browser window.
  Browser* browser2 = CreateBrowser(profile);
  // Shows modal dialog on this new web_contents.
  content::WebContents* new_web_contents =
      browser2->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, GURL("data:text/html,<html></html>")));
  scoped_refptr<PasswordProtectionRequest> new_request =
      CreateDummyRequest(new_web_contents);
  service->ShowModalWarning(
      new_request.get(),
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED, "unused_token",
      account_type);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, profile->GetPrefs()
                    ->GetDict(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses)
                    .size());

  // Simulates a Gaia password change.
  SimulateGaiaPasswordChanged(service, user_manager::kStubUserEmail,
                              /*is_other_password=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, profile->GetPrefs()
                    ->GetDict(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyCheckGaiaPasswordChange) {
  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  Profile* profile = browser()->profile();
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  // Configures initial password to "password_1";
  password_manager::PasswordReuseManager* reuse_manager =
      PasswordReuseManagerFactory::GetForProfile(browser()->profile());
  reuse_manager->SaveGaiaPasswordHash(
      user_manager::kStubUserEmail, u"password_1",
      /*is_primary_account=*/true,
      password_manager::metrics_util::GaiaPasswordHashChange::
          CHANGED_IN_CONTENT_AREA);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/")));

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  account_type.set_is_account_syncing(true);

  // Shows modal dialog on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, profile->GetPrefs()
                    ->GetDict(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses)
                    .size());

  // Save the same password will not trigger OnGaiaPasswordChanged(), thus no
  // change to size of unhandled_password_reuses().
  SimulateGaiaPasswordChange("password_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, profile->GetPrefs()
                    ->GetDict(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses)
                    .size());
  // Save a different password will clear unhandled_password_reuses().
  SimulateGaiaPasswordChange("password_2");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, profile->GetPrefs()
                    ->GetDict(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       ChromeEnterprisePasswordAlertMode) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PASSWORD_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  service->set_reused_password_account_type_for_last_shown_warning(
      account_type);

  base::HistogramTester histograms;
  // Shows interstitial on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowInterstitial(web_contents, account_type);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  // Clicks on "Reset Password" button.
  std::string script =
      "var node = document.getElementById('reset-password-button'); \n"
      "node.click();";
  ASSERT_TRUE(content::ExecJs(new_web_contents, script));
  content::TestNavigationObserver observer1(new_web_contents,
                                            /*number_of_navigations=*/1);
  observer1.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL(kChangePasswordUrl),
            new_web_contents->GetLastCommittedURL());
  EXPECT_THAT(histograms.GetAllSamples("PasswordProtection.InterstitialAction."
                                       "NonGaiaEnterprisePasswordEntry"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       EnterprisePhishingReuseMode) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));
  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);

  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
  // Enterprise password reuse should not trigger warning in Chrome settings UI.
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::ENTERPRISE_PASSWORD));

  // Security info should be properly updated.
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking "Change Password" button on the modal dialog.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::MODAL_DIALOG,
                        WarningAction::CHANGE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Enterprise change password page should be opened in a new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(embedded_test_server()->GetURL(kChangePasswordUrl),
            new_web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       EnterprisePhishingReuseMarkSiteAsLegitimate) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);

  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking on "Mark site legitimate". Site is no longer dangerous.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::PAGE_INFO,
                        WarningAction::MARK_AS_LEGITIMATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::ENTERPRISE_PASSWORD));
  EXPECT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       EnterprisePhishingReuseOpenChromeSettingsViaPageInfo) {
  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  scoped_refptr<PasswordProtectionRequest> request =
      CreateDummyRequest(web_contents);
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(
      request.get(), LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", account_type);
  base::RunLoop().RunUntilIdle();

  // Simulates clicking on "Change Password" in the page info bubble.
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::PAGE_INFO,
                        WarningAction::CHANGE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Enterprise change password page should be opened in a new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(embedded_test_server()->GetURL(kChangePasswordUrl),
            new_web_contents->GetVisibleURL());
  // Security info should be updated.
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OnEnterpriseTriggerOffGSuite) {
  GetService(/*is_incognito=*/false);  // Create a service to listen to events.
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/true, PasswordProtectionTrigger::PHISHING_REUSE);
  Profile* profile = browser()->profile();
  SimulateGaiaPasswordChange("password");
  ASSERT_EQ(1u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    .size());
  // Turn off trigger
  profile->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger,
      PasswordProtectionTrigger::PASSWORD_PROTECTION_OFF);

  password_manager::HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(profile->GetPrefs());
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      user_manager::kStubUserEmail, /*is_gaia_password=*/true));
  EXPECT_EQ(0u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    .size());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OnEnterpriseTriggerOff) {
  GetService(/*is_incognito=*/false);  // Create a service to listen to events.
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  Profile* profile = browser()->profile();

  ASSERT_EQ(0u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    .size());
  // Configures initial password to "password_1";
  password_manager::PasswordReuseManager* reuse_manager =
      PasswordReuseManagerFactory::GetForProfile(browser()->profile());
  reuse_manager->SaveEnterprisePasswordHash("username@domain.com",
                                            u"password_1");
  reuse_manager->SaveGaiaPasswordHash(
      user_manager::kStubUserEmail, u"password_2",
      /*is_primary_account=*/false,
      password_manager::metrics_util::GaiaPasswordHashChange::
          CHANGED_IN_CONTENT_AREA);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kLocalStateEnterprisePasswordHashes)) {
    ASSERT_EQ(1u, profile->GetPrefs()
                      ->GetList(password_manager::prefs::kPasswordHashDataList)
                      .size());
    ASSERT_EQ(1u,
              g_browser_process->local_state()
                  ->GetList(password_manager::prefs::kLocalPasswordHashDataList)
                  .size());
  } else {
    ASSERT_EQ(2u, profile->GetPrefs()
                      ->GetList(password_manager::prefs::kPasswordHashDataList)
                      .size());
  }

  // Turn off trigger
  profile->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger,
      PasswordProtectionTrigger::PASSWORD_PROTECTION_OFF);

  password_manager::HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(profile->GetPrefs());
  hash_password_manager.set_local_prefs(g_browser_process->local_state());
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username@domain.com", /*is_gaia_password=*/false));
  EXPECT_FALSE(
      hash_password_manager.HasPasswordHash(user_manager::kStubUserEmail,
                                            /*is_gaia_password=*/true));
  EXPECT_EQ(0u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    .size());
}

// Test fixture for testing the navigation deferral mechanism while a modal
// warning dialog is being shown or potentially about to be shown.
class ChromePasswordProtectionServiceNavigationDeferralBrowserTest
    : public ChromePasswordProtectionServiceBrowserTest {
 public:
  ChromePasswordProtectionServiceNavigationDeferralBrowserTest() = default;
  ~ChromePasswordProtectionServiceNavigationDeferralBrowserTest() override =
      default;

  scoped_refptr<PasswordProtectionRequest>
  StartRequestWithPotentialForModalWarning() {
    ChromePasswordProtectionService* service =
        GetService(/*is_incognito=*/false);

    const std::string kSignonRealm = "https://example.test";
    const std::u16string kUsername = u"username1";
    std::vector<password_manager::MatchingReusedCredential> credentials = {
        {kSignonRealm, kUsername}};

    service->StartRequestForTesting(
        GetWebContents(), GURL(), GURL(), GURL(), "",
        PasswordType::SAVED_PASSWORD, credentials,
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT, true);
    if (service->get_pending_requests_for_testing().size() != 1ul)
      return nullptr;

    return *service->get_pending_requests_for_testing().begin();
  }

  void FinishRequest(PasswordProtectionRequest* request,
                     LoginReputationClientResponse_VerdictType verdict_type) {
    auto verdict = std::make_unique<LoginReputationClientResponse>();
    verdict->set_verdict_type(verdict_type);
    request->finish_for_testing(RequestOutcome::SUCCEEDED, std::move(verdict));
  }

  PasswordProtectionCommitDeferringCondition* GetDeferringCondition(
      PasswordProtectionRequest* request) {
    auto* request_content =
        static_cast<PasswordProtectionRequestContent*>(request);
    if (request_content->get_deferred_navigations_for_testing().size() != 1ul)
      return nullptr;

    return *request_content->get_deferred_navigations_for_testing().begin();
  }

  void DismissModalDialog(WarningAction action) {
    ReusedPasswordAccountType account_type;
    account_type.set_account_type(ReusedPasswordAccountType::SAVED_PASSWORD);
    GetService(/*is_incognito=*/false)
        ->OnUserAction(GetWebContents(), account_type,
                       RequestOutcome::SUCCEEDED,
                       LoginReputationClientResponse::PHISHING, "unused_token",
                       WarningUIType::MODAL_DIALOG, action);
  }

  bool RunUntilRequestDefersCommit(
      PasswordProtectionRequest* request,
      content::TestNavigationManager& navigation_manager) {
    bool is_at_respose = navigation_manager.WaitForResponse();
    CHECK(is_at_respose);

    // This will run until the first yield point of the navigation which could
    // be the PasswordProtectionCommitDeferringCondition but it may also be some
    // other condition, or if no conditions defer it will run until the
    // navigation finish.
    navigation_manager.ResumeNavigation();

    // If the navigation finished, fail the test.
    EXPECT_TRUE(navigation_manager.GetNavigationHandle());
    if (!navigation_manager.GetNavigationHandle())
      return false;

    // We must be blocked on a CommitDeferringCondition, otherwise, some new
    // yield point was added after the response but before
    // CommitDeferringConditions.
    CHECK(navigation_manager.GetNavigationHandle()
              ->IsCommitDeferringConditionDeferredForTesting());

    // A PasswordProtectionCommitDeferringCondition should have been created.
    PasswordProtectionCommitDeferringCondition* condition =
        GetDeferringCondition(request);
    CHECK(condition);

    // If the navigation is blocked on the PasswordProtection condition then
    // we're already done.
    if (navigation_manager.GetNavigationHandle()
            ->GetCommitDeferringConditionForTesting() == condition) {
      return true;
    }

    // There could be other CommitDeferringConditions before the
    // PasswordProtection one. Continue past them until we get to it.
    base::RunLoop run_loop(base::RunLoop::Type::kDefault);
    condition->register_invoke_callback_for_testing(run_loop.QuitClosure());
    run_loop.Run();

    // If the navigation finished, fail the test.
    EXPECT_TRUE(navigation_manager.GetNavigationHandle());
    if (!navigation_manager.GetNavigationHandle())
      return false;

    // Ensure the navigation is deferred on the condition we expect.
    EXPECT_EQ(navigation_manager.GetNavigationHandle()
                  ->GetCommitDeferringConditionForTesting(),
              condition);
    return navigation_manager.GetNavigationHandle()
               ->GetCommitDeferringConditionForTesting() == condition;
  }
};

// Tests that a navigation started while a modal warning is showing is deferred
// until the modal is dismissed.
IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceNavigationDeferralBrowserTest,
    ModalWarningDefersNavigation) {
  const GURL kNextPage = embedded_test_server()->GetURL("/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  // Start a request for a PASSWORD_REUSE_EVENT.
  scoped_refptr<PasswordProtectionRequest> request =
      StartRequestWithPotentialForModalWarning();
  ASSERT_TRUE(request);

  // Finish the request so that it results in a modal warning being shown.
  FinishRequest(request.get(), LoginReputationClientResponse::PHISHING);
  ASSERT_TRUE(request->is_modal_warning_showing());

  // Start a renderer-initiated navigation away from the current page. The
  // navigation should be deferred.
  content::TestNavigationManager navigation(GetWebContents(), kNextPage);
  ASSERT_TRUE(content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                              content::JsReplace("location = $1", kNextPage)));

  // Run the navigation until it defers on the
  // PasswordProtectionCommitDeferringCondition.
  ASSERT_TRUE(RunUntilRequestDefersCommit(request.get(), navigation));

  // Simulate the user dismissing the dialog. The navigation should be resumed.
  DismissModalDialog(WarningAction::IGNORE_WARNING);

  ASSERT_TRUE(navigation.WaitForNavigationFinished());
  ASSERT_TRUE(navigation.was_successful());
}

// Tests that a navigation started while there is a password protection request
// that could lead to showing a modal warning will be deferred. In this case,
// the request response determines the page is safe so the navigation should be
// automatically resumed.
IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceNavigationDeferralBrowserTest,
    PendingRequestDefersNavigationSafeResponseResumes) {
  const GURL kNextPage = embedded_test_server()->GetURL("/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  // Start a request for a PASSWORD_REUSE_EVENT.
  scoped_refptr<PasswordProtectionRequest> request =
      StartRequestWithPotentialForModalWarning();
  ASSERT_TRUE(request);

  // Start a renderer-initiated navigation away from the current page. The
  // navigation should be deferred.
  content::TestNavigationManager navigation(GetWebContents(), kNextPage);
  ASSERT_TRUE(content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                              content::JsReplace("location = $1", kNextPage)));

  // Run the navigation until it defers on the
  // PasswordProtectionCommitDeferringCondition.
  ASSERT_TRUE(RunUntilRequestDefersCommit(request.get(), navigation));

  // Finish the request so that the modal warning is not shown.
  FinishRequest(request.get(), LoginReputationClientResponse::SAFE);
  ASSERT_FALSE(request->is_modal_warning_showing());

  ASSERT_TRUE(navigation.WaitForNavigationFinished());
  ASSERT_TRUE(navigation.was_successful());
}

// Tests that a navigation started while there is a password protection request
// that could lead to showing a modal warning will be deferred. In this case,
// the request response does show a warning so the navigation should continue
// to be deferred until the user dismisses the warning.
IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceNavigationDeferralBrowserTest,
    PendingRequestDefersNavigationWarningContinuesDeferring) {
  const GURL kNextPage = embedded_test_server()->GetURL("/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  // Start a request for a PASSWORD_REUSE_EVENT.
  scoped_refptr<PasswordProtectionRequest> request =
      StartRequestWithPotentialForModalWarning();
  ASSERT_TRUE(request);

  // Start a renderer-initiated navigation away from the current page. The
  // navigation should be deferred.
  content::TestNavigationManager navigation(GetWebContents(), kNextPage);
  ASSERT_TRUE(content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                              content::JsReplace("location = $1", kNextPage)));
  // Run the navigation until it defers on the
  // PasswordProtectionCommitDeferringCondition.
  ASSERT_TRUE(RunUntilRequestDefersCommit(request.get(), navigation));

  // Finish the request so that a modal warning is shown
  FinishRequest(request.get(), LoginReputationClientResponse::PHISHING);
  ASSERT_TRUE(request->is_modal_warning_showing());
  ASSERT_TRUE(navigation.GetNavigationHandle()
                  ->IsCommitDeferringConditionDeferredForTesting());

  // Simulate the user dismissing the dialog. The navigation should be resumed.
  DismissModalDialog(WarningAction::IGNORE_WARNING);

  ASSERT_TRUE(navigation.WaitForNavigationFinished());
  ASSERT_TRUE(navigation.was_successful());
}

// Extends the test fixture with support for testing prerendered and
// back/forward cached pages.
class ChromePasswordProtectionServiceDeferActivationBrowserTest
    : public ChromePasswordProtectionServiceNavigationDeferralBrowserTest {
 public:
  ChromePasswordProtectionServiceDeferActivationBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ChromePasswordProtectionServiceDeferActivationBrowserTest::
                GetWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ChromePasswordProtectionServiceNavigationDeferralBrowserTest::SetUp();
  }

  // This is almost the same as the base class version but is slightly
  // different as it must use a TestActivationManager which has different
  // navigation timing.
  bool RunUntilRequestDefersCommit(
      PasswordProtectionRequest* request,
      content::TestActivationManager& prerender_manager) {
    bool is_at_before_checks = prerender_manager.WaitForBeforeChecks();
    CHECK(is_at_before_checks);

    // A PasswordProtectionCommitDeferringCondition should have been created
    // for commit checks.
    PasswordProtectionCommitDeferringCondition* condition =
        GetDeferringCondition(request);
    CHECK(condition);

    base::RunLoop run_loop(base::RunLoop::Type::kDefault);
    condition->register_invoke_callback_for_testing(run_loop.QuitClosure());

    // This will run until the first yield point of the navigation which could
    // be the PasswordProtectionCommitDeferringCondition but it may also be some
    // other condition, or if no conditions defer it will run until the
    // navigation finish.
    prerender_manager.ResumeActivation();

    // If the navigation finished, fail the test.
    EXPECT_TRUE(prerender_manager.GetNavigationHandle());
    if (!prerender_manager.GetNavigationHandle())
      return false;

    // If the navigation yielded on a condition before the
    // PasswordProtectionCommitDeferringCondition, continue until it is
    // invoked.  If ResumeActivation yielded due to the PasswordProtection
    // condition, this will be a no-op.
    run_loop.Run();

    // If the navigation finished, fail the test.
    EXPECT_TRUE(prerender_manager.GetNavigationHandle());
    if (!prerender_manager.GetNavigationHandle())
      return false;

    // Ensure the navigation is deferred on the condition we expect.
    EXPECT_EQ(prerender_manager.GetNavigationHandle()
                  ->GetCommitDeferringConditionForTesting(),
              condition);
    return prerender_manager.GetNavigationHandle()
               ->GetCommitDeferringConditionForTesting() == condition;
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that activation of a prerendered page is deferred when there is a
// pending PasswordProtectionRequest which might trigger a modal warning.
// This tests the case where the prerender starts before the
// PasswordProtectionRequest.
IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceDeferActivationBrowserTest,
    DeferActivatePrerenderStartedBeforeRequest) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  // Start a prerender.
  const GURL kPrerenderUrl = embedded_test_server()->GetURL("/simple.html");
  prerender_helper_.AddPrerender(kPrerenderUrl);

  // Start a request that could lead to showing a modal warning.
  scoped_refptr<PasswordProtectionRequest> request =
      StartRequestWithPotentialForModalWarning();
  ASSERT_TRUE(request);

  // Navigate to the prerendered URL. Ensure the activation navigation is
  // deferred until the request finishes without showing a modal.
  content::TestActivationManager prerender_manager(GetWebContents(),
                                                   kPrerenderUrl);

  ASSERT_TRUE(
      content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", kPrerenderUrl)));

  // Run the navigation until it defers on the
  // PasswordProtectionCommitDeferringCondition.
  ASSERT_TRUE(RunUntilRequestDefersCommit(request.get(), prerender_manager));

  // Simulate a response arriving that doesn't show the warning dialog. The
  // activating navigation should now be allowed to complete.
  FinishRequest(request.get(), LoginReputationClientResponse::SAFE);
  ASSERT_FALSE(request->is_modal_warning_showing());
  prerender_manager.WaitForNavigationFinished();
  ASSERT_TRUE(prerender_manager.was_activated());
}

// Tests that activation of a prerendered page is deferred when there is a
// pending PasswordProtectionRequest which might trigger a modal warning.
// This tests the case where the prerender starts after the
// PasswordProtectionRequest.
IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceDeferActivationBrowserTest,
    DeferActivatePrerenderStartedAfterRequest) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  // Start a request that could lead to showing a modal warning.
  scoped_refptr<PasswordProtectionRequest> request =
      StartRequestWithPotentialForModalWarning();
  ASSERT_TRUE(request);

  // Start a prerender; this should not be deferred since it's happening in the
  // background.
  const GURL kPrerenderUrl = embedded_test_server()->GetURL("/simple.html");
  prerender_helper_.AddPrerender(kPrerenderUrl);

  ASSERT_TRUE(prerender_helper_.GetHostForUrl(kPrerenderUrl));

  // Navigate to the prerendered URL. Ensure the activation navigation is
  // deferred until the request finishes without showing a modal.
  content::TestActivationManager prerender_manager(GetWebContents(),
                                                   kPrerenderUrl);
  ASSERT_TRUE(
      content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", kPrerenderUrl)));

  // Run the navigation until it defers on the
  // PasswordProtectionCommitDeferringCondition.
  ASSERT_TRUE(RunUntilRequestDefersCommit(request.get(), prerender_manager));

  // Simulate a response arriving that doesn't show the warning dialog. The
  // activating navigation should now be allowed to complete.
  FinishRequest(request.get(), LoginReputationClientResponse::SAFE);
  ASSERT_FALSE(request->is_modal_warning_showing());

  prerender_manager.WaitForNavigationFinished();
  ASSERT_TRUE(prerender_manager.was_activated());
}

IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceDeferActivationBrowserTest,
    PrerenderActivationDeferredByModalWarning) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  // Start a prerender.
  const GURL kPrerenderUrl = embedded_test_server()->GetURL("/simple.html");
  prerender_helper_.AddPrerender(kPrerenderUrl);

  // Start a request that could lead to showing a modal warning.
  scoped_refptr<PasswordProtectionRequest> request =
      StartRequestWithPotentialForModalWarning();
  ASSERT_TRUE(request);

  // Finish the request so that it results in a modal warning being shown.
  FinishRequest(request.get(), LoginReputationClientResponse::PHISHING);
  ASSERT_TRUE(request->is_modal_warning_showing());

  // Navigate to the prerendered URL. Ensure the activation navigation is
  // deferred until the request finishes without showing a modal.
  content::TestActivationManager prerender_manager(GetWebContents(),
                                                   kPrerenderUrl);

  ASSERT_TRUE(
      content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", kPrerenderUrl)));

  // Run the navigation until it defers on the
  // PasswordProtectionCommitDeferringCondition.
  ASSERT_TRUE(RunUntilRequestDefersCommit(request.get(), prerender_manager));

  // Simulate the user dismissing the dialog. The navigation should be resumed.
  DismissModalDialog(WarningAction::IGNORE_WARNING);

  prerender_manager.WaitForNavigationFinished();
  ASSERT_TRUE(prerender_manager.was_successful());
}

// Tests that activation of back/forward cached pages is not deferred if the
// navigation is browser-initiated.
// TODO(bokan): This is existing behavior but I wonder if it's intentional? The
// user can't click any browser UI while the modal is up anyway...
IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceDeferActivationBrowserTest,
    BrowserInitiatedBackForwardCacheActivation) {
  // Put a simple page in the back/forward cache.
  const GURL kURLInBFCache = embedded_test_server()->GetURL("/simple.html");
  content::RenderFrameHost* rfh_a_raw =
      ui_test_utils::NavigateToURL(browser(), kURLInBFCache);
  content::RenderFrameHostWrapper rfh_a(rfh_a_raw);
  content::RenderFrameHost* rfh_b_raw = ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl));
  content::RenderFrameHostWrapper rfh_b(rfh_b_raw);

  // Ensure that `rfh_a` is in the back/forward cache.
  EXPECT_FALSE(rfh_a.IsRenderFrameDeleted());
  EXPECT_NE(rfh_a.get(), rfh_b.get());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Start a request that could lead to showing a modal warning.
  scoped_refptr<PasswordProtectionRequest> request =
      StartRequestWithPotentialForModalWarning();
  ASSERT_TRUE(request);

  content::TestActivationManager prerender_manager(GetWebContents(),
                                                   kURLInBFCache);

  // Do a browser-initiated back history navigation. This should be allowed
  // because it is browser-initiated.
  GetWebContents()->GetController().GoBack();
  ASSERT_TRUE(prerender_manager.WaitForBeforeChecks());

  // The activating navigation should be unblocked.
  prerender_manager.WaitForNavigationFinished();
  ASSERT_TRUE(prerender_manager.was_activated());
}

// Tests that activation of back/forward cached pages from a renderer initiated
// history navigation is deferred while there is a pending
// PasswordProtectionRequest which might trigger a modal warning.
IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceDeferActivationBrowserTest,
    DeferRendererInitiatedBackForwardCacheActivation) {
  // Put a simple page in the back/forward cache.
  const GURL kURLInBFCache = embedded_test_server()->GetURL("/simple.html");
  content::RenderFrameHost* rfh_a_raw =
      ui_test_utils::NavigateToURL(browser(), kURLInBFCache);
  content::RenderFrameHostWrapper rfh_a(rfh_a_raw);
  content::RenderFrameHost* rfh_b_raw = ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl));
  content::RenderFrameHostWrapper rfh_b(rfh_b_raw);

  // Ensure that `rfh_a` is in the back/forward cache.
  EXPECT_FALSE(rfh_a.IsRenderFrameDeleted());
  EXPECT_NE(rfh_a.get(), rfh_b.get());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Start a request that could lead to showing a modal warning.
  scoped_refptr<PasswordProtectionRequest> request =
      StartRequestWithPotentialForModalWarning();
  ASSERT_TRUE(request);

  // Navigate back to the URL in the BFCache. Ensure the BFCache restore is
  // deferred until the request finishes without showing a modal.  Note: this
  // must be a renderer-initiated history navigation. A user-initiated history
  // navigation should not be deferred.
  content::TestActivationManager prerender_manager(GetWebContents(),
                                                   kURLInBFCache);
  ASSERT_TRUE(content::ExecJs(
      GetWebContents()->GetPrimaryMainFrame(),
      content::JsReplace("window.history.back()", kURLInBFCache)));

  // Run the navigation until it defers on the
  // PasswordProtectionCommitDeferringCondition.
  ASSERT_TRUE(RunUntilRequestDefersCommit(request.get(), prerender_manager));

  // Finish the request so that no warning is shown.
  FinishRequest(request.get(), LoginReputationClientResponse::SAFE);
  ASSERT_FALSE(request->is_modal_warning_showing());

  // Since no modal is showing, the activating navigation should be unblocked.
  prerender_manager.WaitForNavigationFinished();
  ASSERT_TRUE(prerender_manager.was_activated());
}

#if !BUILDFLAG(IS_ANDROID)
class ChromePasswordProtectionServiceTrustSafetySentimentServiceBrowserTest
    : public ChromePasswordProtectionServiceBrowserTest {
 public:
  void SetUpMockServiceExpectations() {
    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));
  }

  void ExpectPhishedPasswordUpdateNotClickedCall(
      PasswordProtectionUIType ui_type,
      PasswordProtectionUIAction action) {
    EXPECT_CALL(*mock_sentiment_service_,
                PhishedPasswordUpdateNotClicked(ui_type, action));
  }

  void ExpectProtectResetOrCheckPasswordClickedCall(
      PasswordProtectionUIType ui_type) {
    EXPECT_CALL(*mock_sentiment_service_,
                ProtectResetOrCheckPasswordClicked(ui_type));
  }

 private:
  raw_ptr<MockTrustSafetySentimentService, DanglingUntriaged>
      mock_sentiment_service_;
};

IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceTrustSafetySentimentServiceBrowserTest,
    NonPasswordChangeTrigger) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingSurveysEnabled, true);
  // Expect Trust and Safety Sentiment Service to call
  // PhishedPasswordUpdateNotClicked.
  SetUpMockServiceExpectations();
  ExpectPhishedPasswordUpdateNotClickedCall(
      PasswordProtectionUIType::MODAL_DIALOG,
      PasswordProtectionUIAction::CLOSE);

  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  account_type.set_is_account_syncing(true);
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::MODAL_DIALOG,
                        WarningAction::CLOSE);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceTrustSafetySentimentServiceBrowserTest,
    PasswordChangeTrigger) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingSurveysEnabled, true);
  // Expect Trust and Safety Sentiment Service to call
  // ProtectResetOrCheckPasswordClicked.
  SetUpMockServiceExpectations();
  ExpectProtectResetOrCheckPasswordClickedCall(
      PasswordProtectionUIType::MODAL_DIALOG);

  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kLoginPageUrl)));

  ReusedPasswordAccountType account_type;
  account_type.set_account_type(ReusedPasswordAccountType::GSUITE);
  account_type.set_is_account_syncing(true);
  service->OnUserAction(web_contents, account_type, RequestOutcome::UNKNOWN,
                        LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
                        "unused_token", WarningUIType::MODAL_DIALOG,
                        WarningAction::CHANGE_PASSWORD);
  base::RunLoop().RunUntilIdle();
}
#endif

}  // namespace safe_browsing
