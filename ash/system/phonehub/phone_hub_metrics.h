// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_METRICS_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_METRICS_H_

namespace ash {
namespace phone_hub_metrics {

// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
enum class InterstitialScreenEvent {
  kShown = 0,
  kLearnMore = 1,
  kDismiss = 2,
  kConfirm = 3,
  kMaxValue = kConfirm
};

// Keep in sync with corresponding template variants in the
// Ash.PhoneHub.InterstitialEvent.* histogram.
enum class InterstitialScreen {
  kConnectionError = 0,
  kBluetoothOrWifiDisabled,
  kReconnecting,
  kInitialConnecting,
  kOnboardingExistingMultideviceUser,
  kOnboardingNewMultideviceUser
};

// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
enum class QuickAction {
  kToggleHotspotOn = 0,
  kToggleHotspotOff,
  kToggleQuietModeOn,
  kToggleQuietModeOff,
  kToggleLocatePhoneOn,
  kToggleLocatePhoneOff,
  kMaxValue = kToggleLocatePhoneOff
};

// Logs an |event| occurring for the given |interstitial_screen|.
void LogInterstitialScreenEvent(InterstitialScreen screen,
                                InterstitialScreenEvent event);

// Logs an |event| for the notification opt-in prompt.
void LogNotificationOptInEvent(InterstitialScreenEvent event);

// Logs the |tab_index| of the tab continuation chip that was clicked.
void LogTabContinuationChipClicked(int tab_index);

// Logs a given |quick_action| click.
void LogQuickActionClick(QuickAction quick_action);

}  // namespace phone_hub_metrics
}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_METRICS_H_
