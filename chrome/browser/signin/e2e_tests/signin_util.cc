// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/sign_in_test_observer.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_urls.h"

namespace signin::test {

signin::IdentityManager* identity_manager(Browser* browser) {
  return IdentityManagerFactory::GetForProfile(browser->profile());
}

syncer::SyncService* sync_service(Browser* browser) {
  return SyncServiceFactory::GetForProfile(browser->profile());
}

AccountReconcilor* account_reconcilor(Browser* browser) {
  return AccountReconcilorFactory::GetForProfile(browser->profile());
}

SignInFunctions::SignInFunctions(
    const base::RepeatingCallback<Browser*()> browser,
    const base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>
        add_tab_function)
    : browser_(browser), add_tab_function_(add_tab_function) {}

SignInFunctions::~SignInFunctions() = default;

void SignInFunctions::SignInFromWeb(const TestAccount& test_account,
                                    int previously_signed_in_accounts) {
  ASSERT_TRUE(add_tab_function_.Run(0,
                                    GaiaUrls::GetInstance()->add_account_url(),
                                    ui::PageTransition::PAGE_TRANSITION_TYPED));
  SignInFromCurrentPage(test_account, previously_signed_in_accounts);
}

void SignInFunctions::SignInFromSettings(const TestAccount& test_account,
                                         int previously_signed_in_accounts) {
  GURL settings_url("chrome://settings");
  ASSERT_TRUE(add_tab_function_.Run(0, settings_url,
                                    ui::PageTransition::PAGE_TRANSITION_TYPED));
  auto* settings_tab =
      browser_.Run()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      settings_tab,
      base::StringPrintf(
          kSettingsScriptWrapperFormat,
          "settings.SyncBrowserProxyImpl.getInstance().startSignIn();")));
  SignInFromCurrentPage(test_account, previously_signed_in_accounts);
}

void SignInFunctions::SignInFromCurrentPage(const TestAccount& test_account,
                                            int previously_signed_in_accounts) {
  SignInTestObserver observer(identity_manager(browser_.Run()),
                              account_reconcilor(browser_.Run()));
  login_ui_test_utils::ExecuteJsToSigninInSigninFrame(
      browser_.Run(), test_account.user, test_account.password);
  observer.WaitForAccountChanges(previously_signed_in_accounts + 1,
                                 PrimarySyncAccountWait::kNotWait);
}

void SignInFunctions::TurnOnSync(const TestAccount& test_account,
                                 int previously_signed_in_accounts) {
  SignInFromSettings(test_account, previously_signed_in_accounts);

  SignInTestObserver observer(identity_manager(browser_.Run()),
                              account_reconcilor(browser_.Run()));
  EXPECT_TRUE(login_ui_test_utils::ConfirmSyncConfirmationDialog(
      browser_.Run(), kDialogTimeout));
  observer.WaitForAccountChanges(previously_signed_in_accounts + 1,
                                 PrimarySyncAccountWait::kWaitForAdded);
}

void SignInFunctions::SignOutFromWeb() {
  SignInTestObserver observer(identity_manager(browser_.Run()),
                              account_reconcilor(browser_.Run()));
  ASSERT_TRUE(
      add_tab_function_.Run(0, GaiaUrls::GetInstance()->service_logout_url(),
                            ui::PageTransition::PAGE_TRANSITION_TYPED));
  observer.WaitForAccountChanges(0, PrimarySyncAccountWait::kNotWait);
}

void SignInFunctions::TurnOffSync() {
  GURL settings_url("chrome://settings");
  ASSERT_TRUE(add_tab_function_.Run(0, settings_url,
                                    ui::PageTransition::PAGE_TRANSITION_TYPED));
  SignInTestObserver observer(identity_manager(browser_.Run()),
                              account_reconcilor(browser_.Run()));
  auto* settings_tab =
      browser_.Run()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      settings_tab,
      base::StringPrintf(
          kSettingsScriptWrapperFormat,
          "settings.SyncBrowserProxyImpl.getInstance().signOut(false)")));
  observer.WaitForAccountChanges(0, PrimarySyncAccountWait::kWaitForCleared);
}

}  // namespace signin::test
