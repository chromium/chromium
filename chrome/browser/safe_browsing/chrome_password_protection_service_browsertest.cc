// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kGaiaPasswordChangeHistogramName[] =
    "PasswordProtection.GaiaPasswordReusesBeforeGaiaPasswordChanged";
const char kLoginPageUrl[] = "/safe_browsing/login_page.html";
const char kChangePasswordUrl[] = "/safe_browsing/change_password_page.html";

}  // namespace

namespace safe_browsing {
using PasswordReuseEvent = LoginReputationClientRequest::PasswordReuseEvent;

class ChromePasswordProtectionServiceBrowserTest : public InProcessBrowserTest {
 public:
  ChromePasswordProtectionServiceBrowserTest() {}

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  ChromePasswordProtectionService* GetService(bool is_incognito) {
    return ChromePasswordProtectionService::GetPasswordProtectionService(
        is_incognito ? browser()->profile()->GetOffTheRecordProfile()
                     : browser()->profile());
  }

  void SimulateGaiaPasswordChange(const std::string& new_password) {
    password_manager::HashPasswordManager hash_manager;
    hash_manager.set_prefs(browser()->profile()->GetPrefs());
    hash_manager.SavePasswordHash("stub-user@example.com",
                                  base::UTF8ToUTF16(new_password));
  }

  void SimulateGaiaPasswordChanged(ChromePasswordProtectionService* service) {
    service->OnGaiaPasswordChanged();
  }

