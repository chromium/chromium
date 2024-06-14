// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_ASH_TOUCH_TRANSFORM_CONTROLLER_H_
#define ASH_TOUCH_ASH_TOUCH_TRANSFORM_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/manager/touch_transform_controller.h"

namespace display {
class DisplayManager;
}

namespace ash {

// AshTouchTransformController listens for display configuration changes and
// updates the touch transforms when one occurs.
class ASH_EXPORT AshTouchTransformController
    : public display::TouchTransformController,
      public display::DisplayManagerObserver {
 public:
  AshTouchTransformController(
      display::DisplayManager* display_manager,
      std::unique_ptr<display::TouchTransformSetter> setter);

  AshTouchTransformController(const AshTouchTransformController&) = delete;
  AshTouchTransformController& operator=(const AshTouchTransformController&) =
      delete;

  ~AshTouchTransformController() override;

  // WindowTreeHostManager::Observer:
  void OnDisplaysInitialized() override;
  void OnDidApplyDisplayChanges() override;
};

}  // namespace ash

#endif  // ASH_TOUCH_ASH_TOUCH_TRANSFORM_CONTROLLER_H_
