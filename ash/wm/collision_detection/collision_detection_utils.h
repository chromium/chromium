// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COLLISION_DETECTION_COLLISION_DETECTION_UTILS_H_
#define ASH_WM_COLLISION_DETECTION_COLLISION_DETECTION_UTILS_H_

#include "ash/ash_export.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// The inset into the work area for a window's resting position. Visible for
// testing.
constexpr int kCollisionWindowWorkAreaInsetsDp = 8;

// Provides utility functions to compute resting positions for windows which
// wish to avoid other system windows, for example, the PIP and the Automatic
// Clicks bubble menu.
class ASH_EXPORT CollisionDetectionUtils {
 public:
  enum class Gravity {
    kGravityLeft,
    kGravityRight,
    kGravityTop,
    kGravityBottom,
  };

  // Ordered list of windows which do collision detection. Higher numbers take
  // higher priority, i.e. the higher RelativePriority of two windows will not
  // move when the windows are in conflict. For example, the Picture in Picture
  // window will move out of the way for the Automatic Clicks menu, and both
  // will move out of the way for the Automatic Clicks scroll menu, and all
  // will move for default system windows. Windows with the same relative
  // priority will not affect collision with each other. kDefault is used for
  // "everything else", and should not be an input to GetRestingPosition or
  // AvoidObstacles.
  // TODO(crbug.com/955512): Ensure calculations take place from high to low
  // priority to reduce number of collision computations.
  enum class RelativePriority {
    kPictureInPicture = 0,
    kSwitchAccessMenu = 1,
    kAutomaticClicksMenu = 2,
    kAutomaticClicksScrollMenu = 3,
    kDictationBubble = 4,
    kFaceGazeBubble = 5,
    kDefault = 6,
  };

  CollisionDetectionUtils() = delete;

  CollisionDetectionUtils(const CollisionDetectionUtils&) = delete;
  CollisionDetectionUtils& operator=(const CollisionDetectionUtils&) = delete;

  ~CollisionDetectionUtils() = delete;

  // Returns the area that the window can be positioned inside for a given
  // display |display|.
  static gfx::Rect GetMovementArea(const display::Display& display);

  // Returns the result of adjusting |bounds_in_screen| according to gravity
  // inside the movement area of |display| without taking obstacles into
  // account.
  static gfx::Rect AdjustToFitMovementAreaByGravity(
      const display::Display& display,
      const gfx::Rect& bounds_in_screen);

  // Returns the position the window should come to rest at. For example,
  // this will be at a screen edge, not in the middle of the screen.
  // TODO(edcourtney): This should consider drag velocity for fling as well.
  static gfx::Rect GetRestingPosition(const display::Display& display,
                                      const gfx::Rect& bounds_in_screen,
                                      RelativePriority priority);

  // Mark a window as ignored for collision detection.
  static void IgnoreWindowForCollisionDetection(aura::Window* window);

  // Allows the windows which use CollisionDetectionUtils to mark their relative
  // priority when they come in position conflict.
  static void MarkWindowPriorityForCollisionDetection(
      aura::Window* window,
      RelativePriority priority);

  // Moves |bounds| such that it does not intersect with system ui areas, such
  // as the unified system tray or the floating keyboard.
  static gfx::Rect AvoidObstacles(const display::Display& display,
                                  const gfx::Rect& bounds_in_screen,
                                  RelativePriority priority);

  // Returns the result of adjusting |bounds| according to |gravity| inside
  // |region|.
  static gfx::Rect GetAdjustedBoundsByGravity(const gfx::Rect& bounds,
                                              const gfx::Rect& region,
                                              Gravity gravity);

  // Returns the gravity that would make |bounds| fall to the closest edge of
  // |region|. If |bounds| is outside of |region| then it will return the
  // gravity as if |bounds| had fallen outside of |region|. See the below
  // diagram for what the gravity regions look like for a point.
  //  \  TOP /
  //   \____/ R
  // L |\  /| I
  // E | \/ | G
  // F | /\ | H
  // T |/__\| T
  //   /    \
  //  /BOTTOM
  static Gravity GetGravityToClosestEdge(const gfx::Rect& bounds,
                                         const gfx::Rect& region);

 private:
  friend class CollisionDetectionUtilsDisplayTest;
  friend class CollisionDetectionUtilsLogicTest;

  // Internal method for collision resolution. Returns a gfx::Rect with the
  // same size as |bounds|. That rectangle will not intersect any of the
  // rectangles in |rects| and will be completely inside |work_area|. If such a
  // rectangle does not exist, returns |bounds|. Otherwise, it will be the
  // closest such rectangle to |bounds|.
  static gfx::Rect AvoidObstaclesInternal(const gfx::Rect& work_area,
                                          const std::vector<gfx::Rect>& rects,
                                          const gfx::Rect& bounds_in_screen,
                                          RelativePriority priority);
};

}  // namespace ash

#endif  // ASH_WM_COLLISION_DETECTION_COLLISION_DETECTION_UTILS_H_
