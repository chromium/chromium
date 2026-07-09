// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/cross_device_signin_qr_bubble.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "google_apis/gaia/core_account_id.h"

namespace signin_ui_util {

namespace {

void ShowDiceTab(BrowserWindowInterface* browser,
                 const std::string& email,
                 bool enable_sync,
                 signin_metrics::AccessPoint access_point,
                 signin_metrics::PromoAction promo_action) {
  // Failed to create a browser. Bailing.
  if (!browser)
    return;

  if (enable_sync) {
    browser->GetFeatures().signin_view_controller()->ShowDiceEnableSyncTab(
        access_point, promo_action, email);
  } else {
    browser->GetFeatures().signin_view_controller()->ShowDiceAddAccountTab(
        access_point, email);
  }
}

}  // namespace

void SigninUiDelegateImplDice::ShowSigninUI(
    Profile* profile,
    bool enable_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& extension_name) {
  ShowDiceTab(EnsureBrowser(profile), /*email=*/std::string(), enable_sync,
              access_point, promo_action);
}

void SigninUiDelegateImplDice::ShowReauthUI(
    Profile* profile,
    const std::string& email,
    bool enable_sync,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action) {
  DCHECK(profile);

  ShowDiceTab(EnsureBrowser(profile), email, enable_sync, access_point,
              promo_action);
}

void SigninUiDelegateImplDice::ShowCrossDeviceSigninQrBubble(
    BrowserWindowInterface* browser,
    base::OnceClosure closing_callback) {
  if (!browser) {
    if (closing_callback) {
      std::move(closing_callback).Run();
    }
    return;
  }
  browser->GetFeatures()
      .signin_view_controller()
      ->ShowCrossDeviceSigninQrBubble(std::move(closing_callback));
}

}  // namespace signin_ui_util
