// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
#define ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/window_move_client.h"

namespace aura {
class Window;
}

namespace ui {
class KeyEvent;
class LocatedEvent;
class MouseEvent;
class GestureEvent;
}  // namespace ui

namespace ash {
class BackGestureAffordance;
namespace mojom {
enum class WindowStateType;
}

// ToplevelWindowEventHandler handles dragging and resizing of top level
// windows.
class ASH_EXPORT ToplevelWindowEventHandler
    : public WindowTreeHostManager::Observer,
      public aura::WindowObserver,
      public display::DisplayObserver,
      public ui::EventHandler,
      public ::wm::WindowMoveClient {
 public:
  // The threshold of the fling velocity while fling from left edge to go
  // previous page.
  static constexpr int kFlingVelocityForGoingBack = 1000;

  // How many dips are reserved for gesture events to start swiping to previous
  // page from the left edge of the screen in tablet mode.
  static constexpr int kStartGoingBackLeftEdgeInset = 16;

  // Describes what triggered ending the drag.
  enum class DragResult {
    // The drag successfully completed.
    SUCCESS,

    REVERT,

    // The underlying window was destroyed while the drag is in process.
    WINDOW_DESTROYED
  };
  using EndClosure = base::OnceCallback<void(DragResult)>;

  ToplevelWindowEventHandler();
  ~ToplevelWindowEventHandler() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Attempts to start a drag if one is not already in progress. Returns true if
  // successful. |end_closure| is run when the drag completes, including if the
  // drag is not started. If |update_gesture_target| is true, the gesture
  // target is forcefully updated and gesture events are transferred to
  // new target if any. In general, prefer the first version.
  bool AttemptToStartDrag(aura::Window* window,
                          const gfx::Point& point_in_parent,
                          int window_component,
                          ToplevelWindowEventHandler::EndClosure end_closure);
  bool AttemptToStartDrag(aura::Window* window,
                          const gfx::Point& point_in_parent,
                          int window_component,
                          ::wm::WindowMoveSource source,
                          EndClosure end_closure,
                          bool update_gesture_target);

  // If there is a drag in progress it is reverted, otherwise does nothing.
  void RevertDrag();

  // Returns true if there is a drag in progress.
  bool is_drag_in_progress() const { return window_resizer_.get() != nullptr; }

  // Returns the toplevel window that should be dragged for a gesture event that
  // occurs in the HTCLIENT area of a window. Returns null if there shouldn't be
  // special casing for this HTCLIENT area gesture. This is used to drag app
  // windows which are fullscreened/maximized in tablet mode from the top of the
  // screen, which don't have a window frame.
  static aura::Window* GetTargetForClientAreaGesture(ui::GestureEvent* event,
                                                     aura::Window* target);

  // Returns the window that is currently handling gesture events and its
  // location.
  aura::Window* gesture_target() { return gesture_target_; }
  const gfx::Point& event_location_in_gesture_target() {
    return event_location_in_gesture_target_;
  }

  // Overridden from wm::WindowMoveClient:
  ::wm::WindowMoveResult RunMoveLoop(
      aura::Window* source,
      const gfx::Vector2d& drag_offset,
      ::wm::WindowMoveSource move_source) override;
  void EndMoveLoop() override;

 private:
  class ScopedWindowResizer;

  // Called from AttemptToStartDrag() to create the WindowResizer. This returns
  // true on success, false if there is something preventing the resize from
  // starting.
  bool PrepareForDrag(aura::Window* window,
                      const gfx::Point& point_in_parent,
                      int window_component,
                      ::wm::WindowMoveSource source);

  // Completes or reverts the drag if one is in progress. Returns true if a
  // drag was completed or reverted.
  bool CompleteDrag(DragResult result);

  void HandleMousePressed(aura::Window* target, ui::MouseEvent* event);
  void HandleMouseReleased(aura::Window* target, ui::MouseEvent* event);

  // Called during a drag to resize/position the window.
  void HandleDrag(aura::Window* target, ui::LocatedEvent* event);

  // Called during mouse moves to update window resize shadows.
  void HandleMouseMoved(aura::Window* target, ui::LocatedEvent* event);

  // Called for mouse exits to hide window resize shadows.
  void HandleMouseExited(aura::Window* target, ui::LocatedEvent* event);

  // Called when mouse capture is lost.
  void HandleCaptureLost(ui::LocatedEvent* event);

  // Handles the gesture fling or swipe event.
  void HandleFlingOrSwipe(ui::GestureEvent* event);

  // Invoked from ScopedWindowResizer if the window is destroyed.
  void ResizerWindowDestroyed();

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanging() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Update the gesture target and event location.
  void UpdateGestureTarget(aura::Window* window,
                           const gfx::Point& location = gfx::Point());

  // True if the event is handled for swiping to previous page.
  bool HandleGoingBackFromLeftEdge(ui::GestureEvent* event);

  // The hittest result for the first finger at the time that it initially
  // touched the screen. |first_finger_hittest_| is one of ui/base/hit_test.h
  int first_finger_hittest_;

  // The point for the first finger at the time that it initially touched the
  // screen.
  gfx::Point first_finger_touch_point_;

  // Is a window move/resize in progress because of gesture events?
  bool in_gesture_drag_ = false;

  aura::Window* gesture_target_ = nullptr;
  gfx::Point event_location_in_gesture_target_;

  std::unique_ptr<ScopedWindowResizer> window_resizer_;

  EndClosure end_closure_;

  // Are we running a nested run loop from RunMoveLoop().
  bool in_move_loop_ = false;

  // True if swiping from left edge to go to previous page is in progress.
  bool going_back_started_ = false;

  // Tracks the x-axis and y-axis drag amount through touch events. Used for
  // back gesture affordance in tablet mode. The gesture movement of back
  // gesture can't be recognized by GestureRecognizer, which leads to wrong
  // gesture locations of back gesture. See crbug.com/1015464 for the details.
  int x_drag_amount_ = 0;
  int y_drag_amount_ = 0;

  // True if back gesture dragging on the negative direction of x-axis.
  bool during_reverse_dragging_ = false;

  // Position of last touch event. Used to calculate |y_drag_amount_|. Note,
  // only touch events from |first_touch_id_| will be recorded.
  gfx::Point last_touch_point_;
  ui::PointerId first_touch_id_ = ui::kPointerIdUnknown;

  // Used to show the affordance while swiping from left edge to go to the
  // previout page.
  std::unique_ptr<BackGestureAffordance> back_gesture_affordance_;

  base::WeakPtrFactory<ToplevelWindowEventHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventHandler);
};

}  // namespace ash

#endif  // ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
