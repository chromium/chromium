// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_COMPONENT_H_
#define ASH_SHELF_SHELF_COMPONENT_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// An interface describing any shelf component such as the navigation widget,
// the hotseat widget or the status area widget, to make it easier to
// coordinate animations for all of them.
class ASH_EXPORT ShelfComponent {
  // Makes the component calculate its new target bounds given the current
  // target conditions. It is the component's responsibility to store the
  // calculated bounds.
  virtual void CalculateTargetBounds() = 0;

  // Returns this component's current target bounds, in screen coordinates.
  virtual gfx::Rect GetTargetBounds() const = 0;

  // Updates the component's layout and bounds to match the most recently
  // calculated target bounds. The change should be animated if |animate| is
  // true.
  virtual void UpdateLayout(bool animate) = 0;

  // Updates this component's target bounds according to a gesture that
  // is currently being performed. The |shelf_position| parameter is the
  // new position of the shelf, its x position if it's vertical or its y
  // position if it's horizontal.
  virtual void UpdateTargetBoundsForGesture(int shelf_position) = 0;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_COMPONENT_H_
