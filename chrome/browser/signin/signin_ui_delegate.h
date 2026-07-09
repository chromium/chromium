// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_H_

#include <string>
#include <type_traits>

#include "base/functional/callback_forward.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#endif

class BrowserWindowInterface;
class Profile;
struct CoreAccountId;

namespace signin_ui_util {

// SigninUiDelegate provides a cross-platform interface for invoking various
// sign-in related UIs.
// Do not use this class directly. Instead, call the functions defined in
// signin_ui_util.cc.
// TODO(crbug.com/530902365): Create a centralized mock for this class to make
// updates easier.
class SigninUiDelegate {
 public:
  // Displays a sign-in prompt to the user.
  // `enable_sync` indicates whether the sync should be enabled after the user
  // successfully signs in. When this prompt is displayed for extensions, we
  // also pass the `extension_name`.
  virtual void ShowSigninUI(Profile* profile,
                            bool enable_sync,
                            signin_metrics::AccessPoint access_point,
                            signin_metrics::PromoAction promo_action,
                            const std::string& extension_name) = 0;

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

#if !BUILDFLAG(IS_ANDROID)
  // Displays a sync confirmation dialog to the user for an account with
  // identified by `account_id`. Account must be a valid (have no auth error)
  // account added to `profile`.
  virtual void ShowTurnSyncOnUI(
      Profile* profile,
      signin_metrics::AccessPoint access_point,
      signin_metrics::PromoAction promo_action,
      const CoreAccountId& account_id,
      TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
      bool is_sync_promo,
      bool user_already_signed_in);

  // Displays a history sync opt-in dialog to the user for an account
  // identified by `account_id`. Account must be a valid (have no auth error).
  // Virtual for testing purpose.
  virtual void ShowHistorySyncOptinUI(Profile* profile,
                                      const CoreAccountId& account_id,
                                      signin_metrics::AccessPoint access_point);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  virtual void ShowCrossDeviceSigninQrBubble(
      BrowserWindowInterface* browser,
      base::OnceClosure closing_callback) = 0;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

 protected:
  static BrowserWindowInterface* EnsureBrowser(Profile* profile);
#endif  // !BUILDFLAG(IS_ANDROID)
};

static_assert(std::is_trivially_destructible_v<SigninUiDelegate>,
              "SigninUiDelegate must remain trivially destructible to be "
              "statically defined!");

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_H_
