// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/notification_manager.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/rect.h"

namespace keyboard {

template <typename T>
ValueNotificationConsolidator<T>::ValueNotificationConsolidator(
    const T& initial_value)
    : value_(initial_value) {}

template <typename T>
bool ValueNotificationConsolidator<T>::ShouldSendNotification(
    const T& new_value) {
  const bool value_changed = new_value != value_;
  if (value_changed) {
    value_ = new_value;
  }
  return value_changed;
}

NotificationManager::NotificationManager()
    : visibility_(false),
      visual_bounds_(gfx::Rect()),
      occluded_bounds_(gfx::Rect()),
      workspace_displaced_bounds_(gfx::Rect()) {}

void NotificationManager::SendNotifications(
    bool does_occluded_bounds_affect_layout,
    const gfx::Rect& visual_bounds,
    const gfx::Rect& occluded_bounds,
    bool is_temporary,
    const base::ObserverList<ash::KeyboardControllerObserver>::Unchecked&
        observers) {
  bool is_visible = !visual_bounds.IsEmpty();
  bool send_visibility_notification =
      ShouldSendVisibilityNotification(is_visible);

  bool send_visual_bounds_notification =
      ShouldSendVisualBoundsNotification(visual_bounds);

  bool send_occluded_bounds_notification =
      ShouldSendOccludedBoundsNotification(occluded_bounds);

  const gfx::Rect workspace_layout_offset_region =
      does_occluded_bounds_affect_layout ? occluded_bounds : gfx::Rect();
  bool send_displaced_bounds_notification =
      ShouldSendWorkspaceDisplacementBoundsNotification(
          workspace_layout_offset_region);

  ash::KeyboardStateDescriptor state;
  state.is_visible = is_visible;
  state.is_temporary = is_temporary;
  state.visual_bounds = visual_bounds;
  state.occluded_bounds_in_screen = occluded_bounds;
  state.displaced_bounds_in_screen = workspace_layout_offset_region;

  for (auto& observer : observers) {
    if (send_visibility_notification)
      observer.OnKeyboardVisibilityChanged(is_visible);

    if (send_visual_bounds_notification)
      observer.OnKeyboardVisibleBoundsChanged(visual_bounds);

    if (send_occluded_bounds_notification)
      observer.OnKeyboardOccludedBoundsChanged(occluded_bounds);

    if (send_displaced_bounds_notification) {
      observer.OnKeyboardDisplacingBoundsChanged(
          workspace_layout_offset_region);
    }

    observer.OnKeyboardAppearanceChanged(state);
  }
}

bool NotificationManager::ShouldSendVisibilityNotification(
    bool current_visibility) {
  return visibility_.ShouldSendNotification(current_visibility);
}

bool NotificationManager::ShouldSendVisualBoundsNotification(
    const gfx::Rect& new_bounds) {
  return visual_bounds_.ShouldSendNotification(
      CanonicalizeEmptyRectangles(new_bounds));
}

bool NotificationManager::ShouldSendOccludedBoundsNotification(
    const gfx::Rect& new_bounds) {
  return occluded_bounds_.ShouldSendNotification(
      CanonicalizeEmptyRectangles(new_bounds));
}

bool NotificationManager::ShouldSendWorkspaceDisplacementBoundsNotification(
    const gfx::Rect& new_bounds) {
  return workspace_displaced_bounds_.ShouldSendNotification(
      CanonicalizeEmptyRectangles(new_bounds));
}

gfx::Rect NotificationManager::CanonicalizeEmptyRectangles(
    const gfx::Rect& rect) const {
  if (rect.IsEmpty()) {
    return gfx::Rect();
  }
  return rect;
}

}  // namespace keyboard
