// Copyright 2022 The Chromium Authors
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
  // `enable_sync` indicates whether the sync should be enabled after the user
  // successfully signs in.
  virtual void ShowSigninUI(Profile* profile,
                            bool enable_sync,
                            signin_metrics::AccessPoint access_point,
                            signin_metrics::PromoAction promo_action) = 0;

  // Displays a reauth prompt to the user for an account with indicated `email`.
  // This account should be already known to Chrome.
  // `enable_sync` indicates whether the sync should be enabled after the user
  // successfully re-authenticates.
  // Note: if sync is enabled, `enable_sync` has to be false, as it's not valid
  // to start a new sync setup flow when sync is already enabled.
  virtual void ShowReauthUI(Profile* profile,
                            const std::string& email,
                            bool enable_sync,
                            signin_metrics::AccessPoint access_point,
                            signin_metrics::PromoAction promo_action) = 0;

  // Displays a sync confirmation dialog to the user for an account with
  // identified by `account_id`. Account must be a valid (have no auth error)
  // account added to `profile`.
  virtual void ShowTurnSyncOnUI(
      Profile* profile,
      signin_metrics::AccessPoint access_point,
      signin_metrics::PromoAction promo_action,
      const CoreAccountId& account_id,
      TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
      bool is_sync_promo);

 protected:
  static Browser* EnsureBrowser(Profile* profile);
};

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_H_
