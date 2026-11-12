// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/signin/cross_device_signin_qr_bubble.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "url/gurl.h"

namespace signin_ui_util {

namespace {

void ShowDiceTab(Profile* profile,
                 const std::string& email,
                 bool enable_history_sync,
                 signin_metrics::AccessPoint access_point,
                 signin_metrics::PromoAction promo_action) {
  CHECK(profile);

  // Extensions don't activate an existing signin tab, to prevent misbehaving
  // extensions from keeping it focused, so return before
  // `ScopedTabbedBrowserDisplayer` shows a window.
  if (access_point == signin_metrics::AccessPoint::kExtensions &&
      SigninViewController::HasDiceSigninTabForExtensions(
          profile, enable_history_sync, email)) {
    return;
  }

  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  BrowserWindowInterface* browser = displayer.browser_window_interface();
  // Failed to create a browser. Bailing.
  if (!browser)
    return;

  if (enable_history_sync) {
    SigninViewController::From(browser)->ShowEnableHistorySyncTab(
        access_point, promo_action, email);
  } else {
    SigninViewController::From(browser)->ShowDiceAddAccountTab(access_point,
                                                               email);
  }
}

}  // namespace

void SigninUiDelegateImplDice::ShowSigninUI(
    Profile* profile,
    bool enable_history_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& extension_name) {
  ShowDiceTab(profile, /*email=*/std::string(), enable_history_sync,
              access_point, promo_action);
}

void SigninUiDelegateImplDice::ShowReauthUI(
    Profile* profile,
    const std::string& email,
    bool enable_history_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action) {
  ShowDiceTab(profile, email, enable_history_sync, access_point, promo_action);
}

void SigninUiDelegateImplDice::ShowCrossDeviceSigninQrBubble(
    BrowserWindowInterface* browser,
    GURL qr_code_url,
    base::OnceClosure closing_callback,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  if (!browser) {
    if (closing_callback) {
      std::move(closing_callback).Run();
    }
    return;
  }
  SigninViewController::From(browser)->ShowCrossDeviceSigninQrBubble(
      std::move(qr_code_url), std::move(closing_callback), entry_point);
}

}  // namespace signin_ui_util
