// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_

#include "base/functional/callback_forward.h"

class BrowserWindowInterface;
class Profile;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CrossDeviceSigninPromoEntryPoint)
enum class CrossDeviceSigninPromoEntryPoint {
  kProfileMenu = 0,
  kHistoryPage = 1,
  kMaxValue = kHistoryPage,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:CrossDeviceSigninPromoEntryPoint)

// These values are persisted to UMA logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CrossDeviceSigninPromoShouldShowResult)
enum class CrossDeviceSigninPromoShouldShowResult {
  kCanShow = 0,
  kNotSignedIn = 1,
  kHasOtherDevices = 2,
  kDataTypeNotEnabled = 3,
  kShownLimitReached = 4,
  kCooldownActive = 5,
  kAlreadyShownAfterDismissalLimitReached = 6,
  kMaxValue = kAlreadyShownAfterDismissalLimitReached,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:CrossDeviceSigninPromoShouldShowResult)

// Returns true if the cross-device sign-in promo should be shown.
bool ShouldShowCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile);

// Called when the cross-device sign-in promo is shown to the user.
void OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint entry_point,
                                   Profile* profile);

// Called when the cross-device sign-in promo is dismissed by the user.
void OnCrossDeviceSigninPromoDismissed(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile);

// Opens the "Sign in to phone" QR code bubble associated with the specified
// `browser_window`.
// `closing_callback` is run when the bubble closes.
void OpenSigninToPhoneQrCodeBubble(BrowserWindowInterface* browser_window,
                                   CrossDeviceSigninPromoEntryPoint entry_point,
                                   base::OnceClosure closing_callback);

#endif  // CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_
