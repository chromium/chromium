// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MAGNIFIER_FULLSCREEN_MAGNIFIER_CONTROLLER_H_
#define ASH_ACCESSIBILITY_MAGNIFIER_FULLSCREEN_MAGNIFIER_CONTROLLER_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Transform;
}

namespace ui {
class GestureProviderAura;
}  // namespace ui

namespace ash {

// FullscreenMagnifierController implements GestureConsumer as it has its own
// GestureProvider to recognize gestures with screen coordinates of touches.
// Logical coordinates of touches cannot be used as they are changed with
// viewport change: scroll, zoom.
// FullscreenMagnifierController implements EventRewriter to see and rewrite
// touch events. Once the controller detects two fingers pinch or scroll, it
// starts consuming all touch events not to confuse an app or a browser on the
// screen. It needs to rewrite events to dispatch touch cancel events.
class ASH_EXPORT FullscreenMagnifierController
    : public ui::EventHandler,
      public ui::ImplicitAnimationObserver,
      public aura::WindowObserver,
      public ui::GestureConsumer,
      public ui::EventRewriter {
 public:
  enum ScrollDirection {
    SCROLL_NONE,
    SCROLL_LEFT,
    SCROLL_RIGHT,
    SCROLL_UP,
    SCROLL_DOWN
  };

  FullscreenMagnifierController();
  FullscreenMagnifierController(const FullscreenMagnifierController&) = delete;
  FullscreenMagnifierController& operator=(
      const FullscreenMagnifierController&) = delete;
  ~FullscreenMagnifierController() override;

  void set_mouse_following_mode(
      MagnifierMouseFollowingMode mouse_following_mode) {
    mouse_following_mode_ = mouse_following_mode;
  }

  MagnifierMouseFollowingMode mouse_following_mode() const {
    return mouse_following_mode_;
  }

  // Enables (or disables if |enabled| is false) screen magnifier feature.
  void SetEnabled(bool enabled);

  // Returns if the screen magnifier is enabled or not.
  bool IsEnabled() const;

  // Sets the magnification ratio. 1.0f means no magnification.
  void SetScale(float scale, bool animate);

  // Returns the current magnification ratio.
  float GetScale() const { return scale_; }

  // Maps the current scale value to an index in the range between the minimum
  // and maximum scale values, and steps up or down the scale depending on the
  // value of |delta_index|.
  void StepToNextScaleValue(int delta_index);

  // Set the top-left point of the magnification window.
  void MoveWindow(int x, int y, bool animate);
  void MoveWindow(const gfx::Point& point, bool animate);

  // Returns the current top-left point of the magnification window.
  gfx::Point GetWindowPosition() const;

  void SetScrollDirection(ScrollDirection direction);

  // Returns the viewport (i.e. the current visible window)'s Rect in root
  // window coordinates.
  gfx::Rect GetViewportRect() const;

  // Centers the viewport around the given point in screen coordinates.
  void CenterOnPoint(const gfx::Point& point_in_screen);

  // Move |rect_in_screen| within the magnifier viewport. If |rect_in_screen| is
  // already completely within the viewport, do nothing. If any edge of
  // |rect_in_screen| is outside the viewport (e.g. if rect is larger than or
  // extends partially beyond the viewport), center the overflowing dimensions
  // of the viewport on center of |rect_in_screen| (e.g. center viewport
  // vertically if |rect| extends beyond bottom of screen). Called from
  // Accessibility Common extension. Called from Accessibility Common extension.
  void HandleMoveMagnifierToRect(const gfx::Rect& rect_in_screen);

  // Switch the magnified root window to |new_root_window|. This does following:
  //  - Unzoom the current root_window.
  //  - Zoom the given new root_window |new_root_window|.
  //  - Switch the target window from current window to |new_root_window|.
  void SwitchTargetRootWindow(aura::Window* new_root_window,
                              bool redraw_original_root);

  // Returns the magnification transformation for the root window. If
  // magnification is disabled, return an empty Transform.
  gfx::Transform GetMagnifierTransform() const;

  // Returns the last mouse cursor (or last touched) location.
  gfx::Point GetPointOfInterestForTesting() {
    return point_of_interest_in_root_;
  }

  // Returns true if magnifier is still on animation for moving viewport.
  bool IsOnAnimationForTesting() const { return is_on_animation_; }

  // Returns the current number of touch points.
  int32_t GetTouchPointsForTesting() const { return touch_points_; }

  void set_cursor_moved_callback_for_testing(
      base::RepeatingCallback<void(const gfx::Point&)> callback) {
    cursor_moved_callback_for_testing_ = std::move(callback);
  }

 private:
  friend class FullscreenMagnifierControllerTest;
  class GestureProviderClient;

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* root_window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // ui::EventRewriter overrides:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  // ui::GestureConsumer:
  const std::string& GetName() const override;

  // Redraws the magnification window with the given origin position and the
  // given scale. Returns true if the window is changed; otherwise, false.
  // These methods should be called internally just after the scale and/or
  // the position are changed to redraw the window.
  bool Redraw(const gfx::PointF& position_in_pixels, float scale, bool animate);

  // Redraws the magnification window with the given origin position in dip and
  // the given scale. Returns true if the window is changed; otherwise, false.
  // The last two parameters specify the animation duration and tween type.
  // If |animation_in_ms| is zero, there will be no animation, and |tween_type|
  // will be ignored.
  bool RedrawDIP(const gfx::PointF& position_in_dip,
                 float scale,
                 int animation_in_ms,
                 gfx::Tween::Type tween_type);

  // 1) If the screen is scrolling (i.e. animating) and should scroll further,
  // it does nothing.
  // 2) If the screen is scrolling (i.e. animating) and the direction is NONE,
  // it stops the scrolling animation.
  // 3) If the direction is set to value other than NONE, it starts the
  // scrolling/ animation towards that direction.
  void StartOrStopScrollIfNecessary();

  // Redraw with the given zoom scale keeping the mouse cursor location. In
  // other words, zoom (or unzoom) centering around the cursor.
  // Ignore mouse position change after redrawing if |ignore_mouse_change| is
  // true.
  void RedrawKeepingMousePosition(float scale,
                                  bool animate,
                                  bool ignore_mouse_change);

  // Takes mouse root `location` in floating-point DIP. Note at higher zoom
  // levels, the floating point values matter more, because the ratio of px to
  // DIP increases.
  void OnMouseMove(const gfx::PointF& location);

  // Move the mouse cursot to the given point. Actual move will be done when
  // the animation is completed. This should be called after animation is
  // started.
  void AfterAnimationMoveCursorTo(const gfx::Point& location);

  // Returns if the magnification scale is 1.0 or not (larger then 1.0).
  bool IsMagnified() const;

  // Returns the rect of the magnification window.
  gfx::RectF GetWindowRectDIP(float scale) const;

  // Returns the size of the root window.
  gfx::Size GetHostSizeDIP() const;

  // Correct the given scale value if necessary.
  void ValidateScale(float* scale);

  // Process pending gestures in |gesture_provider_|. This method returns true
  // if the controller needs to cancel existing touches.
  bool ProcessGestures();

  // Moves the viewport when |point| is located within
  // |x_margin| and |y_margin| to the edge of the visible
  // window region. The viewport will be moved so that the |point| will be
  // moved to the point where it has |x_margin| and |y_margin|
  // to the edge of the visible region if possible (less if the mouse is closer
  // to the edge of the screen). If |reduce_bottom_margin| is true,
  // then a reduced value will be used as the |y_margin| and
  // |y_target_margin| for the bottom edge.
  void MoveMagnifierWindowFollowPoint(const gfx::Point& point,
                                      int x_margin,
                                      int y_margin,
                                      bool reduce_bottom_margin);

  // Moves the viewport to center |point| in magnifier screen.
  void MoveMagnifierWindowCenterPoint(const gfx::Point& point);

  // Moves the viewport so that |rect| is fully visible. If |rect| is larger
  // than the viewport horizontally or vertically, the viewport will be moved
  // to center the |rect| in that dimension.
  void MoveMagnifierWindowFollowRect(const gfx::Rect& rect);

  // Moves the cursor to the given location in the root window.
  void MoveCursorTo(const gfx::Point& root_location);

  // Target root window. This must not be NULL.
  raw_ptr<aura::Window> root_window_;

  // True if the magnified window is currently animating a change. Otherwise,
  // false.
  bool is_on_animation_ = false;

  bool is_enabled_ = false;

  bool keep_focus_centered_ = false;

  // The current mouse following mode (e.g. continuous, centered, edge).
  MagnifierMouseFollowingMode mouse_following_mode_ =
      MagnifierMouseFollowingMode::kEdge;

  // True if the cursor needs to move the given position after the animation
  // will be finished. When using this, set |position_after_animation_| as well.
  bool move_cursor_after_animation_ = false;

  // Stores the position of cursor to be moved after animation.
  gfx::Point position_after_animation_;

  // Stores the last mouse cursor (or last touched) location. This value is
  // used on zooming to keep this location visible.
  gfx::Point point_of_interest_in_root_;

  // Current scale, origin (left-top) position of the magnification window.
  float scale_;
  gfx::PointF origin_;

  float original_scale_;
  gfx::PointF original_origin_;

  ScrollDirection scroll_direction_ = SCROLL_NONE;

  // If true, FullscreenMagnifierController consumes all touch events.
  bool consume_touch_event_ = false;

  // Number of touch points on the screen.
  int32_t touch_points_ = 0;

  // Map for holding EventType::kTouchPress events. Those events are used to
  // dispatch EventType::kTouchCancelled events. Events will be removed from
  // this map when press events are cancelled, i.e. size of this map can be
  // different from number of touches on the screen. Key is pointer id.
  std::map<int32_t, std::unique_ptr<ui::TouchEvent>> press_event_map_;

  std::unique_ptr<GestureProviderClient> gesture_provider_client_;

  // MagnificationCotroller owns its GestureProvider to detect gestures with
  // screen coordinates of touch events. As FullscreenMagnifierController
  // changes zoom level and moves viewport, logical coordinates of touches
  // cannot be used for gesture detection as they are changed if the controller
  // reacts to gestures.
  std::unique_ptr<ui::GestureProviderAura> gesture_provider_;

  // Most recent caret position in |root_window_| coordinates.
  gfx::Point caret_point_;

  // Flag to draw a preview box around magnifier viewport area instead of
  // magnifying the screen for debugging.
  bool magnifier_debug_draw_rect_ = false;

  // Called every time MoveCursorTo is called, when set in tests.
  base::RepeatingCallback<void(const gfx::Point&)>
      cursor_moved_callback_for_testing_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MAGNIFIER_FULLSCREEN_MAGNIFIER_CONTROLLER_H_
