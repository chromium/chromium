// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DARK_MODE_CONTROLLER_H_
#define ASH_STYLE_DARK_MODE_CONTROLLER_H_

#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "components/prefs/pref_registry_simple.h"

class PrefRegistrySimple;

namespace ash {

// DarkModeController handles automatic scheduling of dark mode to turn it on
// at sunset and off at sunrise. However, it does not support custom start
// and end times for scheduling.
class DarkModeController : public ScheduledFeature {
 public:
  DarkModeController();
  DarkModeController(const DarkModeController&) = delete;
  DarkModeController& operator=(const DarkModeController&) = delete;
  ~DarkModeController() override;

  static DarkModeController* Get();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Enables or disables auto scheduling on dark mode feature. When enabled,
  // the dark mode will automatically turn on during sunset to sunrise and off
  // outside that period.
  void SetAutoScheduleEnabled(bool enabled);

  // True if dark mode is automatically scheduled to turn on at sunset and off
  // at sunrise.
  bool GetAutoScheduleEnabled() const;

 protected:
  // ScheduledFeature:
  void RefreshFeatureState() override;

 private:
  // ScheduledFeature:
  const char* GetFeatureName() const override;
};

}  // namespace ash

#endif  // ASH_STYLE_DARK_MODE_CONTROLLER_H_