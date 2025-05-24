// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_INTERCEPTED_SESSION_STARTUP_HELPER_H_
#define CHROME_BROWSER_SIGNIN_DICE_INTERCEPTED_SESSION_STARTUP_HELPER_H_

#include "base/cancelable_callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/core_account_id.h"

namespace content {
class WebContents;
}

namespace signin {
class AccountsInCookieJarInfo;
class IdentityManager;
enum class SetAccountsInCookieResult;
}

class GoogleServiceAuthError;
class Profile;

// Called when the user accepted the dice signin interception and the new
// profile has been created. Creates a new browser and moves the intercepted tab
// to the new browser.
// It is assumed that the account is already in the profile, but not necessarily
// in the content area (cookies).
class DiceInterceptedSessionStartupHelper
    : public signin::IdentityManager::Observer,
      public AccountReconcilor::Observer {
 public:
  // |profile| is the new profile that was created after signin interception.
  // |account_id| is the main account for the profile, it's already in the
  // profile.
  // |tab_to_move| is the tab where the interception happened, in the source
  // profile.
  DiceInterceptedSessionStartupHelper(Profile* profile,
                                      bool is_new_profile,
                                      CoreAccountId account_id,
                                      content::WebContents* tab_to_move);

  ~DiceInterceptedSessionStartupHelper() override;

  DiceInterceptedSessionStartupHelper(
      const DiceInterceptedSessionStartupHelper&) = delete;
  DiceInterceptedSessionStartupHelper& operator=(
      const DiceInterceptedSessionStartupHelper&) = delete;

  // Start up the session. Can only be called once.
  void Startup(base::OnceClosure callback);

  // signin::IdentityManager::Observer:
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // AccountReconcilor::Observer:
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

 private:
  // For new profiles, the account is added directly using multilogin.
  void StartupMultilogin(signin::IdentityManager* identity_manager);

  // For existing profiles, simply wait for the reconcilor to update the
  // accounts.
  void StartupReconcilor(signin::IdentityManager* identity_manager);

  // Called when multilogin completes.
  void OnSetAccountInCookieCompleted(signin::SetAccountsInCookieResult result);

  // Creates a browser with a new tab, and closes the intercepted tab if it's
  // still open.
  void MoveTab();

  const raw_ptr<Profile> profile_;
  base::WeakPtr<content::WebContents> web_contents_;
  bool use_multilogin_;
  CoreAccountId account_id_;
  base::OnceClosure callback_;
  bool reconcile_error_encountered_ = false;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      accounts_in_cookie_observer_{this};
  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      reconcilor_observer_{this};
  std::unique_ptr<AccountReconcilor::Lock> reconcilor_lock_;
  // Timeout while waiting for the account to be added to the cookies in the new
  // profile.
  base::CancelableOnceCallback<void()> on_cookie_update_timeout_;

  base::WeakPtrFactory<DiceInterceptedSessionStartupHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_INTERCEPTED_SESSION_STARTUP_HELPER_H_
