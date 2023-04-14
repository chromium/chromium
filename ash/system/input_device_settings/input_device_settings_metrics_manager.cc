// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace ash {

namespace {

enum class PointerSensitivity {
  kLowest = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
  kHighest = 5,
  kMaxValue = kHighest,
};

constexpr auto kModifierNames =
    base::MakeFixedFlatMap<ui::mojom::ModifierKey, const char*>({
        {ui::mojom::ModifierKey::kMeta, "Meta"},
        {ui::mojom::ModifierKey::kControl, "Control"},
        {ui::mojom::ModifierKey::kAlt, "Alt"},
        {ui::mojom::ModifierKey::kCapsLock, "CapsLock"},
        {ui::mojom::ModifierKey::kEscape, "Escape"},
        {ui::mojom::ModifierKey::kBackspace, "Backspace"},
        {ui::mojom::ModifierKey::kAssistant, "Assistant"},
    });

std::string GetKeyboardMetricsPrefix(const mojom::Keyboard& keyboard) {
  if (!keyboard.is_external) {
    return "ChromeOS.Settings.Device.Keyboard.Internal.";
  } else if (keyboard.meta_key == mojom::MetaKey::kLauncher ||
             keyboard.meta_key == mojom::MetaKey::kSearch) {
    return "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.";
  } else {
    return "ChromeOS.Settings.Device.Keyboard.External.";
  }
}

ui::mojom::ModifierKey GetModifierRemappingTo(
    const mojom::KeyboardSettings& settings,
    ui::mojom::ModifierKey modifier_key) {
  const auto iter = settings.modifier_remappings.find(modifier_key);
  if (iter != settings.modifier_remappings.end()) {
    return iter->second;
  }
  return modifier_key;
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

  const std::string keyboard_metrics_prefix =
      GetKeyboardMetricsPrefix(keyboard);

  base::UmaHistogramBoolean(keyboard_metrics_prefix + "TopRowAreFKeys.Initial",
                            keyboard.settings->top_row_are_fkeys);
  // Only record BlockMetaFKeyRewrites when keyboard is external/external
  // chromeos.
  if (keyboard.is_external) {
    base::UmaHistogramBoolean(
        keyboard_metrics_prefix + "BlockMetaFKeyRewrites.Initial",
        keyboard.settings->suppress_meta_fkey_rewrites);
  }

  // Record metrics for modifier remappings.
  for (const auto modifier_key : keyboard.modifier_keys) {
    auto* modifier_name_iter = kModifierNames.find(modifier_key);
    DCHECK(modifier_name_iter != kModifierNames.end());
    const auto key_remapped_to =
        GetModifierRemappingTo(*keyboard.settings, modifier_key);
    const std::string modifier_remapping_metrics =
        base::StrCat({keyboard_metrics_prefix, "Modifiers.",
                      modifier_name_iter->second, "RemappedTo.Initial"});
    base::UmaHistogramEnumeration(modifier_remapping_metrics, key_remapped_to);
  }
}

void InputDeviceSettingsMetricsManager::RecordKeyboardChangedMetrics(
    const mojom::Keyboard& keyboard,
    const mojom::KeyboardSettings& old_settings) {
  const std::string keyboard_metrics_prefix =
      GetKeyboardMetricsPrefix(keyboard);

  if (keyboard.settings->top_row_are_fkeys != old_settings.top_row_are_fkeys) {
    base::UmaHistogramBoolean(
        keyboard_metrics_prefix + "TopRowAreFKeys.Changed",
        keyboard.settings->top_row_are_fkeys);
  }
  // Only record BlockMetaFKeyRewrites when keyboard is external/external
  // chromeos.
  if (keyboard.is_external && keyboard.settings->suppress_meta_fkey_rewrites !=
                                  old_settings.suppress_meta_fkey_rewrites) {
    base::UmaHistogramBoolean(
        keyboard_metrics_prefix + "BlockMetaFKeyRewrites.Changed",
        keyboard.settings->suppress_meta_fkey_rewrites);
  }

  // Record metrics for modifier remappings.
  for (const auto modifier_key : keyboard.modifier_keys) {
    auto* modifier_name_iter = kModifierNames.find(modifier_key);
    DCHECK(modifier_name_iter != kModifierNames.end());
    const auto key_remapped_to_before =
        GetModifierRemappingTo(old_settings, modifier_key);
    const auto key_remapped_to =
        GetModifierRemappingTo(*keyboard.settings, modifier_key);
    if (key_remapped_to_before != key_remapped_to) {
      const std::string modifier_remapping_metrics =
          base::StrCat({keyboard_metrics_prefix, "Modifiers.",
                        modifier_name_iter->second, "RemappedTo.Changed"});
      base::UmaHistogramEnumeration(modifier_remapping_metrics,
                                    key_remapped_to);
    }
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

void InputDeviceSettingsMetricsManager::RecordMouseChangedMetrics(
    const mojom::Mouse& mouse,
    const mojom::MouseSettings& old_settings) {
  if (mouse.settings->acceleration_enabled !=
      old_settings.acceleration_enabled) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.Mouse.AccelerationEnabled.Changed",
        mouse.settings->acceleration_enabled);
  }
  if (mouse.settings->reverse_scrolling != old_settings.reverse_scrolling) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.Mouse.ReverseScrolling.Changed",
        mouse.settings->reverse_scrolling);
  }
  if (mouse.settings->sensitivity != old_settings.sensitivity) {
    PointerSensitivity sensitivity =
        static_cast<PointerSensitivity>(mouse.settings->sensitivity);
    base::UmaHistogramEnumeration(
        "ChromeOS.Settings.Device.Mouse.Sensitivity.Changed", sensitivity);
  }
  if (mouse.settings->swap_right != old_settings.swap_right) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.Mouse.SwapPrimaryButtons.Changed",
        mouse.settings->swap_right);
  }
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

