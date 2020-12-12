// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace ash {
namespace phone_hub_metrics {

namespace {

std::string GetInterstitialScreenEventHistogramName(Screen screen) {
  switch (screen) {
    case Screen::kConnectionError:
      return "Ash.PhoneHub.InterstitialScreenEvent.ConnectionError";
    case Screen::kBluetoothOrWifiDisabled:
      return "Ash.PhoneHub.InterstitialScreenEvent.BluetoothOrWifiDisabled";
    case Screen::kReconnecting:
      return "Ash.PhoneHub.InterstitialScreenEvent.Reconnecting";
    case Screen::kInitialConnecting:
      return "Ash.PhoneHub.InterstitialScreenEvent.InitialConnecting";
    case Screen::kOnboardingExistingMultideviceUser:
      return "Ash.PhoneHub.InterstitialScreenEvent.Onboarding."
             "ExistingMultideviceUser";
    case Screen::kOnboardingNewMultideviceUser:
      return "Ash.PhoneHub.InterstitialScreenEvent.Onboarding."
             "NewMultideviceUser";
    case Screen::kOnboardingDismissPrompt:
      return "Ash.PhoneHub.InterstitialScreenEvent.OnboardingDismissPrompt";
    default:
      DCHECK(false) << "Invalid interstitial screen";
      return "";
  }
}

}  // namespace

void LogInterstitialScreenEvent(Screen screen, InterstitialScreenEvent event) {
  base::UmaHistogramEnumeration(GetInterstitialScreenEventHistogramName(screen),
                                event);
}

void LogScreenOnBubbleOpen(Screen screen) {
  base::UmaHistogramEnumeration("Ash.PhoneHub.ScreenOnBubbleOpen", screen);
}

void LogScreenOnBubbleClose(Screen screen) {
  base::UmaHistogramEnumeration("Ash.PhoneHub.ScreenOnBubbleClose", screen);
}

void LogScreenOnSettingsButtonClicked(Screen screen) {
  base::UmaHistogramEnumeration("Ash.PhoneHub.Screen.OnSettingsButtonClicked",
                                screen);
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

void LogNotificationCount(int count) {
  base::UmaHistogramCounts100("Ash.PhoneHub.NotificationCount", count);
}

void LogNotificationInteraction(NotificationInteraction interaction) {
  base::UmaHistogramEnumeration("PhoneHub.NotificationInteraction",
                                interaction);
}

}  // namespace phone_hub_metrics
}  // namespace ash
