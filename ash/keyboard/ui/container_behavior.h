// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_CONTAINER_BEHAVIOR_H_
#define ASH_KEYBOARD_UI_CONTAINER_BEHAVIOR_H_

#include "ash/keyboard/ui/keyboard_export.h"
#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace aura {
class Window;
}

namespace ui {
class LocatedEvent;
class ScopedLayerAnimationSettings;
}  // namespace ui

namespace wm {
class ScopedHidingAnimationSettings;
}

namespace keyboard {

// Represents and encapsulates how the keyboard container should visually behave
// within the workspace window.
class KEYBOARD_EXPORT ContainerBehavior {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual gfx::Rect GetBoundsInScreen() const = 0;
    virtual bool IsKeyboardLocked() const = 0;
    virtual void MoveKeyboardWindow(const gfx::Rect& new_bounds) = 0;
    virtual void MoveKeyboardWindowToDisplay(const display::Display& display,
                                             const gfx::Rect& new_bounds) = 0;
  };

  explicit ContainerBehavior(Delegate* delegate);
  virtual ~ContainerBehavior();

  // Apply changes to the animation settings to animate the keyboard container
  // showing.
  virtual void DoShowingAnimation(
      aura::Window* container,
      ui::ScopedLayerAnimationSettings* animation_settings) = 0;

  // Apply changes to the animation settings to animate the keyboard container
  // hiding.
  virtual void DoHidingAnimation(
      aura::Window* container,
      wm::ScopedHidingAnimationSettings* animation_settings) = 0;

  // Initialize the starting state of the keyboard container for the showing
  // animation.
  virtual void InitializeShowAnimationStartingState(
      aura::Window* container) = 0;

  // Used by the layout manager to intercept any bounds setting request to
  // adjust the request to different bounds, if necessary. This method gets
  // called at any time during the keyboard's life cycle. The bounds are in
  // global screen coordinates.
  virtual gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds_in_screen_coords) = 0;

  // Used to set the bounds to the default location. This is generally called
  // during initialization, but may also be have identical behavior to
  // AdjustSetBoundsRequest in the case of constant layouts such as the fixed
  // full-width keyboard.
  virtual void SetCanonicalBounds(aura::Window* container,
                                  const gfx::Rect& display_bounds) = 0;

  // A ContainerBehavior can choose to not allow overscroll to be used. It is
  // important to note that the word "Allowed" is used because whether or not
  // overscroll is "enabled" depends on multiple external factors.
  virtual bool IsOverscrollAllowed() const = 0;

  virtual void SavePosition(const gfx::Rect& keyboard_bounds_in_screen,
                            const gfx::Size& screen_size) = 0;

  virtual bool HandlePointerEvent(const ui::LocatedEvent& event,
                                  const display::Display& current_display) = 0;

  virtual ContainerType GetType() const = 0;

  // Removing focus from a text field should cause the keyboard to be dismissed.
  virtual bool TextBlurHidesKeyboard() const = 0;

  // Sets a region of the keyboard window that is occluded by the keyboard.
  // This is called by the IME extension code. By default, this call is ignored.
  // Container behaviors that listen for this call should override this method.
  virtual void SetOccludedBounds(const gfx::Rect& occluded_bounds_in_window) {}

  // Gets the region of the keyboard window that is occluded by the keyboard, or
  // an empty rectangle if nothing is occluded. The occluded region is
  // considered to be 'unusable', so the window manager or other system UI
  // should respond to the occluded bounds (e.g. by moving windows out of the
  // occluded region).
  //
  // The occluded bounds must be completely contained in the visual bounds.
  virtual gfx::Rect GetOccludedBounds(
      const gfx::Rect& visual_bounds_in_window) const = 0;

  // Any region of the screen that is occluded by the keyboard should cause the
  // workspace to change its layout.
  virtual bool OccludedBoundsAffectWorkspaceLayout() const = 0;

  // Sets floating keyboard drggable rect.
  virtual void SetDraggableArea(const gfx::Rect& rect) = 0;

  // Sets the area of the keyboard window that should not move off screen. Any
  // area outside of this can be moved off the user's screen. Note the bounds
  // here are relative to the window's origin.
  virtual void SetAreaToRemainOnScreen(const gfx::Rect& rect) = 0;

 protected:
  Delegate* delegate_;

  // The opacity of virtual keyboard container when show animation
  // starts or hide animation finishes. This cannot be zero because we
  // call Show() on the keyboard window before setting the opacity
  // back to 1.0. Since windows are not allowed to be shown with zero
  // opacity, we always animate to 0.01 instead.
  static constexpr float kAnimationStartOrAfterHideOpacity = 0.01f;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_CONTAINER_BEHAVIOR_H_
