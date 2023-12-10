// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_CONTAINER_FLOATING_BEHAVIOR_H_
#define ASH_KEYBOARD_UI_CONTAINER_FLOATING_BEHAVIOR_H_

#include <memory>
#include <optional>

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
  bool HandleGestureEvent(const ui::GestureEvent& event,
                          const gfx::Rect& bounds_in_screen) override;

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

  // This returns a new bounds that is guaranteed to be contained within the
  // bounds of the user's display. If the given bounds is contained within
  // the display's bounds then they remain unchanged. If the given bounds are
  // overlapping an edge of the display, or are completely off the display,
  // then the closest valid bounds within the display will be returned.
  gfx::Rect GetBoundsWithinDisplay(const gfx::Rect& bounds,
                                   const gfx::Rect& display_bounds) const;

  // Convert bounds that are relative to the origin of the keyboard window, to
  // bounds that are relative to the origin of the display.
  gfx::Rect ConvertAreaInKeyboardToScreenBounds(
      const gfx::Rect& area_in_keyboard_window,
      const gfx::Rect& keyboard_window_bounds_in_display) const;

  // This returns a new bounds for the keyboard window that is guaranteed to
  // keep the keyboard window on the display. The area of the keyboard window
  // that is guaranteed to remain on the display is either;
  //
  //  1) the area defined by area_in_window_to_remain_on_screen_, or
  //  2) the entire keyboard window bounds.
  //
  // If area_in_window_to_remain_on_screen_ is not defined, then the entire
  // keyboard window bounds are kept on the display. Otherwise the area
  // defined by area_in_window_to_remain_on_screen_ is kept on the display.
  gfx::Rect ContainKeyboardToDisplayBounds(
      const gfx::Rect& keyboard_window_bounds_in_screen,
      const gfx::Rect& display_bounds) const;

  // Saves the current keyboard location for use the next time it is displayed.
  void UpdateLastPoint(const gfx::Point& position);

  // TODO(blakeo): cache the default_position_ on a per-display basis.
  std::unique_ptr<KeyboardPosition> default_position_in_screen_;

  // Current state of a cursor drag to move the keyboard, if one exists.
  // Otherwise nullptr.
  std::unique_ptr<const DragDescriptor> drag_descriptor_;

  gfx::Rect draggable_area_ = gfx::Rect();

  // The area within the keyboard window that must remain on screen during a
  // drag operation. Note that this is relative to the current keyboard window
  // not the screen.
  std::optional<gfx::Rect> area_in_window_to_remain_on_screen_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_CONTAINER_FLOATING_BEHAVIOR_H_
