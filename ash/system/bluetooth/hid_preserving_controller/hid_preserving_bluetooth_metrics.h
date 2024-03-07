// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_METRICS_H_
#define ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_METRICS_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"

namespace ash::bluetooth {

inline constexpr char kPoweredDisableDialogBehavior[] =
    "Bluetooth.ChromeOS.PoweredState.Disable.HidWarningDialogBehavior";
inline constexpr char kUserAction[] =
    "Bluetooth.ChromeOS.DisconnectHidWarningDialog.UserAction";
inline constexpr char kDialogSource[] =
    "Bluetooth.ChromeOS.DisconnectHidWarningDialog.Source";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UserAction { kKeepOn = 0, kTurnOff = 1, kMaxValue = kTurnOff };

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DialogSource {
  kOsSettings = 0,
  kQuickSettings = 1,
  kMaxValue = kQuickSettings
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DisabledBehavior {
  kWarningDialogShown = 0,
  kWarningDialogNotShown = 1,
  kMaxValue = kWarningDialogNotShown
};

ASH_EXPORT void RecordHidPoweredStateDisableBehavior(bool dialog_shown);

ASH_EXPORT void RecordHidWarningUserAction(bool disabled_bluetooth);

ASH_EXPORT void RecordHidWarningDialogSource(
    mojom::HidWarningDialogSource dialog_source);

}  // namespace ash::bluetooth

#endif  // ASH_SYSTEM_BLUETOOTH_HID_PRESERVING_CONTROLLER_HID_PRESERVING_BLUETOOTH_METRICS_H_
