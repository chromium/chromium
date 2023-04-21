// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_OBSERVER_H_
#define ASH_SHELF_SHELF_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"

namespace ash {

enum class AnimationChangeType;

// Used to observe changes to the shelf.
class ASH_EXPORT ShelfObserver {
 public:
  // Called when the shelf shutdown starts.
  virtual void OnShelfShuttingDown() {}

  // Invoked when background type is changed.
  virtual void OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                                       AnimationChangeType change_type) {}

  // Invoked when Shelf's visibility state changes to |new_state|.
  virtual void OnShelfVisibilityStateChanged(ShelfVisibilityState new_state) {}

  // Invoked when Shelf's auto hide state is changed to |new_state|.
  virtual void OnAutoHideStateChanged(ShelfAutoHideState new_state) {}

  // Invoked when the shelf auto-hide behavior is changed.
  virtual void OnShelfAutoHideBehaviorChanged() {}

  // Invoked when the positions of Shelf Icons are changed.
  virtual void OnShelfIconPositionsChanged() {}

  // Invoked when the hotseat state is changed.
  virtual void OnHotseatStateChanged(HotseatState old_state,
                                     HotseatState new_state) {}

  // Invoked when the Shelf has updated its insets in work area insets.
  virtual void OnShelfWorkAreaInsetsChanged() {}

 protected:
  virtual ~ShelfObserver() {}
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_OBSERVER_H_
