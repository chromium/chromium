// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

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

}  // namespace

DiceInterceptedSessionStartupHelper::DiceInterceptedSessionStartupHelper(
    Profile* profile,
    CoreAccountId account_id,
    content::WebContents* tab_to_move)
    : profile_(profile), account_id_(account_id) {
  Observe(tab_to_move);
  if (tab_to_move)
    url_to_open_ = tab_to_move->GetURL();
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
    MoveTab();
  } else {
    // TODO(https://crbug.com/1051864): cookie notifications are not triggered
    // when the account is added by the reconcilor. Force an explicit cookie
    // update.
    identity_manager->GetAccountsCookieMutator()->TriggerCookieJarUpdate();

    accounts_in_cookie_observer_.Add(identity_manager);
    on_cookie_update_timeout_.Reset(base::BindOnce(
        &DiceInterceptedSessionStartupHelper::MoveTab, base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, on_cookie_update_timeout_.callback(),
        base::TimeDelta::FromSeconds(5));
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
  MoveTab();
}

void DiceInterceptedSessionStartupHelper::MoveTab() {
  accounts_in_cookie_observer_.RemoveAll();
  on_cookie_update_timeout_.Cancel();

  // If the intercepted web contents is still alive, close it now.
  if (web_contents()) {
    // Update the URL once again to catch any potential navigation happening
    // while the cookie was updated.
    url_to_open_ = web_contents()->GetURL();
    web_contents()->Close();
  }

  // Open a new browser.
  NavigateParams params(profile_, url_to_open_,
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  Navigate(&params);

  base::UmaHistogramTimes("Signin.Intercept.SessionStartupDuration",
                          base::TimeTicks::Now() - session_startup_time_);

  if (callback_)
    std::move(callback_).Run();
}
