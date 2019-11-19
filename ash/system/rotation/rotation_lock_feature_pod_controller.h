// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ROTATION_ROTATION_LOCK_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_ROTATION_ROTATION_LOCK_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/macros.h"

namespace ash {

// Controller of a feature pod button that toggles rotation lock mode.
class ASH_EXPORT RotationLockFeaturePodController
    : public FeaturePodControllerBase,
      public TabletModeObserver,
      public ScreenOrientationController::Observer {
 public:
  RotationLockFeaturePodController();
  ~RotationLockFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

  // TabletModeObserver:
  void OnTabletPhysicalStateChanged() override;

  // ScreenOrientationController::Observer:
  void OnUserRotationLockChanged() override;

 private:
  void UpdateButton();

  FeaturePodButton* button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RotationLockFeaturePodController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ROTATION_ROTATION_LOCK_FEATURE_POD_CONTROLLER_H_
