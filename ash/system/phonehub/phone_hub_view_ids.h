// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_VIEW_IDS_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_VIEW_IDS_H_

namespace ash {

// IDs used for the main views that compose the Phone Hub bubble view.
// Use these for easy access to the views during the unittests.
// Note that these IDs are only guaranteed to be unique inside
// the bubble view.
enum PhoneHubViewID {
  // We start from 1 because 0 is the default view ID.
  kPhoneStatusView = 1,
  kQuickActionsView,
  kTaskContinuationView,

  // Notification opt in view and its components.
  kMultideviceFeatureOptInView,

  // Sub feature opt in view buttons
  kSubFeatureOptInConfirmButton,
  kSubFeatureOptInDismissButton,

  // Onboarding view and its components.
  kOnboardingView,
  kOnboardingMainView,
  kOnboardingGetStartedButton,

  // Phone disconnected view and its components.
  kDisconnectedView,
  kDisconnectedLearnMoreButton,
  kDisconnectedRefreshButton,

  // Bluetooth disabled view and its components.
  kBluetoothDisabledView,
  kBluetoothDisabledLearnMoreButton,

  kPhoneConnectedView,
  kPhoneConnectingView,
  kTetherConnectionPendingView,

  kPhoneHubRecentAppsView,

  // Camera roll view and its components.
  kCameraRollView,
  kAppStreamLauncherView,
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_VIEW_IDS_H_
