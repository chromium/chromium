// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
#define ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/events/event_handler.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/wm/public/window_move_client.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class KeyEvent;
class LocatedEvent;
class MouseEvent;
class GestureEvent;
}  // namespace ui

namespace ash {
namespace mojom {
enum class WindowStateType;
}

// ToplevelWindowEventHandler handles dragging and resizing of top level
// windows.
class ASH_EXPORT ToplevelWindowEventHandler
    : public display::DisplayManagerObserver,
      public aura::WindowObserver,
      public display::DisplayObserver,
      public ui::EventHandler,
      public wm::WindowMoveClient {
 public:
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

  ToplevelWindowEventHandler(const ToplevelWindowEventHandler&) = delete;
  ToplevelWindowEventHandler& operator=(const ToplevelWindowEventHandler&) =
      delete;

  ~ToplevelWindowEventHandler() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // wm::WindowMoveClient:
  wm::WindowMoveResult RunMoveLoop(aura::Window* source,
                                   const gfx::Vector2d& drag_offset,
                                   ::wm::WindowMoveSource move_source) override;
  void EndMoveLoop() override;

  // Attempts to start a drag if one is not already in progress. Returns true if
  // successful. |end_closure| is run when the drag completes, including if the
  // drag is not started. If |update_gesture_target| is true, the gesture
  // target is forcefully updated and gesture events are transferred to
  // new target if any. If |grab_capture| is true, capture is set to |window|,
  // if it is not set yet. In general, prefer the first version.
  bool AttemptToStartDrag(aura::Window* window,
                          const gfx::PointF& point_in_parent,
                          int window_component,
                          ToplevelWindowEventHandler::EndClosure end_closure);
  bool AttemptToStartDrag(aura::Window* window,
                          const gfx::PointF& point_in_parent,
                          int window_component,
                          ::wm::WindowMoveSource source,
                          EndClosure end_closure,
                          bool update_gesture_target,
                          bool grab_capture = true);

  // Attempts to start a pinch if one is not already in progress. Returns true
  // if successful.
  bool AttemptToStartPinch(aura::Window* window,
                           const gfx::PointF& point_in_parent,
                           int window_component,
                           bool update_gesture_target);

  // If there is a drag in progress it is reverted, otherwise does nothing.
  void RevertDrag();

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
  const gfx::PointF& event_location_in_gesture_target() {
    return event_location_in_gesture_target_;
  }
  bool in_gesture_drag() { return in_gesture_drag_; }

  // Returns true if there is a drag in progress.
  bool is_drag_in_progress() const { return window_resizer_.get() != nullptr; }

  bool in_pinch() const { return in_pinch_; }

  void CompleteDragForTesting(DragResult result);

  void ResetWindowResizerForTesting();

 private:
  class ScopedWindowResizer;

  // Called from AttemptToStartDrag() to create the WindowResizer. This returns
  // true on success, false if there is something preventing the resize from
  // starting.
  bool PrepareForDrag(aura::Window* window,
                      const gfx::PointF& point_in_parent,
                      int window_component,
                      ::wm::WindowMoveSource source,
                      bool grab_capture);

  // Called from `AttemptToStartPinch()` to create the WindowResizer. This also
  // returns true on success and false if resize cannot be initiated.
  bool PrepareForPinch(aura::Window* window,
                       const gfx::PointF& point_in_parent,
                       int window_component);

  // Completes or reverts the drag if one is in progress. Returns true if a
  // drag was completed or reverted.
  bool CompleteDrag(DragResult result);

  // Completes pinch but not drag. `CompleteDrag()` should be called even
  // after `CompletePinch()` is called to handle revert, snap, etc.
  bool CompletePinch();

  void HandleMousePressed(aura::Window* target, ui::MouseEvent* event);
  void HandleMouseReleased(aura::Window* target, ui::MouseEvent* event);

  // Called during a drag to resize/position the window.
  void HandleDrag(aura::Window* target, ui::LocatedEvent* event);

  // Called during a pinch to resize/position the window.
  void HandlePinch(aura::Window* target, ui::GestureEvent* event);

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

  // display::DisplayManagerObserver:
  void OnWillApplyDisplayChanges() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Update the gesture target and event location.
  void UpdateGestureTarget(aura::Window* window,
                           const gfx::PointF& location = gfx::PointF());

  // The hittest result for the first finger at the time that it initially
  // touched the screen. |first_finger_hittest_| is one of ui/base/hit_test.h
  int first_finger_hittest_;

  // The point for the first finger at the time that it initially touched the
  // screen.
  gfx::PointF first_finger_touch_point_;

  // True while a drag from the caption area to move the floated window is in
  // progress. If true, stops propagation to avoid showing the tab strip.
  bool is_moving_floated_window_ = false;

  // Is a window move/resize in progress because of gesture events?
  bool in_gesture_drag_ = false;

  // True if the bounds need to be reinitialized in the next gesture update.
  // This is necessary because during the transition from pinch gesture to
  // drag gesture the EventType::kGestureScrollBegin event is never called, and
  // therefore `window_resizer_` must be initiated with the next
  // EventType::kGestureScrollUpdate event.
  bool requires_reinitialization_ = false;

  raw_ptr<aura::Window> gesture_target_ = nullptr;
  gfx::PointF event_location_in_gesture_target_;

  // True if `this` is receiving pinch events. There is a delay from
  // when `this` first receives a gesture begin event to when the client
  // asks the gesture to be initiated, during which time the gesture type
  // may have changed. To start the appropriate gesture, `this` keeps track
  // of if the current gesture is a drag or a pinch.
  bool in_pinch_ = false;

  std::unique_ptr<ScopedWindowResizer> window_resizer_;

  // We exclude dragged or resized windows from occluding things below them to
  // prevent windows from being marked as occluded temporarily while another
  // window is being, for example, dragged over them. This is particularly
  // necessary for lacros where occlusion may cause the content to be evicted
  // and replaced with a snapshot.
  std::optional<aura::WindowOcclusionTracker::ScopedExclude> scoped_exclude_;

  display::ScopedDisplayObserver display_observer_{this};

  EndClosure end_closure_;

  // Are we running a nested run loop from RunMoveLoop().
  bool in_move_loop_ = false;

  base::WeakPtrFactory<ToplevelWindowEventHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_TOPLEVEL_WINDOW_EVENT_HANDLER_H_
