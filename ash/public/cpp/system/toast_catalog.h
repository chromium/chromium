// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TOAST_CATALOG_H_
#define ASH_PUBLIC_CPP_SYSTEM_TOAST_CATALOG_H_

namespace ash {

// A living catalog that registers Toast notifiers.
// These values are persisted to metrics. Entries should not be renumbered and
// numeric values should never be reused.
enum class ToastCatalogName {
  kVirtualDesksLimitMax = 0,
  kVirtualDesksLimitMin = 1,
  kAssistantError = 2,
  kDebugCommand = 3,
  kAssistantUnboundService = 4,
  kStylusPrompt = 5,
  kAppResizable = 6,
  kKioskAppError = 7,
  kBluetoothDevicePaired = 8,
  kBluetoothDeviceDisconnected = 9,
  kBluetoothDeviceConnected = 10,
  kBluetoothAdapterDiscoverable = 11,
  kEncourageUnlock = 12,
  kNetworkAutoConnect = 13,
  kAssistantLoading = 14,
  kToastManagerUnittest = 15,
  kMaximumDeskLaunchTemplate = 16,
  kEnterOverviewGesture = 17,
  kExitOverviewGesture = 18,
  kNextDeskGesture = 19,
  kPreviousDeskGesture = 20,
  kMoveVisibleOnAllDesksWindow = 21,
  kAppCannotSnap = 22,
  kCrostiniUnsupportedVirtualKeyboard = 23,
  kCrostiniUnsupportedIME = 24,
  kCopyToClipboardShareAction = 25,
  kClipboardBlockedAction = 26,
  kClipboardWarnOnPaste = 27,
  kAppNotAvailable = 28,
  kCameraPrivacySwitchOff = 29,
  kCameraPrivacySwitchOn = 30,
  kExtensionInstallSuccess = 31,
  kAccountRemoved = 32,
  kDeskTemplateTooLarge = 33,
  kUndoCloseAll = 34,
  kMaxValue = kUndoCloseAll,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TOAST_CATALOG_H_
