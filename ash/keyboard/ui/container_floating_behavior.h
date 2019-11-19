// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_CONTAINER_FLOATING_BEHAVIOR_H_
#define ASH_KEYBOARD_UI_CONTAINER_FLOATING_BEHAVIOR_H_

#include <memory>

#include "ash/keyboard/ui/container_behavior.h"
#include "ash/keyboard/ui/drag_descriptor.h"
#include "ash/keyboard/ui/keyboard_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace keyboard {

// Margins from the bottom right corner of the screen for the default location
// of the keyboard.
constexpr int kDefaultDistanceFromScreenBottom = 20;
constexpr int kDefaultDistanceFromScreenRight = 20;

class KEYBOARD_EXPORT ContainerFloatingBehavior : public ContainerBehavior {
 public:
  explicit ContainerFloatingBehavior(Delegate* delegate);
  ~ContainerFloatingBehavior() override;

  // ContainerBehavior overrides
  void DoHidingAnimation(
      aura::Window* container,
      ::wm::ScopedHidingAnimationSettings* animation_settings) override;
  void DoShowingAnimation(
      aura::Window* container,
      ui::ScopedLayerAnimationSettings* animation_settings) override;
  void InitializeShowAnimationStartingState(aura::Window* container) override;
  gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds_in_screen_coords) override;
  bool IsOverscrollAllowed() const override;
  void SavePosition(const gfx::Rect& keyboard_bounds_in_screen,
                    const gfx::Size& screen_size) override;
  bool HandlePointerEvent(const ui::LocatedEvent& event,
                          const display::Display& current_display) override;
  void SetCanonicalBounds(aura::Window* container,
                          const gfx::Rect& display_bounds) override;
  ContainerType GetType() const override;
  bool TextBlurHidesKeyboard() const override;
  gfx::Rect GetOccludedBounds(
      const gfx::Rect& visual_bounds_in_screen) const override;
  bool OccludedBoundsAffectWorkspaceLayout() const override;
  void SetDraggableArea(const gfx::Rect& rect) override;
  void SetAreaToRemainOnScreen(const gfx::Rect& bounds) override;

  // Calculate the position of the keyboard for when it is being shown.
  gfx::Point GetPositionForShowingKeyboard(
      const gfx::Size& keyboard_size,
      const gfx::Rect& display_bounds) const;

 private:
  struct KeyboardPosition {
    double left_padding_allotment_ratio;
    double top_padding_allotment_ratio;
  };

  // Ensures that the keyboard is neither off the screen nor overlapping an
  // edge.
  gfx::Rect ContainKeyboardToScreenBounds(
      const gfx::Rect& keyboard_bounds_in_screen,
      const gfx::Rect& display_bounds) const;

  // Saves the current keyboard location for use the next time it is displayed.
  void UpdateLastPoint(const gfx::Point& position);

  // TODO(blakeo): cache the default_position_ on a per-display basis.
  std::unique_ptr<KeyboardPosition> default_position_in_screen_;

  // Current state of a cursor drag to move the keyboard, if one exists.
  // Otherwise nullptr.
  std::unique_ptr<const DragDescriptor> drag_descriptor_;

  gfx::Rect draggable_area_ = gfx::Rect();
  gfx::Rect clip_bounds_ = gfx::Rect();
  gfx::Rect area_to_remain_on_screen_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_CONTAINER_FLOATING_BEHAVIOR_H_