  void GetSecurityInfo(content::WebContents* web_contents,
                       security_state::SecurityInfo* out_security_info) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(web_contents);
    helper->GetSecurityInfo(out_security_info);
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(
                    &ChromePasswordProtectionServiceBrowserTest::
                        OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Replace the signin manager and account fetcher service with fakes.
    SigninManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildFakeSigninManagerForTesting));
    AccountFetcherServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FakeAccountFetcherServiceBuilder::BuildForTests));
  }

  // Makes user signed-in as |email| with |hosted_domain|.
  void PrepareSyncAccount(const std::string& hosted_domain,
                          const std::string& email) {
    // For simplicity purpose, we make gaia_id the same as email.
    std::string gaia_id(email);
    FakeSigninManagerForTesting* signin_manager =
        static_cast<FakeSigninManagerForTesting*>(
            SigninManagerFactory::GetInstance()->GetForProfile(
                browser()->profile()));
#if !defined(OS_CHROMEOS)
    signin_manager->SignIn(gaia_id, email, "password");
#else
    AccountTrackerService* account_tracker_service =
        AccountTrackerServiceFactory::GetForProfile(browser()->profile());
    signin_manager->SignIn(
        account_tracker_service->PickAccountIdForAccount(gaia_id, email));
#endif
    FakeAccountFetcherService* account_fetcher_service =
        static_cast<FakeAccountFetcherService*>(
            AccountFetcherServiceFactory::GetForProfile(browser()->profile()));
    account_fetcher_service->FakeUserInfoFetchSuccess(
        signin_manager->GetAuthenticatedAccountId(), email, gaia_id,
        hosted_domain, "full_name", "given_name", "locale",
        "http://picture.example.com/picture.jpg");
  }

  void ConfigureEnterprisePasswordProtection(
      bool is_gsuite,
      PasswordProtectionTrigger trigger_type) {
    if (is_gsuite)
      PrepareSyncAccount("example.com", "stub-user@example.com");
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger, trigger_type);
    browser()->profile()->GetPrefs()->SetString(
        prefs::kPasswordProtectionChangePasswordURL,
        embedded_test_server()->GetURL(kChangePasswordUrl).spec());
  }

 protected:
  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       SuccessfullyChangeSignInPassword) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;

  // Initialize and verify initial state.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::NONE, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::SIGN_IN_PASSWORD);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking "Change Password" button on the modal dialog.
  service->OnUserAction(web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD,
                        WarningUIType::MODAL_DIALOG,
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
  SimulateGaiaPasswordChanged(service);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            security_info.malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       SuccessfullyShowWarningIncognito) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/true);
  Profile* profile = browser()->profile()->GetOffTheRecordProfile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;

  // Initialize and verify initial state.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::NONE, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::SIGN_IN_PASSWORD);
  base::RunLoop().RunUntilIdle();
  // Change password card on chrome settings page should NOT show.
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       MarkSiteAsLegitimate) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;

  // Initialize and verify initial state.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::NONE, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::SIGN_IN_PASSWORD);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking "Ignore" button on the modal dialog.
  service->OnUserAction(web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD,
                        WarningUIType::MODAL_DIALOG,
                        WarningAction::IGNORE_WARNING);
  base::RunLoop().RunUntilIdle();
  // No new tab opens. SecurityInfo doesn't change.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking on "Mark site legitimate". Site is no longer dangerous.
  service->OnUserAction(web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD,
                        WarningUIType::PAGE_INFO,
                        WarningAction::MARK_AS_LEGITIMATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD));
  GetSecurityInfo(web_contents, &security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OpenChromeSettingsViaPageInfo) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::SIGN_IN_PASSWORD);
  base::RunLoop().RunUntilIdle();
  // Simulates clicking "Ignore" to close dialog.
  service->OnUserAction(web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD,
                        WarningUIType::MODAL_DIALOG,
                        WarningAction::IGNORE_WARNING);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          browser()->profile()));
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking on "Change Password" in the page info bubble.
  service->OnUserAction(web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD,
                        WarningUIType::PAGE_INFO,
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
  PrepareSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                     "stub-user@example.com");
  // Prepare sync account will trigger a password change.
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  ASSERT_TRUE(service);
  Profile* profile = browser()->profile();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_TRUE(
      profile->GetPrefs()
          ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
          ->empty());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  base::HistogramTester histograms;
  // Shows modal dialog on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::SIGN_IN_PASSWORD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());
  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Opens a new browser window.
  Browser* browser2 = CreateBrowser(profile);
  // Shows modal dialog on this new web_contents.
  content::WebContents* new_web_contents =
      browser2->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser2, GURL("data:text/html,<html></html>"));
  service->ShowModalWarning(new_web_contents, "unused_token",
                            PasswordReuseEvent::SIGN_IN_PASSWORD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());
  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Simulates a Gaia password change.
  SimulateGaiaPasswordChange("new_password");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  EXPECT_THAT(histograms.GetAllSamples(kGaiaPasswordChangeHistogramName),
              testing::ElementsAre(base::Bucket(2, 1)));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyCheckGaiaPasswordChange) {
  PrepareSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                     "stub-user@example.com");
  Profile* profile = browser()->profile();
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  // Configures initial password to "password_1";
  password_manager::PasswordHashData hash_data(
      "stub-user@example.com", base::UTF8ToUTF16("password_1"), true);
  password_manager::HashPasswordManager hash_manager;
  hash_manager.set_prefs(profile->GetPrefs());
  hash_manager.SavePasswordHash(hash_data);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));

  // Shows modal dialog on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::SIGN_IN_PASSWORD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());

  // Save the same password will not trigger OnGaiaPasswordChanged(), thus no
  // change to size of unhandled_password_reuses().
  SimulateGaiaPasswordChange("password_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());

  // Save a different password will clear unhandled_password_reuses().
  SimulateGaiaPasswordChange("password_2");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyShouldShowChangePasswordSettingUI) {
  Profile* profile = browser()->profile();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  // Simulates previous session has unhandled password reuses.
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  update->SetKey("https://oldreuse.com",
                 /*navigation_id=*/base::Value("12345"));

  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Simulates a Gaia password change.
  SimulateGaiaPasswordChanged(GetService(/*is_incognito=*/false));
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  EXPECT_TRUE(
      profile->GetPrefs()
          ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
          ->empty());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyIsPasswordReuseProtectionConfigured) {
  Profile* profile = browser()->profile();
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  // If prefs::kPasswordProtectionWarningTrigger isn't set to PASSWORD_REUSE,
  // |IsPasswordReuseProtectionConfigured(..)| returns false.
  EXPECT_EQ(PASSWORD_PROTECTION_OFF,
            service->GetPasswordProtectionWarningTriggerPref());
  EXPECT_FALSE(
      ChromePasswordProtectionService::IsPasswordReuseProtectionConfigured(
          profile));

  PrepareSyncAccount(std::string(AccountTrackerService::kNoHostedDomainFound),
                     "stub-user@example.com");
  profile->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                  PasswordProtectionTrigger::PASSWORD_REUSE);
  // Otherwise, |IsPasswordReuseProtectionConfigured(..)| returns true.
  EXPECT_TRUE(
      ChromePasswordProtectionService::IsPasswordReuseProtectionConfigured(
          profile));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       GSuitePasswordAlertMode) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/true, PasswordProtectionTrigger::PASSWORD_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  base::HistogramTester histograms;
  // Shows interstitial on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowInterstitial(web_contents, PasswordReuseEvent::SIGN_IN_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  // chrome://reset-password page should be opened in a new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(GURL(chrome::kChromeUIResetPasswordURL),
            new_web_contents->GetVisibleURL());
  EXPECT_THAT(histograms.GetAllSamples("PasswordProtection.InterstitialString"),
              testing::ElementsAre(base::Bucket(3, 1)));

  // Clicks on "Reset Password" button.
  std::string script =
      "var node = document.getElementById('reset-password-button'); \n"
      "node.click();";
  ASSERT_TRUE(content::ExecuteScript(new_web_contents, script));
  content::TestNavigationObserver observer1(new_web_contents,
                                            /*number_of_navigations=*/1);
  observer1.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            embedded_test_server()->GetURL(kChangePasswordUrl));
  EXPECT_THAT(
      histograms.GetAllSamples(
          "PasswordProtection.InterstitialAction.GSuiteSyncPasswordEntry"),
      testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       ChromeEnterprisePasswordAlertMode) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PASSWORD_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  base::HistogramTester histograms;
  // Shows interstitial on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowInterstitial(web_contents,
                            PasswordReuseEvent::ENTERPRISE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  EXPECT_THAT(histograms.GetAllSamples("PasswordProtection.InterstitialString"),
              testing::ElementsAre(base::Bucket(2, 1)));

  // Clicks on "Reset Password" button.
  std::string script =
      "var node = document.getElementById('reset-password-button'); \n"
      "node.click();";
  ASSERT_TRUE(content::ExecuteScript(new_web_contents, script));
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
                       UserDirectlyNavigateToResetPasswordPage) {
  base::HistogramTester histograms;
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PASSWORD_REUSE);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://reset-password"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // chrome://reset-password page should be opened.
  ASSERT_EQ(GURL(chrome::kChromeUIResetPasswordURL),
            web_contents->GetVisibleURL());
  EXPECT_THAT(histograms.GetAllSamples("PasswordProtection.InterstitialString"),
              testing::ElementsAre(base::Bucket(0, 1)));

  // Clicks on "Reset Password" button.
  std::string script =
      "var node = document.getElementById('reset-password-button'); \n"
      "node.click();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  content::TestNavigationObserver observer1(web_contents,
                                            /*number_of_navigations=*/1);
  observer1.Wait();
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            embedded_test_server()->GetURL(kChangePasswordUrl));
  EXPECT_THAT(histograms.GetAllSamples(
                  "PasswordProtection.InterstitialActionByUserNavigation"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       EnterprisePhishingReuseMode) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::ENTERPRISE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  // Enterprise password reuse should not trigger warning in Chrome settings UI.
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::ENTERPRISE_PASSWORD));
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  // Security info should be properly updated.
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking "Change Password" button on the modal dialog.
  service->OnUserAction(web_contents, PasswordReuseEvent::ENTERPRISE_PASSWORD,
                        WarningUIType::MODAL_DIALOG,
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
  security_state::SecurityInfo security_info;
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::ENTERPRISE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking on "Mark site legitimate". Site is no longer dangerous.
  service->OnUserAction(web_contents, PasswordReuseEvent::ENTERPRISE_PASSWORD,
                        WarningUIType::PAGE_INFO,
                        WarningAction::MARK_AS_LEGITIMATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordReuseEvent::ENTERPRISE_PASSWORD));
  GetSecurityInfo(web_contents, &security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       EnterprisePhishingReuseOpenChromeSettingsViaPageInfo) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordReuseEvent::ENTERPRISE_PASSWORD);
  base::RunLoop().RunUntilIdle();

  // Simulates clicking on "Change Password" in the page info bubble.
  service->OnUserAction(web_contents, PasswordReuseEvent::ENTERPRISE_PASSWORD,
                        WarningUIType::PAGE_INFO,
                        WarningAction::CHANGE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Enterprise change password page should be opened in a new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(embedded_test_server()->GetURL(kChangePasswordUrl),
            new_web_contents->GetVisibleURL());
  // Security info should be updated.
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            security_info.malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OnEnterpriseTriggerOffGSuite) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/true, PasswordProtectionTrigger::PHISHING_REUSE);
  Profile* profile = browser()->profile();
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  password_manager::HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(profile->GetPrefs());
  hash_password_manager.SavePasswordHash(service->GetAccountInfo().email,
                                         base::UTF8ToUTF16("password"),
                                         /*is_gaia_password=*/true);
  ASSERT_EQ(1u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    ->GetList()
                    .size());

  // Turn off trigger
  profile->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger,
      PasswordProtectionTrigger::PASSWORD_PROTECTION_OFF);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      service->GetAccountInfo().email, /*is_gaia_password=*/true));
  EXPECT_EQ(0u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    ->GetList()
                    .size());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OnEnterpriseTriggerOff) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  Profile* profile = browser()->profile();
  password_manager::HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(profile->GetPrefs());
  hash_password_manager.SavePasswordHash(
      "username", base::UTF8ToUTF16("password"), /*is_gaia_password=*/false);
  hash_password_manager.SavePasswordHash("foo@gmail.com",
                                         base::UTF8ToUTF16("password"),
                                         /*is_gaia_password=*/true);
  ASSERT_EQ(2u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    ->GetList()
                    .size());

  // Turn off trigger
  profile->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger,
      PasswordProtectionTrigger::PASSWORD_PROTECTION_OFF);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("foo@gmail.com",
                                                    /*is_gaia_password=*/true));
  EXPECT_EQ(1u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    ->GetList()
                    .size());
}

}  // namespace safe_browsing
