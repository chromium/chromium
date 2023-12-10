// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_METRICS_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_METRICS_H_

#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"

namespace ash::phone_hub_metrics {

using RecentAppsUiState =
    phonehub::RecentAppsInteractionHandler::RecentAppsUiState;

// Keep in sync with corresponding PhoneHubInterstitialScreenEvent enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml.
enum class InterstitialScreenEvent {
  kShown = 0,
  kLearnMore = 1,
  kDismiss = 2,
  kConfirm = 3,
  kMaxValue = kConfirm
};

// Keep in sync with corresponding PhoneHubScreen enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
// Note that value 2 and 3 have been deprecated and should not be reused.
enum class Screen {
  kBluetoothOrWifiDisabled = 0,
  kPhoneDisconnected = 1,
  kOnboardingExistingMultideviceUser = 4,
  kOnboardingNewMultideviceUser = 5,
  kPhoneConnected = 6,
  kOnboardingDismissPrompt = 7,
  kInvalid = 8,
  kPhoneConnecting = 9,
  kTetherConnectionPending = 10,
  kMiniLauncher = 11,
  kMaxValue = kMiniLauncher
};

// Keep in sync with corresponding PhoneHubQuickAction enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml.
enum class QuickAction {
  kToggleHotspotOn = 0,
  kToggleHotspotOff,
  kToggleQuietModeOn,
  kToggleQuietModeOff,
  kToggleLocatePhoneOn,
  kToggleLocatePhoneOff,
  kMaxValue = kToggleLocatePhoneOff
};

// Enumeration of possible interactions with a PhoneHub notification. Keep in
// sync with corresponding PhoneHubNotificationInteraction enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml. Values are persisted
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class NotificationInteraction {
  kInlineReply = 0,
  kDismiss = 1,
  kMaxValue = kDismiss,
};

// Keep in sync with corresponding PhoneHubCameraRollContentShown enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml.
enum class CameraRollContentShown {
  kContentShown1 = 1,
  kContentShown2 = 2,
  kContentShown3 = 3,
  kContentShown4 = 4,
  kContentShownGTE5 = 5,
  kMaxValue = kContentShownGTE5
};

// Keep in sync with corresponding PhoneHubCameraRollContentClicked enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml.
enum class CameraRollContentClicked {
  kContentClicked1 = 11,
  kContentClicked2 = 21,
  kContentClicked3 = 31,
  kContentClicked4 = 41,
  kContentClickedGTE5 = 51,
  kMaxValue = kContentClickedGTE5
};

// Keep in sync with corresponding PhoneHubCameraRollContextMenuDownload enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml.
enum class CameraRollContextMenuDownload {
  kDownload1 = 111,
  kDownload2 = 211,
  kDownload3 = 311,
  kDownload4 = 411,
  kDownloadGTE5 = 511,
  kMaxValue = kDownloadGTE5
};

// Keep in sync with corresponding MoreAppsButtonLoadingState enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml.
enum class MoreAppsButtonLoadingState {
  kAnimationShown = 0,
  kMoreAppsButtonLoaded = 1,
  kMaxValue = kMoreAppsButtonLoaded
};

// Keep in sync with corresponding RecentAppsViewUiState enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml.
enum class RecentAppsViewUiState {
  kLoading = 0,
  kError = 1,
  kApps = 2,
  kMaxValue = kApps,
};

// Keep in sync with MultideviceSetupNudgeInteraction enum in
// //tools/metrics/histograms/enums.xml.
enum class MultideviceSetupNudgeInteraction {
  kNudgeClicked = 0,
  kPhoneHubIconClicked = 1,
  kMaxValue = kPhoneHubIconClicked,
};

enum class CameraRollMediaType { kPhoto = 0, kVideo = 1, kMaxValue = kVideo };

// Logs an |event| occurring for the given |interstitial_screen|.
void LogInterstitialScreenEvent(Screen screen, InterstitialScreenEvent event);

// Logs the |screen| when the PhoneHub bubble opens.
void LogScreenOnBubbleOpen(Screen screen);

// Logs the |screen| when the PhoneHub bubble closes.
void LogScreenOnBubbleClose(Screen screen);

// Logs the |screen| when the settings button is clicked.
void LogScreenOnSettingsButtonClicked(Screen screen);

// Logs the |tab_index| of the tab continuation chip that was clicked.
void LogTabContinuationChipClicked(int tab_index);

// Logs a given |quick_action| click.
void LogQuickActionClick(QuickAction quick_action);

// Logs the number of PhoneHub notifications after one is added or removed.
void LogNotificationCount(int count);

// Logs a given |interaction| with a PhoneHub notification.
void LogNotificationInteraction(NotificationInteraction interaction);

// Logs the message length of a PhoneHub notification.
void LogNotificationMessageLength(int length);

// Logs the display of a Camera Roll item at |index|.
void LogCameraRollContentShown(int index, CameraRollMediaType mediaType);

// Logs clicking a Camera Roll item at |index|.
void LogCameraRollContentClicked(int index, CameraRollMediaType mediaType);

// Logs a download of item at |index| from the Camera Roll context menu.
void LogCameraRollContextMenuDownload(int index, CameraRollMediaType mediaType);

// Logs the display of any Camera Roll item. Emits once per opening of bubble.
void LogCameraRollContentPresent();

// Logs if the glimmer animation was shown or not (more apps button was shown
// instead) when Phone Hub is opened.
void LogMoreAppsButtonAnimationOnShow(MoreAppsButtonLoadingState loading_state);

// Logs the time latency from initializing the More Apps button and when we
// receive/load the full apps list.
void LogMoreAppsButtonFullAppsLatency(const base::TimeDelta latency);

// Logs the recent apps UI state when the Phone Hub bubble is opened.
void LogRecentAppsStateOnBubbleOpened(RecentAppsUiState ui_state);

// Logs the latency between showing the loading animation in the Recent Apps
// view to the connection failed error button.
void LogRecentAppsTransitionToFailedLatency(const base::TimeDelta latency);

// Logs the latency between showing the loading animation in the Recent Apps
// view to showing the recent apps icons and more apps button.
void LogRecentAppsTransitionToSuccessLatency(const base::TimeDelta latency);

// Logs the interaction with Multidedevice setup notification when it is
// visible.
void LogMultiDeviceSetupNotificationInteraction();

}  // namespace ash::phone_hub_metrics

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_METRICS_H_
