// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DARK_MODE_CONTROLLER_H_
#define ASH_STYLE_DARK_MODE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"

class PrefRegistrySimple;

namespace ash {

class DarkLightModeNudgeController;

// TODO(minch): Rename to DarkLightModeController.
// Controls the behavior of dark/light mode. Turns on the dark mode at sunset
// and off at sunrise if auto schedule is set (custom start and end for
// scheduling is not supported). And determine whether to show the educational
// nudge for users on login.
class ASH_EXPORT DarkModeController : public ScheduledFeature {
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

  // Happens if the user toggled the entry points of dark/light mode to switch
  // color mode. Educational nudge will not be shown any more when this happens.
  void ToggledByUser();

  void SetShowNudgeForTesting(bool value);

 protected:
  // ScheduledFeature:
  void RefreshFeatureState() override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  // ScheduledFeature:
  const char* GetFeatureName() const override;

  std::unique_ptr<DarkLightModeNudgeController> nudge_controller_;
};

}  // namespace ash

#endif  // ASH_STYLE_DARK_MODE_CONTROLLER_H_
