// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace ash::phone_hub_metrics {

namespace {

std::string GetInterstitialScreenEventHistogramName(Screen screen) {
  switch (screen) {
    case Screen::kPhoneDisconnected:
      return "PhoneHub.InterstitialScreenEvent.PhoneDisconnected";
    case Screen::kBluetoothOrWifiDisabled:
      return "PhoneHub.InterstitialScreenEvent.BluetoothOrWifiDisabled";
    case Screen::kPhoneConnecting:
      return "PhoneHub.InterstitialScreenEvent.PhoneConnecting";
    case Screen::kTetherConnectionPending:
      return "PhoneHub.InterstitialScreenEvent.TetherConnectionPending";
    case Screen::kOnboardingExistingMultideviceUser:
      return "PhoneHub.InterstitialScreenEvent.Onboarding."
             "ExistingMultideviceUser2";
    case Screen::kOnboardingNewMultideviceUser:
      return "PhoneHub.InterstitialScreenEvent.Onboarding."
             "NewMultideviceUser2";
    case Screen::kOnboardingDismissPrompt:
      return "PhoneHub.InterstitialScreenEvent.OnboardingDismissPrompt";
    default:
      DCHECK(false) << "Invalid interstitial screen";
      return "";
  }
}

}  // namespace

void LogInterstitialScreenEvent(Screen screen, InterstitialScreenEvent event) {
  base::UmaHistogramEnumeration(GetInterstitialScreenEventHistogramName(screen),
                                event);

  // NOTE(https://crbug.com/1187255): The new- and existing-user metrics were
  // previously reversed. For continuity, we continue logging the old metrics in
  // reverse. The new metrics
  // "PhoneHub.InterstitialScreenEvent.Onboarding.NewMultideviceUser2" and
  // "PhoneHub.InterstitialScreenEvent.Onboarding.ExistingMultideviceUser2" are
  // logged correctly.
  if (screen == Screen::kOnboardingExistingMultideviceUser) {
    base::UmaHistogramEnumeration(
        "PhoneHub.InterstitialScreenEvent.Onboarding.NewMultideviceUser",
        event);
  } else if (screen == Screen::kOnboardingNewMultideviceUser) {
    base::UmaHistogramEnumeration(
        "PhoneHub.InterstitialScreenEvent.Onboarding.ExistingMultideviceUser",
        event);
  }
}

void LogScreenOnBubbleOpen(Screen screen) {
  base::UmaHistogramEnumeration("PhoneHub.ScreenOnBubbleOpen", screen);
}

void LogScreenOnBubbleClose(Screen screen) {
  base::UmaHistogramEnumeration("PhoneHub.ScreenOnBubbleClose", screen);
}

void LogScreenOnSettingsButtonClicked(Screen screen) {
  base::UmaHistogramEnumeration("PhoneHub.ScreenOnSettingsButtonClicked",
                                screen);
}

void LogTabContinuationChipClicked(int tab_index) {
  base::UmaHistogramCounts100("PhoneHub.TabContinuationChipClicked", tab_index);
}

void LogQuickActionClick(QuickAction action) {
  base::UmaHistogramEnumeration("PhoneHub.QuickActionClicked", action);
}

void LogNotificationCount(int count) {
  base::UmaHistogramCounts100("PhoneHub.NotificationCount", count);
}

void LogNotificationInteraction(NotificationInteraction interaction) {
  base::UmaHistogramEnumeration("PhoneHub.NotificationInteraction",
                                interaction);
}

void LogNotificationMessageLength(int length) {
  base::UmaHistogramCounts10000("PhoneHub.NotificationMessageLength", length);
}

std::string GetCameraRollMediaTypeSubcategoryName(
    CameraRollMediaType mediaType) {
  switch (mediaType) {
    case CameraRollMediaType::kPhoto:
      return ".Photo";
    case CameraRollMediaType::kVideo:
      return ".Video";
    default:
      DCHECK(false) << "Invalid Camera Roll media type";
      return "";
  }
}

void LogCameraRollContentShown(int index, CameraRollMediaType mediaType) {
  std::string subcategory = GetCameraRollMediaTypeSubcategoryName(mediaType);
  switch (index) {
    case 0:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Shown" + subcategory,
          static_cast<int>(CameraRollContentShown::kContentShown1));
      break;
    case 1:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Shown" + subcategory,
          static_cast<int>(CameraRollContentShown::kContentShown2));
      break;
    case 2:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Shown" + subcategory,
          static_cast<int>(CameraRollContentShown::kContentShown3));
      break;
    case 3:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Shown" + subcategory,
          static_cast<int>(CameraRollContentShown::kContentShown4));
      break;
    default:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Shown" + subcategory,
          static_cast<int>(CameraRollContentShown::kContentShownGTE5));
      break;
  }
}

