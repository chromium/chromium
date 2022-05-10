// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_H_

#include <string>

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "components/signin/public/base/signin_metrics.h"

class Browser;
class Profile;
struct CoreAccountId;

namespace signin_ui_util {

// SigninUiDelegate provides a cross-platform interface for invoking various
// sign-in related UIs.
// Do not use this class directly. Instead, call the functions defined in
// signin_ui_util.cc.
class SigninUiDelegate {
 public:
  // Displays a sign-in prompt to the user.
  // `browser` might be null. In that case, the delegate will find an existing
  // suitable window for `profile` or create a new one if needed.
  // `enable_sync` indicates whether the sync should be enabled after the user
  // successfully signs in.
  virtual void ShowSigninUI(Browser* browser,
                            Profile* profile,
                            bool enable_sync,
                            signin_metrics::AccessPoint access_point,
                            signin_metrics::PromoAction promo_action) = 0;

  // Displays a reauth prompt to the user for an account with indicated `email`.
  // This account should be already known to Chrome.
  // `browser` might be null. In that case, the delegate will find an existing
  // suitable window for `profile` or create a new one if needed.
  // `enable_sync` indicates whether the sync should be enabled after the user
  // successfully re-authenticates.
  // Note: if sync is enabled, `enable_sync` has to be false, as it's not valid
  // to start a new sync setup flow when sync is already enabled.
  virtual void ShowReauthUI(Browser* browser,
                            Profile* profile,
                            const std::string& email,
                            bool enable_sync,
                            signin_metrics::AccessPoint access_point,
                            signin_metrics::PromoAction promo_action) = 0;

  // Displays a sync confirmation dialog to the user for an account with
  // identified by `account_id`. Account must be a valid (have no auth error)
  // account added to `profile`.
  // `browser` might be null. In that case, the delegate will find an existing
  // suitable window for `profile` or create a new one if needed.
  virtual void ShowTurnSyncOnUI(
      Browser* browser,
      Profile* profile,
      signin_metrics::AccessPoint access_point,
      signin_metrics::PromoAction promo_action,
      signin_metrics::Reason signin_reason,
      const CoreAccountId& account_id,
      TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode);

 protected:
  static Browser* EnsureBrowser(Browser* browser, Profile* profile);
};

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_H_
