// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TOAST_CATALOG_H_
#define ASH_PUBLIC_CPP_SYSTEM_TOAST_CATALOG_H_

namespace ash {

// A living catalog that registers Toast notifiers.
// CatalogNames can be shared between semantically similar notifiers, but opt
// for unique names when possible for more specific identification and metrics.
// TODO(crbug/1287561): Implement metrics for toasts using their CatalogNames.
// New enumerators can be added to the end, but existing items must not be
// removed or reordered.
enum class ToastCatalogName {
  kVirtualDesksLimitMax = 0,
  kVirtualDesksLimitMin,
  kAssistantError,
  kDebugCommand,
  kAssistantUnboundService,
  kStylusPrompt,
  kAppResizable,
  kKioskAppError,
  kBluetoothDevicePaired,
  kBluetoothDeviceDisconnected,
  kBluetoothDeviceConnected,
  kBluetoothAdapterDiscoverable,
  kEncourageUnlock,
  kNetworkAutoConnect,
  kAssistantLoading,
  kToastManagerUnittest,
  kMaximumDeskLaunchTemplate,
  kEnterOverviewGesture,
  kExitOverviewGesture,
  kNextDeskGesture,
  kPreviousDeskGesture,
  kMoveVisibleOnAllDesksWindow,
  kAppCannotSnap,
  kCrostiniUnsupportedVirtualKeyboard,
  kCrostiniUnsupportedIME,
  kCopyToClipboardShareAction,
  kClipboardBlockedAction,
  kClipboardWarnOnPaste,
  kAppNotAvailable,
  kCameraPrivacySwitchOff,
  kCameraPrivacySwitchOn,
  kExtensionInstallSuccess,
  kAccountRemoved,
  kDeskTemplateTooLarge,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TOAST_CATALOG_H_
