// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

namespace {

enum class KeyboardType {
  kExternal = 0,
  kExternalChromeOS,
  kInternal,
};

enum class PointerSensitivity {
  kLowest = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
  kHighest = 5,
  kMaxValue = kHighest,
};

KeyboardType GetKeyboardType(const mojom::Keyboard& keyboard) {
  if (!keyboard.is_external) {
    return KeyboardType::kInternal;
  } else if (keyboard.meta_key == mojom::MetaKey::kLauncher ||
             keyboard.meta_key == mojom::MetaKey::kSearch) {
    return KeyboardType::kExternalChromeOS;
  } else {
    return KeyboardType::kExternal;
  }
}

}  // namespace

InputDeviceSettingsMetricsManager::InputDeviceSettingsMetricsManager() =
    default;
InputDeviceSettingsMetricsManager::~InputDeviceSettingsMetricsManager() =
    default;

void InputDeviceSettingsMetricsManager::RecordKeyboardInitialMetrics(
    const mojom::Keyboard& keyboard) {
  // Only record the metrics once for each keyboard.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_keyboards_.find(account_id);

  if (iter != recorded_keyboards_.end() &&
      base::Contains(iter->second, keyboard.device_key)) {
    return;
  }
  recorded_keyboards_[account_id].insert(keyboard.device_key);

  const KeyboardType keyboard_type = GetKeyboardType(keyboard);
  switch (keyboard_type) {
    case KeyboardType::kExternal:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Device.Keyboard.External.BlockMetaFKeyRewrites."
          "Initial",
          keyboard.settings->suppress_meta_fkey_rewrites);
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
          keyboard.settings->top_row_are_fkeys);
      break;
    case KeyboardType::kExternalChromeOS:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS."
          "BlockMetaFKeyRewrites.Initial",
          keyboard.settings->suppress_meta_fkey_rewrites);
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
          "Initial",
          keyboard.settings->top_row_are_fkeys);
      break;
    case KeyboardType::kInternal:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
          keyboard.settings->top_row_are_fkeys);
      break;
  }
}

void InputDeviceSettingsMetricsManager::RecordMouseInitialMetrics(
    const mojom::Mouse& mouse) {
  // Only record the metrics once for each mouse.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_mice_.find(account_id);

  if (iter != recorded_mice_.end() &&
      base::Contains(iter->second, mouse.device_key)) {
    return;
  }
  recorded_mice_[account_id].insert(mouse.device_key);

  PointerSensitivity sensitivity =
      static_cast<PointerSensitivity>(mouse.settings->sensitivity);

  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Mouse.AccelerationEnabled.Initial",
      mouse.settings->acceleration_enabled);
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Mouse.ReverseScrolling.Initial",
      mouse.settings->reverse_scrolling);
  base::UmaHistogramEnumeration(
      "ChromeOS.Settings.Device.Mouse.Sensitivity.Initial", sensitivity);
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.Mouse.SwapPrimaryButtons.Initial",
      mouse.settings->swap_right);
  // TODO(yyhyyh@): Add scroll settings metrics.
}

void InputDeviceSettingsMetricsManager::RecordPointingStickInitialMetrics(
    const mojom::PointingStick& pointing_stick) {
  // Only record the metrics once for each pointing stick.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_pointing_sticks_.find(account_id);

  if (iter != recorded_pointing_sticks_.end() &&
      base::Contains(iter->second, pointing_stick.device_key)) {
    return;
  }
  recorded_pointing_sticks_[account_id].insert(pointing_stick.device_key);

  PointerSensitivity sensitivity =
      static_cast<PointerSensitivity>(pointing_stick.settings->sensitivity);

  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.PointingStick.AccelerationEnabled.Initial",
      pointing_stick.settings->acceleration_enabled);
  base::UmaHistogramEnumeration(
      "ChromeOS.Settings.Device.PointingStick.Sensitivity.Initial",
      sensitivity);
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Device.PointingStick.SwapPrimaryButtons.Initial",
      pointing_stick.settings->swap_right);
}

void InputDeviceSettingsMetricsManager::RecordTouchpadInitialMetrics(
    const mojom::Touchpad& touchpad) {
  // Only record the metrics once for each Touchpad.
  const auto account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  auto iter = recorded_touchpads_.find(account_id);

  if (iter != recorded_touchpads_.end() &&
      base::Contains(iter->second, touchpad.device_key)) {
    return;
  }
  recorded_touchpads_[account_id].insert(touchpad.device_key);

  const std::string touchpad_metrics_prefix =
      touchpad.is_external ? "ChromeOS.Settings.Device.Touchpad.External."
                           : "ChromeOS.Settings.Device.Touchpad.Internal.";
  PointerSensitivity sensitivity =
      static_cast<PointerSensitivity>(touchpad.settings->sensitivity);

  base::UmaHistogramBoolean(
      touchpad_metrics_prefix + "AccelerationEnabled.Initial",
      touchpad.settings->acceleration_enabled);
  base::UmaHistogramBoolean(
      touchpad_metrics_prefix + "ReverseScrolling.Initial",
      touchpad.settings->reverse_scrolling);
  base::UmaHistogramEnumeration(touchpad_metrics_prefix + "Sensitivity.Initial",
                                sensitivity);
  base::UmaHistogramBoolean(touchpad_metrics_prefix + "TapDragging.Initial",
                            touchpad.settings->tap_dragging_enabled);
  base::UmaHistogramBoolean(touchpad_metrics_prefix + "TapToClick.Initial",
                            touchpad.settings->tap_to_click_enabled);
  // TODO(yyhyyh@): Add haptic settings metrics.
}

}  // namespace ash