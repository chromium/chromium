// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_INTERCEPTED_SESSION_STARTUP_HELPER_H_
#define CHROME_BROWSER_SIGNIN_DICE_INTERCEPTED_SESSION_STARTUP_HELPER_H_

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace signin {
struct AccountsInCookieJarInfo;
}

class GoogleServiceAuthError;
class Profile;

// Called when the user accepted the dice signin interception and the new
// profile has been created. Creates a new browser and moves the intercepted tab
// to the new browser.
// It is assumed that the account is already in the profile, but not necessarily
// in the content area (cookies).
class DiceInterceptedSessionStartupHelper
    : public content::WebContentsObserver,
      public signin::IdentityManager::Observer {
 public:
  // |profile| is the new profile that was created after signin interception.
  // |account_id| is the main account for the profile, it's already in the
  // profile.
  // |tab_to_move| is the tab where the interception happened, in the source
  // profile.
  DiceInterceptedSessionStartupHelper(Profile* profile,
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

 private:
  // Creates a browser with a new tab, and closes the intercepted tab if it's
  // still open.
  void MoveTab();

  Profile* const profile_;
  CoreAccountId account_id_;
  base::OnceClosure callback_;
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      accounts_in_cookie_observer_{this};
  base::TimeTicks session_startup_time_;
  // Timeout while waiting for the account to be added to the cookies in the new
  // profile.
  base::CancelableOnceCallback<void()> on_cookie_update_timeout_;
  GURL url_to_open_;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_INTERCEPTED_SESSION_STARTUP_HELPER_H_
