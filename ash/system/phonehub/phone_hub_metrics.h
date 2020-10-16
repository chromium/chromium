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

// Keep in sync with corresponding suffix in
// tools/metrics/histograms_xml/histogram_suffixes_list.xml.
enum class InterstitialScreen {
  kConnectionError = 0,
  kBluetoothOrWifiDisabled,
  kNotificationOptIn,
  kReconnecting,
  kInitialConnecting,
  kOnboardingExistingMultideviceUser,
  kOnboardingNewMultideviceUser
};

// Logs an |event| occurring for the given |interstitial_screen|.
void LogInterstitialScreenEvent(InterstitialScreen screen,
                                InterstitialScreenEvent event);

}  // namespace phone_hub_metrics
}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_METRICS_H_