void InputDeviceSettingsMetricsManager::RecordPointingStickChangedMetrics(
    const mojom::PointingStick& pointing_stick,
    const mojom::PointingStickSettings& old_settings) {
  if (pointing_stick.settings->acceleration_enabled !=
      old_settings.acceleration_enabled) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.PointingStick.AccelerationEnabled.Changed",
        pointing_stick.settings->acceleration_enabled);
  }
  if (pointing_stick.settings->sensitivity != old_settings.sensitivity) {
    PointerSensitivity sensitivity =
        static_cast<PointerSensitivity>(pointing_stick.settings->sensitivity);
    base::UmaHistogramEnumeration(
        "ChromeOS.Settings.Device.PointingStick.Sensitivity.Changed",
        sensitivity);
  }
  if (pointing_stick.settings->swap_right != old_settings.swap_right) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.PointingStick.SwapPrimaryButtons.Changed",
        pointing_stick.settings->swap_right);
  }
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

void InputDeviceSettingsMetricsManager::RecordTouchpadChangedMetrics(
    const mojom::Touchpad& touchpad,
    const mojom::TouchpadSettings& old_settings) {
  const std::string touchpad_metrics_prefix =
      touchpad.is_external ? "ChromeOS.Settings.Device.Touchpad.External."
                           : "ChromeOS.Settings.Device.Touchpad.Internal.";
  if (touchpad.settings->acceleration_enabled !=
      old_settings.acceleration_enabled) {
    base::UmaHistogramBoolean(
        touchpad_metrics_prefix + "AccelerationEnabled.Changed",
        touchpad.settings->acceleration_enabled);
  }
  if (touchpad.settings->reverse_scrolling != old_settings.reverse_scrolling) {
    base::UmaHistogramBoolean(
        touchpad_metrics_prefix + "ReverseScrolling.Changed",
        touchpad.settings->reverse_scrolling);
  }
  if (touchpad.settings->sensitivity != old_settings.sensitivity) {
    PointerSensitivity sensitivity =
        static_cast<PointerSensitivity>(touchpad.settings->sensitivity);
    base::UmaHistogramEnumeration(
        touchpad_metrics_prefix + "Sensitivity.Changed", sensitivity);
  }
  if (touchpad.settings->tap_dragging_enabled !=
      old_settings.tap_dragging_enabled) {
    base::UmaHistogramBoolean(touchpad_metrics_prefix + "TapDragging.Changed",
                              touchpad.settings->tap_dragging_enabled);
  }
  if (touchpad.settings->tap_to_click_enabled !=
      old_settings.tap_to_click_enabled) {
    base::UmaHistogramBoolean(touchpad_metrics_prefix + "TapToClick.Changed",
                              touchpad.settings->tap_to_click_enabled);
  }
  // TODO(yyhyyh@): Add haptic settings metrics.
}

}  // namespace ash