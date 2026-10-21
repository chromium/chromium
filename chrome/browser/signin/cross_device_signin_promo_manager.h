// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"

class BrowserWindowInterface;
class Profile;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CrossDeviceSigninPromoEntryPoint)
enum class CrossDeviceSigninPromoEntryPoint {
  kProfileMenu = 0,
  kHistoryPage = 1,
  kSendTabToSelf = 2,
  kWhatsNewPage = 3,
  kMaxValue = kWhatsNewPage,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:CrossDeviceSigninPromoEntryPoint)

// These values are persisted to UMA logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CrossDeviceSigninPromoShouldShowResult)
enum class CrossDeviceSigninPromoShouldShowResult {
  kCanShow = 0,
  kNotSignedIn = 1,
  kHasMobileDevice = 2,
  kDataTypeNotEnabled = 3,
  kShownLimitReached = 4,
  kCooldownActive = 5,
  kAlreadyShownAfterDismissalLimitReached = 6,
  kMaxValue = kAlreadyShownAfterDismissalLimitReached,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:CrossDeviceSigninPromoShouldShowResult)

// Returns whether the cross-device sign-in promo should start being shown at
// `entry_point`: the user is eligible (signed in with no error, without a
// synced mobile device and with the required data types enabled) and the
// impression/dismissal frequency caps allow it. Records the result in the
// `Signin.CrossDeviceSigninPromo.ShouldShowResult.*` histograms.
bool ShouldStartShowingCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile);

// Returns whether an already shown cross-device sign-in promo should keep being
// shown at `entry_point`, i.e. whether the user is still eligible (e.g. has not
// signed in on a mobile device since). Unlike
// `ShouldStartShowingCrossDeviceSigninPromo()`, this ignores the frequency caps
// and records no metrics.
bool ShouldKeepShowingCrossDeviceSigninPromo(
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
// `browser_window`. `closing_callback` is run when the bubble closes.
//
// At most one bubble may be open per profile across all browser windows. If one
// is already open, calls from permanent entry points (`kProfileMenu`,
// `kSendTabToSelf`, `kWhatsNewPage`) close the existing bubble and reopen it in
// `browser_window`, while calls from promo entry points do nothing.
//
// Must be called on the UI thread.
void OpenSigninToPhoneQrCodeBubble(BrowserWindowInterface* browser_window,
                                   CrossDeviceSigninPromoEntryPoint entry_point,
                                   base::OnceClosure closing_callback);

// Returns whether the QR code bubble is currently open for `profile`. Must be
// called on the UI thread. Unlike
// `RegisterCrossDeviceSigninPromoBubbleStateCallback()`, this does not create
// any state on `profile`.
bool IsCrossDeviceSigninPromoBubbleOpen(Profile* profile);

// Registers `callback` to be notified with the new open state whenever the QR
// code bubble opens or closes in `profile`. Must be called on the UI thread;
// `callback` is also always run on the UI thread. Lazily creates the bubble
// state on `profile`. The returned subscription may safely outlive `profile`.
base::CallbackListSubscription
RegisterCrossDeviceSigninPromoBubbleStateCallback(
    Profile* profile,
    base::RepeatingCallback<void(bool)> callback);

#endif  // CHROME_BROWSER_SIGNIN_CROSS_DEVICE_SIGNIN_PROMO_MANAGER_H_