void LogCameraRollContentClicked(int index, CameraRollMediaType mediaType) {
  std::string subcategory = GetCameraRollMediaTypeSubcategoryName(mediaType);
  switch (index) {
    case 0:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Clicked" + subcategory,
          static_cast<int>(CameraRollContentClicked::kContentClicked1));
      break;
    case 1:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Clicked" + subcategory,
          static_cast<int>(CameraRollContentClicked::kContentClicked2));
      break;
    case 2:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Clicked" + subcategory,
          static_cast<int>(CameraRollContentClicked::kContentClicked3));
      break;
    case 3:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Clicked" + subcategory,
          static_cast<int>(CameraRollContentClicked::kContentClicked4));
      break;
    default:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.Content.Clicked" + subcategory,
          static_cast<int>(CameraRollContentClicked::kContentClickedGTE5));
      break;
  }
}

void LogCameraRollContextMenuDownload(int index,
                                      CameraRollMediaType mediaType) {
  std::string subcategory = GetCameraRollMediaTypeSubcategoryName(mediaType);
  switch (index) {
    case 0:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.ContextMenu.Download" + subcategory,
          static_cast<int>(CameraRollContextMenuDownload::kDownload1));
      break;
    case 1:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.ContextMenu.Download" + subcategory,
          static_cast<int>(CameraRollContextMenuDownload::kDownload2));
      break;
    case 2:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.ContextMenu.Download" + subcategory,
          static_cast<int>(CameraRollContextMenuDownload::kDownload3));
      break;
    case 3:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.ContextMenu.Download" + subcategory,
          static_cast<int>(CameraRollContextMenuDownload::kDownload4));
      break;
    default:
      base::UmaHistogramSparse(
          "PhoneHub.CameraRoll.ContextMenu.Download" + subcategory,
          static_cast<int>(CameraRollContextMenuDownload::kDownloadGTE5));
      break;
  }
}

void LogCameraRollContentPresent() {
  base::UmaHistogramBoolean("PhoneHub.CameraRoll.Content.Present", true);
}

void LogMoreAppsButtonAnimationOnShow(
    MoreAppsButtonLoadingState loading_state) {
  base::UmaHistogramEnumeration("PhoneHub.MoreAppsButton.LoadingState",
                                loading_state);
}

void LogMoreAppsButtonFullAppsLatency(const base::TimeDelta latency) {
  base::UmaHistogramTimes("PhoneHub.LauncherButton.Loading.Latency", latency);
}

void LogRecentAppsStateOnBubbleOpened(RecentAppsUiState ui_state) {
  switch (ui_state) {
    case RecentAppsUiState::HIDDEN:
      [[fallthrough]];
    case RecentAppsUiState::PLACEHOLDER_VIEW:
      break;
    case RecentAppsUiState::LOADING:
      base::UmaHistogramEnumeration("PhoneHub.RecentApps.State.OnBubbleOpened",
                                    RecentAppsViewUiState::kLoading);
      break;
    case RecentAppsUiState::CONNECTION_FAILED:
      base::UmaHistogramEnumeration("PhoneHub.RecentApps.State.OnBubbleOpened",
                                    RecentAppsViewUiState::kError);
      break;
    case RecentAppsUiState::ITEMS_VISIBLE:
      base::UmaHistogramEnumeration("PhoneHub.RecentApps.State.OnBubbleOpened",
                                    RecentAppsViewUiState::kApps);
      break;
    default:
      break;
  }
}

void LogRecentAppsTransitionToFailedLatency(const base::TimeDelta latency) {
  base::UmaHistogramTimes("PhoneHub.RecentApps.TransitionToFailed.Latency",
                          latency);
}

void LogRecentAppsTransitionToSuccessLatency(const base::TimeDelta latency) {
  base::UmaHistogramTimes("PhoneHub.RecentApps.TransitionToSuccess.Latency",
                          latency);
}

void LogMultiDeviceSetupNotificationInteraction() {
  base::UmaHistogramCounts100("MultiDeviceSetup.NotificationInteracted", 1);
}

}  // namespace ash::phone_hub_metrics
