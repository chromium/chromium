// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace {

// Returns true if |account_id| is signed in the cookies.
bool CookieInfoContains(const signin::AccountsInCookieJarInfo& cookie_info,
                        const CoreAccountId& account_id) {
  const std::vector<gaia::ListedAccount>& accounts =
      cookie_info.GetPotentiallyInvalidSignedInAccounts();
  return base::Contains(accounts, account_id, &gaia::ListedAccount::id);
}

}  // namespace

DiceInterceptedSessionStartupHelper::DiceInterceptedSessionStartupHelper(
    Profile* profile,
    bool is_new_profile,
    CoreAccountId account_id,
    content::WebContents* tab_to_move)
    : profile_(profile),
      use_multilogin_(is_new_profile),
      account_id_(account_id) {
  if (tab_to_move)
    web_contents_ = tab_to_move->GetWeakPtr();
}

DiceInterceptedSessionStartupHelper::~DiceInterceptedSessionStartupHelper() =
    default;

void DiceInterceptedSessionStartupHelper::Startup(base::OnceClosure callback) {
  callback_ = std::move(callback);

  // Wait until the account is set in cookies of the newly created profile
  // before opening the URL, so that the user is signed-in in content area. If
  // the account is still not in the cookie after some timeout, proceed without
  // cookies, so that the user can at least take some action in the new profile.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  signin::AccountsInCookieJarInfo cookie_info =
      identity_manager->GetAccountsInCookieJar();
  if (cookie_info.AreAccountsFresh() &&
      CookieInfoContains(cookie_info, account_id_)) {
    MoveTab();
  } else {
    // Set the timeout.
    on_cookie_update_timeout_.Reset(base::BindOnce(
        &DiceInterceptedSessionStartupHelper::MoveTab, base::Unretained(this)));
    // Adding accounts to the cookies can be an expensive operation. In
    // particular the ExternalCCResult fetch may time out after multiple seconds
    // (see kExternalCCResultTimeoutSeconds and https://crbug.com/750316#c37).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, on_cookie_update_timeout_.callback(), base::Seconds(12));

    accounts_in_cookie_observer_.Observe(identity_manager);
    if (use_multilogin_)
      StartupMultilogin(identity_manager);
    else
      StartupReconcilor(identity_manager);
  }
}

void DiceInterceptedSessionStartupHelper::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (error != GoogleServiceAuthError::AuthErrorNone())
    return;
  if (!accounts_in_cookie_jar_info.AreAccountsFresh()) {
    return;
  }
  if (!CookieInfoContains(accounts_in_cookie_jar_info, account_id_))
    return;

  MoveTab();
}

void DiceInterceptedSessionStartupHelper::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  DCHECK(!use_multilogin_);
  if (state == signin_metrics::AccountReconcilorState::kError) {
    reconcile_error_encountered_ = true;
    return;
  }

  // TODO(crbug.com/40673982): remove this when the cookie updates are
  // correctly sent after reconciliation.
  if (state == signin_metrics::AccountReconcilorState::kOk) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    // GetAccountsInCookieJar() automatically re-schedules a /ListAccounts call
    // if the cookie is not fresh.
    signin::AccountsInCookieJarInfo cookie_info =
        identity_manager->GetAccountsInCookieJar();
    OnAccountsInCookieUpdated(cookie_info,
                              GoogleServiceAuthError::AuthErrorNone());
  }
}

void DiceInterceptedSessionStartupHelper::StartupMultilogin(
    signin::IdentityManager* identity_manager) {
  // Lock the reconcilor to avoid making multiple multilogin calls.
  reconcilor_lock_ = std::make_unique<AccountReconcilor::Lock>(
      AccountReconcilorFactory::GetForProfile(profile_));

  // Start the multilogin call.
  signin::MultiloginParameters params = {
      /*mode=*/gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      /*accounts_to_send=*/{account_id_}};
  identity_manager->GetAccountsCookieMutator()->SetAccountsInCookie(
      params, gaia::GaiaSource::kChrome,
      base::BindOnce(
          &DiceInterceptedSessionStartupHelper::OnSetAccountInCookieCompleted,
          weak_factory_.GetWeakPtr()));
}

void DiceInterceptedSessionStartupHelper::StartupReconcilor(
    signin::IdentityManager* identity_manager) {
  // TODO(crbug.com/40673982): cookie notifications are not triggered
  // when the account is added by the reconcilor. Observe the reconcilor and
  // re-trigger the cookie update when it completes.
  reconcilor_observer_.Observe(
      AccountReconcilorFactory::GetForProfile(profile_));
  identity_manager->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
}

void DiceInterceptedSessionStartupHelper::OnSetAccountInCookieCompleted(
    signin::SetAccountsInCookieResult result) {
  DCHECK(use_multilogin_);
  MoveTab();
}

void DiceInterceptedSessionStartupHelper::MoveTab() {
  accounts_in_cookie_observer_.Reset();
  reconcilor_observer_.Reset();
  on_cookie_update_timeout_.Cancel();
  reconcilor_lock_.reset();

  GURL url_to_open = GURL(chrome::kChromeUINewTabURL);
  // If the intercepted web contents is still alive, close it now.
  if (web_contents_) {
    url_to_open = web_contents_->GetLastCommittedURL();
    web_contents_->Close();
  }

  // Open a new browser.
  NavigateParams params(profile_, url_to_open,
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  Navigate(&params);

  if (callback_)
    std::move(callback_).Run();
}
