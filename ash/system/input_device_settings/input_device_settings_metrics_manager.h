// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/account_id/account_id.h"

namespace ash {

class ASH_EXPORT InputDeviceSettingsMetricsManager {
 public:
  InputDeviceSettingsMetricsManager();
  InputDeviceSettingsMetricsManager(const InputDeviceSettingsMetricsManager&) =
      delete;
  InputDeviceSettingsMetricsManager& operator=(
      const InputDeviceSettingsMetricsManager&) = delete;
  ~InputDeviceSettingsMetricsManager();

  void RecordKeyboardInitialMetrics(const mojom::Keyboard& keyboard);

 private:
  base::flat_map<AccountId, base::flat_set<uint32_t>> recorded_keyboards_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_
