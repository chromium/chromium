// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DARK_MODE_DARK_MODE_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_DARK_MODE_DARK_MODE_FEATURE_POD_CONTROLLER_H_

#include "ash/system/dark_mode/color_mode_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/macros.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of a feature pod button that toggles dark mode for ash.
class DarkModeFeaturePodController : public FeaturePodControllerBase,
                                     public ColorModeObserver {
 public:
  explicit DarkModeFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  DarkModeFeaturePodController(const DarkModeFeaturePodController& other) =
      delete;
  DarkModeFeaturePodController& operator=(
      const DarkModeFeaturePodController& other) = delete;
  ~DarkModeFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

  // ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

 private:
  void UpdateButton(bool dark_mode_enabled);

  UnifiedSystemTrayController* const tray_controller_;

  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_DARK_MODE_DARK_MODE_FEATURE_POD_CONTROLLER_H_
