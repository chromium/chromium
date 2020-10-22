// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_LAUNCHER_GESTURE_HANDLER_H_
#define ASH_HOME_SCREEN_HOME_LAUNCHER_GESTURE_HANDLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

class SwipeHomeToOverviewController;

// HomeLauncherGestureHandler handles swipe gesture from home screen to overview
// in the home launcher UI.
// TODO(https://crbug.com/1137452): This is now just a wrapper around
// SwipeHomeToOverviewController - the usages should be transitioned to use
// SwipeHomeToOverviewController directly.
class ASH_EXPORT HomeLauncherGestureHandler : public TabletModeObserver {
 public:
  // Enum which tracks which mode the current scroll process is in.
  enum class Mode {
    // There is no current scroll process.
    kNone,
    // Sliding up from the shelf in home launcher screen to the overview screen.
    kSwipeHomeToOverview,
  };

  HomeLauncherGestureHandler();
  ~HomeLauncherGestureHandler() override;

  // Called by owner of this object when a gesture event is received. |location|
  // should be in screen coordinates. Returns false if the the gesture event
  // was not processed.
  bool OnPressEvent(Mode mode, const gfx::PointF& location);
  bool OnScrollEvent(const gfx::PointF& location,
                     float scroll_x,
                     float scroll_y);
  bool OnReleaseEvent(const gfx::PointF& location,
                      base::Optional<float> velocity_y);

  // Cancels a current drag.
  void Cancel();

  bool IsDragInProgress() const;

  // TabletModeObserver:
  void OnTabletModeEnded() override;

  Mode mode() const { return mode_; }

  SwipeHomeToOverviewController*
  swipe_home_to_overview_controller_for_testing() {
    return swipe_home_to_overview_controller_.get();
  }

 private:
  // Resets the handler state.
  void Reset();

  // Returns true if there's no gesture dragging and animation.
  bool IsIdle();

  Mode mode_ = Mode::kNone;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  // The display where the windows are being processed.
  display::Display display_;

  // The gesture controller that switches from home screen to overview when it
  // detects a swipe from the shelf area.
  std::unique_ptr<SwipeHomeToOverviewController>
      swipe_home_to_overview_controller_;

  DISALLOW_COPY_AND_ASSIGN(HomeLauncherGestureHandler);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_LAUNCHER_GESTURE_HANDLER_H_
