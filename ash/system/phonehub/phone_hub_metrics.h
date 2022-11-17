// Copyright 2020 The Chromium Authors
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

// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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

// Enumeration of possible interactions with a PhoneHub notification. Keep in
// sync with corresponding enum in tools/metrics/histograms/enums.xml. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class NotificationInteraction {
  kInlineReply = 0,
  kDismiss = 1,
  kMaxValue = kDismiss,
};

// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
enum class CameraRollContentShown {
  kContentShown1 = 1,
  kContentShown2 = 2,
  kContentShown3 = 3,
  kContentShown4 = 4,
  kContentShownGTE5 = 5,
  kMaxValue = kContentShownGTE5
};

// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
enum class CameraRollContentClicked {
  kContentClicked1 = 11,
  kContentClicked2 = 21,
  kContentClicked3 = 31,
  kContentClicked4 = 41,
  kContentClickedGTE5 = 51,
  kMaxValue = kContentClickedGTE5
};

// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
enum class CameraRollContextMenuDownload {
  kDownload1 = 111,
  kDownload2 = 211,
  kDownload3 = 311,
  kDownload4 = 411,
  kDownloadGTE5 = 511,
  kMaxValue = kDownloadGTE5
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

}  // namespace phone_hub_metrics
}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_METRICS_H_
