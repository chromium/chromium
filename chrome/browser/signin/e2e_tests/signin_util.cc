// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/e2e_tests/signin_util.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/sign_in_test_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"  // nogncheck
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/test_signout_confirmation_handler_waiter.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
namespace signin::test {

signin::IdentityManager* identity_manager(BrowserWindowInterface* browser) {
  return IdentityManagerFactory::GetForProfile(browser->GetProfile());
}

syncer::SyncService* sync_service(BrowserWindowInterface* browser) {
  return SyncServiceFactory::GetForProfile(browser->GetProfile());
}

AccountReconcilor* account_reconcilor(BrowserWindowInterface* browser) {
  return AccountReconcilorFactory::GetForProfile(browser->GetProfile());
}

SignInFunctions::SignInFunctions(
    const base::RepeatingCallback<BrowserWindowInterface*()> browser,
    const base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>
        add_tab_function)
    : browser_(browser), add_tab_function_(add_tab_function) {}

SignInFunctions::~SignInFunctions() = default;

void SignInFunctions::SignInFromWeb(
    const TestAccountSigninCredentials& test_account,
    int previously_signed_in_accounts) {
  ASSERT_TRUE(add_tab_function_.Run(0,
                                    GaiaUrls::GetInstance()->add_account_url(),
                                    ui::PageTransition::PAGE_TRANSITION_TYPED));
  SignInFromCurrentPage(
      browser_.Run()->GetTabStripModel()->GetActiveWebContents(), test_account,
      previously_signed_in_accounts);
}

void SignInFunctions::SignInFromSettings(
    const TestAccountSigninCredentials& test_account,
    int previously_signed_in_accounts,
    bool complete_signin_operation) {
  GURL settings_url("chrome://settings");
  BrowserWindowInterface* browser = browser_.Run();
  ASSERT_TRUE(add_tab_function_.Run(0, settings_url,
                                    ui::PageTransition::PAGE_TRANSITION_TYPED));
  ui_test_utils::TabAddedWaiter signin_tab_waiter(browser);
  auto* settings_tab = browser->GetTabStripModel()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecJs(
      settings_tab,
      base::StringPrintf(
          kSettingsScriptWrapperFormat,
          "settings.SyncBrowserProxyImpl.getInstance()."
          "startSignIn(settings.ChromeSigninAccessPoint.SETTINGS);")));
  signin_tab_waiter.Wait();
  // Ensure the gaia login tab is loaded before proceeding.
  auto* gaia_login_tab = browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(gaia_login_tab));
  if (complete_signin_operation) {
    SignInFromCurrentPage(gaia_login_tab, test_account,
                          previously_signed_in_accounts);
  }
}

void SignInFunctions::SignInFromSettingsWithSyncChoice(
    const TestAccountSigninCredentials& test_account,
    int previously_signed_in_accounts,
    SyncChoice sync_choice) {
#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
  NOTREACHED();
#else
  SignInTestObserver observer(identity_manager(browser_.Run()),
                              account_reconcilor(browser_.Run()),
                              ConsentLevel::kSignin);

  std::unique_ptr<content::TestNavigationObserver> sync_confirmation_observer =
      std::make_unique<content::TestNavigationObserver>(
          AppendSyncConfirmationQueryParams(
              GURL("chrome://sync-confirmation"),
              SyncConfirmationStyle::kDefaultModal,
              /*is_sync_promo=*/true));
  std::unique_ptr<content::TestNavigationObserver> history_sync_observer =
      std::make_unique<content::TestNavigationObserver>(
          HistorySyncOptinUI::AppendHistorySyncOptinQueryParams(
              GURL("chrome://history-sync-optin"),
              HistorySyncOptinLaunchContext::kModal));
  sync_confirmation_observer->StartWatchingNewWebContents();
  history_sync_observer->StartWatchingNewWebContents();

  SignInFromSettings(test_account, previously_signed_in_accounts,
                     /*complete_signin_operation=*/true);
  observer.WaitForAccountChanges(previously_signed_in_accounts + 1,
                                 PrimaryAccountWait::kWaitForAdded);

  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    history_sync_observer->Wait();
    switch (sync_choice) {
      case SyncChoice::kAcceptAllOptionalDataTypesSync:
        EXPECT_TRUE(login_ui_test_utils::ConfirmHistorySyncOptinDialog(
            browser_.Run(), kDialogTimeout));
        break;
      case SyncChoice::kRejectOptionalDateTypesSync:
        EXPECT_TRUE(login_ui_test_utils::RejectHistorySyncOptinDialog(
            browser_.Run(), kDialogTimeout));
        break;
    }
  } else {
    sync_confirmation_observer->Wait();
    switch (sync_choice) {
      case SyncChoice::kAcceptAllOptionalDataTypesSync:
        EXPECT_TRUE(login_ui_test_utils::ConfirmSyncConfirmationDialog(
            browser_.Run(), kDialogTimeout));
        break;
      case SyncChoice::kRejectOptionalDateTypesSync:
        EXPECT_TRUE(login_ui_test_utils::CancelSyncConfirmationDialog(
            browser_.Run(), kDialogTimeout));
        break;
    }
  }
