// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_
#define ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_

#include "ash/display/display_configuration_controller.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/display/display.h"

namespace ash {
class ScreenOrientationController;

class ScreenOrientationControllerTestApi {
 public:
  explicit ScreenOrientationControllerTestApi(
      ScreenOrientationController* controller);

  ScreenOrientationControllerTestApi(
      const ScreenOrientationControllerTestApi&) = delete;
  ScreenOrientationControllerTestApi& operator=(
      const ScreenOrientationControllerTestApi&) = delete;

  void SetDisplayRotation(
      display::Display::Rotation rotation,
      display::Display::RotationSource source,
      DisplayConfigurationController::RotationAnimation mode =
          DisplayConfigurationController::ANIMATION_ASYNC);

  void SetRotationLocked(bool rotation_locked);

  chromeos::OrientationType UserLockedOrientation() const;

  chromeos::OrientationType GetCurrentOrientation() const;

  void UpdateNaturalOrientation();

  bool IsAutoRotationAllowed() const;

 private:
  raw_ptr<ScreenOrientationController> controller_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_
