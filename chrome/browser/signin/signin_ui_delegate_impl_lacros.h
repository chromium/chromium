// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_IMPL_LACROS_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_IMPL_LACROS_H_

#include "chrome/browser/signin/signin_ui_delegate.h"

namespace signin_ui_util {

// Lacros-specific implementation of the SigninUiDelegate interface.
class SigninUiDelegateImplLacros : public SigninUiDelegate {
 public:
  // SigninUiDelegate:
  // Displays the Chrome account picker first, if the system has available
  // accounts. If the user chooses to add a new account or no existing accounts
  // are available, this function will display OS's add account flow.
  void ShowSigninUI(Browser* browser,
                    Profile* profile,
                    bool enable_sync,
                    signin_metrics::AccessPoint access_point,
                    signin_metrics::PromoAction promo_action) override;
  // Displays OS's reauth dialog.
  void ShowReauthUI(Browser* browser,
                    Profile* profile,
                    const std::string& email,
                    bool enable_sync,
                    signin_metrics::AccessPoint access_point,
                    signin_metrics::PromoAction promo_action) override;

 private:
  void OnAccountAdded(bool enable_sync,
                      base::WeakPtr<Browser> browser_weak,
                      const base::FilePath& profile_path,
                      signin_metrics::AccessPoint access_point,
                      signin_metrics::PromoAction promo_action,
                      const CoreAccountId& account_id);
};

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_IMPL_LACROS_H_
