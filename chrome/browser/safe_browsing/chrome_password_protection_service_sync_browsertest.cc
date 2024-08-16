// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kLoginPageUrl[] = "/safe_browsing/login_page.html";
const char kChangePasswordUrl[] = "/safe_browsing/change_password_page.html";

}  // namespace

using ChromePasswordProtectionService =
    safe_browsing::ChromePasswordProtectionService;
using PasswordProtectionTrigger = safe_browsing::PasswordProtectionTrigger;
using password_manager::metrics_util::PasswordType;
using WarningUIType = safe_browsing::WarningUIType;
using WarningAction = safe_browsing::WarningAction;

// This test suite tests functionality that requires Sync to be active.
class ChromePasswordProtectionServiceSyncBrowserTest : public SyncTest {
 public:
  ChromePasswordProtectionServiceSyncBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  ChromePasswordProtectionServiceSyncBrowserTest(
      const ChromePasswordProtectionServiceSyncBrowserTest&) = delete;
  ChromePasswordProtectionServiceSyncBrowserTest& operator=(
      const ChromePasswordProtectionServiceSyncBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_TRUE(SetupClients());

    // Sign the profile in and enable Sync.
    ASSERT_TRUE(
        GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSync));

    CoreAccountInfo current_info =
        IdentityManagerFactory::GetForProfile(GetProfile(0))
            ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
    // Need to update hosted domain since it is not populated.
    AccountInfo account_info;
    account_info.account_id = current_info.account_id;
    account_info.gaia = current_info.gaia;
    account_info.email = current_info.email;
    account_info.hosted_domain = "domain.com";
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(GetProfile(0)), account_info);

    ASSERT_TRUE(GetClient(0)->SetupSync());
  }

  safe_browsing::ChromePasswordProtectionService* GetService(
      bool is_incognito) {
    return ChromePasswordProtectionService::GetPasswordProtectionService(
        is_incognito ? GetProfile(0)->GetPrimaryOTRProfile(
                           /*create_if_needed=*/true)
                     : GetProfile(0));
  }

  void ConfigureEnterprisePasswordProtection(
      PasswordProtectionTrigger trigger_type) {
    GetProfile(0)->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger, trigger_type);
    GetProfile(0)->GetPrefs()->SetString(
        prefs::kPasswordProtectionChangePasswordURL,
        embedded_test_server()->GetURL(kChangePasswordUrl).spec());
  }
};

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceSyncBrowserTest,
                       GSuitePasswordAlertMode) {
  ConfigureEnterprisePasswordProtection(
      PasswordProtectionTrigger::PASSWORD_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  chrome::NewTab(GetBrowser(0));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      GetBrowser(0), embedded_test_server()->GetURL(kLoginPageUrl)));
  base::HistogramTester histograms;
  // Shows interstitial on current web_contents.
  content::WebContents* web_contents =
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
  safe_browsing::ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      safe_browsing::ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  service->set_reused_password_account_type_for_last_shown_warning(
      reused_password_account_type);
  service->ShowInterstitial(web_contents, reused_password_account_type);
  content::WebContents* interstitial_web_contents =
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(interstitial_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  // chrome://reset-password page should be opened in a new foreground tab.
  ASSERT_EQ(2, GetBrowser(0)->tab_strip_model()->count());
  ASSERT_EQ(GURL(chrome::kChromeUIResetPasswordURL),
            interstitial_web_contents->GetVisibleURL());

  // Clicks on "Reset Password" button.
  std::string script =
      "var node = document.getElementById('reset-password-button'); \n"
      "node.click();";
  ASSERT_TRUE(content::ExecJs(interstitial_web_contents, script));
  content::TestNavigationObserver observer1(interstitial_web_contents,
                                            /*number_of_navigations=*/1);
  observer1.Wait();
  EXPECT_EQ(2, GetBrowser(0)->tab_strip_model()->count());
  EXPECT_EQ(GetBrowser(0)
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            embedded_test_server()->GetURL(kChangePasswordUrl));
  EXPECT_THAT(
      histograms.GetAllSamples(
          "PasswordProtection.InterstitialAction.GSuiteSyncPasswordEntry"),
      testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}
