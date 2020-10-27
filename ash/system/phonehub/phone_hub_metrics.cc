// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace ash {
namespace phone_hub_metrics {

namespace {

std::string GetInterstitialScreenHistogramName(InterstitialScreen screen) {
  switch (screen) {
    case InterstitialScreen::kConnectionError:
      return "Ash.PhoneHub.InterstitialScreenEvent.ConnectionError";
    case InterstitialScreen::kBluetoothOrWifiDisabled:
      return "Ash.PhoneHub.InterstitialScreenEvent.BluetoothOrWifiDisabled";
    case InterstitialScreen::kReconnecting:
      return "Ash.PhoneHub.InterstitialScreenEvent.Reconnecting";
    case InterstitialScreen::kInitialConnecting:
      return "Ash.PhoneHub.InterstitialScreenEvent.InitialConnecting";
    case InterstitialScreen::kOnboardingExistingMultideviceUser:
      return "Ash.PhoneHub.InterstitialScreenEvent.Onboarding."
             "ExistingMultideviceUser";
    case InterstitialScreen::kOnboardingNewMultideviceUser:
      return "Ash.PhoneHub.InterstitialScreenEvent.Onboarding."
             "NewMultideviceUser";
  }
}

}  // namespace

void LogInterstitialScreenEvent(InterstitialScreen screen,
                                InterstitialScreenEvent event) {
  base::UmaHistogramEnumeration(GetInterstitialScreenHistogramName(screen),
                                event);
}

void LogNotificationOptInEvent(InterstitialScreenEvent event) {
  base::UmaHistogramEnumeration("Ash.PhoneHub.NotificationOptIn", event);
}

void LogTabContinuationChipClicked(int tab_index) {
  base::UmaHistogramCounts100("Ash.PhoneHub.TabContinuationChipClicked",
                              tab_index);
}

void LogQuickActionClick(QuickAction action) {
  base::UmaHistogramEnumeration("Ash.PhoneHub.QuickActionClicked", action);
}

}  // namespace phone_hub_metrics
}  // namespace ash
