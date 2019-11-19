// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_NOTIFICATION_MANAGER_H_
#define ASH_KEYBOARD_UI_NOTIFICATION_MANAGER_H_

#include "ash/keyboard/ui/keyboard_export.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
class KeyboardControllerObserver;
}

namespace keyboard {

template <typename T>
class ValueNotificationConsolidator {
 public:
  ValueNotificationConsolidator() {}

  bool ShouldSendNotification(const T new_value);

 private:
  bool never_sent_ = true;
  T value_;
};

// Logic for consolidating consecutive identical notifications from the
// KeyboardControllerObserver.
class KEYBOARD_EXPORT NotificationManager {
 public:
  NotificationManager();

  // Sends various KeyboardControllerObserver notifications related to bounds
  // changes:
  // - visual bounds change
  // - occluded bounds change
  // - layout displacement bounds change
  // - general visibility change
  void SendNotifications(
      bool does_occluded_bounds_affect_layout,
      const gfx::Rect& visual_bounds,
      const gfx::Rect& occluded_bounds,
      const base::ObserverList<ash::KeyboardControllerObserver>::Unchecked&
          observers);

  bool ShouldSendVisibilityNotification(bool current_visibility);

  bool ShouldSendVisualBoundsNotification(const gfx::Rect& new_bounds);

  bool ShouldSendOccludedBoundsNotification(const gfx::Rect& new_bounds);

  bool ShouldSendWorkspaceDisplacementBoundsNotification(
      const gfx::Rect& new_bounds);

 private:
  // ValueNotificationConsolidator uses == for comparison, but empty rectangles
  // ought to be considered equal regardless of location or non-zero dimensions.
  // This method will return a default empty (0,0,0,0) rectangle for any 0-area
  // rectangle, otherwise it returns the original rectangle, unmodified.
  gfx::Rect CanonicalizeEmptyRectangles(const gfx::Rect& rect) const;

  ValueNotificationConsolidator<bool> visibility_;
  ValueNotificationConsolidator<gfx::Rect> visual_bounds_;
  ValueNotificationConsolidator<gfx::Rect> occluded_bounds_;
  ValueNotificationConsolidator<gfx::Rect> workspace_displaced_bounds_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_NOTIFICATION_MANAGER_H_