#endif  // BUILDFLAG(!ENABLE_DICE_SUPPORT)
}

void SignInFunctions::SignInFromCurrentPage(
    content::WebContents* web_contents,
    const TestAccountSigninCredentials& test_account,
    int previously_signed_in_accounts) {
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SignInTestObserver observer(IdentityManagerFactory::GetForProfile(profile),
                              AccountReconcilorFactory::GetForProfile(profile),
                              ConsentLevel::kSignin);
  login_ui_test_utils::ExecuteJsToSigninInSigninFrame(
      web_contents, test_account.user, test_account.password);
  observer.WaitForAccountChanges(previously_signed_in_accounts + 1,
                                 PrimaryAccountWait::kNotWait);
}

void SignInFunctions::SignOutFromWeb() {
  SignInTestObserver observer(identity_manager(browser_.Run()),
                              account_reconcilor(browser_.Run()),
                              ConsentLevel::kSignin);
  ASSERT_TRUE(
      add_tab_function_.Run(0, GaiaUrls::GetInstance()->service_logout_url(),
                            ui::PageTransition::PAGE_TRANSITION_TYPED));
  observer.WaitForAccountChanges(0, PrimaryAccountWait::kNotWait);
}

void SignInFunctions::SignOut() {
#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
  NOTREACHED();
#else
  signin::IdentityManager* id_manager = identity_manager(browser_.Run());
  const CoreAccountId primary_account_id =
      id_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  const bool needs_reauth =
      !id_manager->HasAccountWithRefreshToken(primary_account_id) ||
      id_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id);

  GURL url = GURL(chrome::kChromeUISignoutConfirmationURL);
  content::TestNavigationObserver nav_observer(url);
  nav_observer.StartWatchingNewWebContents();

  SignInTestObserver clear_observer(
      id_manager, account_reconcilor(browser_.Run()), ConsentLevel::kSignin);
  auto* signin_view_controller = SigninViewController::From(browser_.Run());
  signin_view_controller->SignoutOrReauthWithPrompt(
      signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt,
      signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);

  if (!needs_reauth) {
    nav_observer.Wait();

    CHECK(signin_view_controller->ShowsModalDialog());
    SignoutConfirmationUI* signout_confirmation_ui =
        SignoutConfirmationUI::GetForTesting(  // IN-TEST
            signin_view_controller
                ->GetModalDialogWebContentsForTesting());  // IN-TEST
    TestSignoutConfirmationHandlerWaiter handler_observer(
        signout_confirmation_ui);
    handler_observer.Wait();

    signout_confirmation_ui->AcceptDialogForTesting();  // IN-TEST
  }

  clear_observer.WaitForAccountChanges(0, PrimaryAccountWait::kWaitForCleared);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

}  // namespace signin::test
