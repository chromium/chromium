// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_CONTAINER_FULL_WIDTH_BEHAVIOR_H_
#define ASH_KEYBOARD_UI_CONTAINER_FULL_WIDTH_BEHAVIOR_H_

#include "ash/keyboard/ui/container_behavior.h"
#include "ui/gfx/geometry/rect.h"

namespace keyboard {

// Relative distance from the parent window, from which show animation starts
// or hide animation finishes.
constexpr int kFullWidthKeyboardAnimationDistance = 30;

class KEYBOARD_EXPORT ContainerFullWidthBehavior : public ContainerBehavior {
 public:
  explicit ContainerFullWidthBehavior(Delegate* delegate);
  ~ContainerFullWidthBehavior() override;

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
  void SavePosition(const gfx::Rect& keyboard_bounds,
                    const gfx::Size& screen_size) override;
  bool HandlePointerEvent(const ui::LocatedEvent& event,
                          const display::Display& current_display) override;
  bool HandleGestureEvent(const ui::GestureEvent& event,
                          const gfx::Rect& bounds_in_screen) override;
  void SetCanonicalBounds(aura::Window* container,
                          const gfx::Rect& display_bounds) override;
  ContainerType GetType() const override;
  bool TextBlurHidesKeyboard() const override;
  void SetOccludedBounds(const gfx::Rect& occluded_bounds_in_window) override;
  gfx::Rect GetOccludedBounds(
      const gfx::Rect& visual_bounds_in_window) const override;
  bool OccludedBoundsAffectWorkspaceLayout() const override;
  void SetDraggableArea(const gfx::Rect& rect) override;
  void SetAreaToRemainOnScreen(const gfx::Rect& bounds) override;

 private:
  gfx::Rect occluded_bounds_in_window_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_CONTAINER_FULL_WIDTH_BEHAVIOR_H_
