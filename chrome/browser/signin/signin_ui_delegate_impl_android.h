// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_IMPL_ANDROID_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_IMPL_ANDROID_H_

#include "chrome/browser/signin/signin_ui_delegate.h"

namespace signin_ui_util {

// SigninUiDelegate implementation on Android.
class SigninUiDelegateImplAndroid : public SigninUiDelegate {
 public:
  // SigninUiDelegate:
  void ShowSigninUI(Profile* profile,
                    bool enable_sync,
                    signin_metrics::AccessPoint access_point,
                    signin_metrics::PromoAction promo_action,
                    const std::string& extension_name) override;
  void ShowReauthUI(Profile* profile,
                    const std::string& email,
                    bool enable_sync,
                    signin_metrics::AccessPoint access_point,
                    signin_metrics::PromoAction promo_action) override;
};

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_DELEGATE_IMPL_ANDROID_H_
