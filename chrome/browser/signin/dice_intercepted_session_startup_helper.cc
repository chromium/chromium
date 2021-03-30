// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/multilogin_parameters.h"
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
      cookie_info.signed_in_accounts;
  return std::find_if(accounts.begin(), accounts.end(),
                      [&account_id](const gaia::ListedAccount& account) {
                        return account.id == account_id;
                      }) != accounts.end();
}

void RecordSessionStartupDuration(const std::string& histogram_name,
                                  base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(histogram_name, duration,
                                /*min=*/base::TimeDelta::FromMilliseconds(1),
                                /*max=*/base::TimeDelta::FromSeconds(30), 50);
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
  Observe(tab_to_move);
}

DiceInterceptedSessionStartupHelper::~DiceInterceptedSessionStartupHelper() =
    default;

void DiceInterceptedSessionStartupHelper::Startup(base::OnceClosure callback) {
  callback_ = std::move(callback);
  session_startup_time_ = base::TimeTicks::Now();

  // Wait until the account is set in cookies of the newly created profile
  // before opening the URL, so that the user is signed-in in content area. If
  // the account is still not in the cookie after some timeout, proceed without
  // cookies, so that the user can at least take some action in the new profile.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  signin::AccountsInCookieJarInfo cookie_info =
      identity_manager->GetAccountsInCookieJar();
  if (cookie_info.accounts_are_fresh &&
      CookieInfoContains(cookie_info, account_id_)) {
    MoveTab(use_multilogin_ ? Result::kMultiloginNothingToDo
                            : Result::kReconcilorNothingToDo);
  } else {
    // Set the timeout.
    on_cookie_update_timeout_.Reset(base::BindOnce(
        &DiceInterceptedSessionStartupHelper::MoveTab, base::Unretained(this),
        use_multilogin_ ? Result::kMultiloginTimeout
                        : Result::kReconcilorTimeout));
    // Adding accounts to the cookies can be an expensive operation. In
    // particular the ExternalCCResult fetch may time out after multiple seconds
    // (see kExternalCCResultTimeoutSeconds and https://crbug.com/750316#c37).
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, on_cookie_update_timeout_.callback(),
        base::TimeDelta::FromSeconds(12));

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
  if (!accounts_in_cookie_jar_info.accounts_are_fresh)
    return;
  if (!CookieInfoContains(accounts_in_cookie_jar_info, account_id_))
    return;

  MoveTab(use_multilogin_ ? Result::kMultiloginOtherSuccess
                          : Result::kReconcilorSuccess);
}

void DiceInterceptedSessionStartupHelper::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  DCHECK(!use_multilogin_);
  if (state == signin_metrics::ACCOUNT_RECONCILOR_ERROR) {
    reconcile_error_encountered_ = true;
    return;
  }

  // TODO(https://crbug.com/1051864): remove this when the cookie updates are
  // correctly sent after reconciliation.
  if (state == signin_metrics::ACCOUNT_RECONCILOR_OK) {
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
  // TODO(https://crbug.com/1051864): cookie notifications are not triggered
  // when the account is added by the reconcilor. Observe the reconcilor and
  // re-trigger the cookie update when it completes.
  reconcilor_observer_.Observe(
      AccountReconcilorFactory::GetForProfile(profile_));
  identity_manager->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
}

void DiceInterceptedSessionStartupHelper::OnSetAccountInCookieCompleted(
    signin::SetAccountsInCookieResult result) {
  DCHECK(use_multilogin_);
  Result session_startup_result = Result::kMultiloginOtherSuccess;
  switch (result) {
    case signin::SetAccountsInCookieResult::kSuccess:
      session_startup_result = Result::kMultiloginSuccess;
      break;
    case signin::SetAccountsInCookieResult::kTransientError:
      session_startup_result = Result::kMultiloginTransientError;
      break;
    case signin::SetAccountsInCookieResult::kPersistentError:
      session_startup_result = Result::kMultiloginPersistentError;
      break;
  }

  MoveTab(session_startup_result);
}

void DiceInterceptedSessionStartupHelper::MoveTab(Result result) {
  accounts_in_cookie_observer_.Reset();
  reconcilor_observer_.Reset();
  on_cookie_update_timeout_.Cancel();
  reconcilor_lock_.reset();

  GURL url_to_open = GURL(chrome::kChromeUINewTabURL);
  // If the intercepted web contents is still alive, close it now.
  if (web_contents()) {
    url_to_open = web_contents()->GetURL();
    web_contents()->Close();
  }

  // Open a new browser.
  NavigateParams params(profile_, url_to_open,
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  Navigate(&params);

  base::UmaHistogramEnumeration("Signin.Intercept.SessionStartupResult",
                                result);
  base::TimeDelta duration = base::TimeTicks::Now() - session_startup_time_;
  if (use_multilogin_) {
    RecordSessionStartupDuration(
        "Signin.Intercept.SessionStartupDuration.Multilogin", duration);
  } else {
    RecordSessionStartupDuration(
        "Signin.Intercept.SessionStartupDuration.Reconcilor", duration);
    // TODO(https://crbug.com/1151313): Remove this histogram when the cause
    // for the timeouts is understood.
    base::UmaHistogramBoolean("Signin.Intercept.SessionStartupReconcileError",
                              reconcile_error_encountered_);
  }

  if (callback_)
    std::move(callback_).Run();
}
