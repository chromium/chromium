// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_HARDWARE_DATA_USAGE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SETTINGS_HARDWARE_DATA_USAGE_CONTROLLER_H_

#include "chrome/browser/ash/settings/owner_pending_setting_controller.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Class to control setting of cros.reven.enable_hw_data_usage device preference
// before ownership is taken.
class HWDataUsageController : public OwnerPendingSettingController {
 public:
  // Manage singleton instance.
  static void Initialize(PrefService* local_state);
  static bool IsInitialized();
  static void Shutdown();
  static HWDataUsageController* Get();

  HWDataUsageController(const HWDataUsageController&) = delete;
  HWDataUsageController& operator=(const HWDataUsageController&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  explicit HWDataUsageController(PrefService* local_state);
  ~HWDataUsageController() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_HARDWARE_DATA_USAGE_CONTROLLER_H_
