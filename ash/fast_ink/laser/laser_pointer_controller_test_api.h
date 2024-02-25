// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_LASER_LASER_POINTER_CONTROLLER_TEST_API_H_
#define ASH_FAST_INK_LASER_LASER_POINTER_CONTROLLER_TEST_API_H_

#include "ash/system/palette/palette_tray.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/display.h"

namespace ash {
class FastInkPoints;
class LaserPointerController;

// An api for testing the LaserPointerController class.
class LaserPointerControllerTestApi {
 public:
  explicit LaserPointerControllerTestApi(LaserPointerController* instance);

  LaserPointerControllerTestApi(const LaserPointerControllerTestApi&) = delete;
  LaserPointerControllerTestApi& operator=(
      const LaserPointerControllerTestApi&) = delete;

  ~LaserPointerControllerTestApi();

  void SetEnabled(bool enabled);
  bool IsEnabled() const;
  bool IsShowingLaserPointer() const;
  bool IsFadingAway() const;
  PaletteTray* GetPaletteTrayOnDisplay(int64_t display_id) const;
  const FastInkPoints& laser_points() const;
  const FastInkPoints& predicted_laser_points() const;

 private:
  raw_ptr<LaserPointerController> instance_;
};

}  // namespace ash

#endif  // ASH_FAST_INK_LASER_LASER_POINTER_CONTROLLER_TEST_API_H_
