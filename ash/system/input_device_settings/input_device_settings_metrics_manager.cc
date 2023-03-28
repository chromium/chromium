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
      base::Contains(iter->second, keyboard.id)) {
    return;
  }
  recorded_keyboards_[account_id].insert(keyboard.id);

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

}  // namespace ash