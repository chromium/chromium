// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/multi_display/multi_display_metrics_controller.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// How many pixels are reserved for gesture events to start dragging the app
// window from the top of the screen in tablet mode.
constexpr int kDragStartTopEdgeInset = 8;

// Returns whether |window| can be moved via a two finger drag given
// the hittest results of the two fingers.
bool CanStartTwoFingerMove(aura::Window* window,
                           int window_component1,
                           int window_component2) {
  // We allow moving a window via two fingers when the hittest components are
  // HTCLIENT. This is done so that a window can be dragged via two fingers when
  // the tab strip is full and hitting the caption area is difficult. We check
  // the window type and the state type so that we do not steal touches from the
  // web contents.
  if (window->GetType() != aura::client::WINDOW_TYPE_NORMAL ||
      !WindowState::Get(window) ||
      !WindowState::Get(window)->IsNormalOrSnapped()) {
    return false;
  }
  int component1_behavior =
      WindowResizer::GetBoundsChangeForWindowComponent(window_component1);
  int component2_behavior =
      WindowResizer::GetBoundsChangeForWindowComponent(window_component2);
  return (component1_behavior & WindowResizer::kBoundsChange_Resizes) == 0 &&
         (component2_behavior & WindowResizer::kBoundsChange_Resizes) == 0;
}

// Returns whether |window| can be moved or resized via one finger given
// |window_component|.
bool CanStartOneFingerDrag(int window_component) {
  return WindowResizer::GetBoundsChangeForWindowComponent(window_component) !=
         0;
}

void ShowResizeShadow(aura::Window* window, int component) {
  // Don't show resize shadow if
  // 1) the window is not toplevel.
  // 2) the device is in tablet mode.
  // 3) the window is not resizable.
  if (display::Screen::GetScreen()->InTabletMode() ||
      window != window->GetToplevelWindow() ||
      ((window->GetProperty(aura::client::kResizeBehaviorKey) &
        aura::client::kResizeBehaviorCanResize) == 0)) {
    return;
  }

  ResizeShadowController* resize_shadow_controller =
      Shell::Get()->resize_shadow_controller();
  if (resize_shadow_controller)
    resize_shadow_controller->ShowShadow(window, component);
}

void HideResizeShadow(aura::Window* window) {
  ResizeShadowController* resize_shadow_controller =
      Shell::Get()->resize_shadow_controller();
  if (resize_shadow_controller &&
      window->GetProperty(kResizeShadowTypeKey) == ResizeShadowType::kUnlock) {
    resize_shadow_controller->HideShadow(window);
  }
}

// Called once the drag completes.
void OnDragCompleted(
    ToplevelWindowEventHandler::DragResult* result_return_value,
    base::RunLoop* run_loop,
    ToplevelWindowEventHandler::DragResult result) {
  *result_return_value = result;
  run_loop->Quit();
}

// Convert event location into location in parent of target window.
gfx::PointF ConvertToLocationInParent(const aura::Window* target,
                                      const gfx::PointF& location) {
  gfx::PointF location_in_parent = location;
  aura::Window::ConvertPointToTarget(target, target->parent(),
                                     &location_in_parent);
  return location_in_parent;
}

}  // namespace

// -----------------------------------------------------------------------------
// ToplevelWindowEventHandler::ScopedWindowResizer:

// Wraps a WindowResizer and installs an observer on its target window.  When
// the window is destroyed ResizerWindowDestroyed() is invoked back on the
// ToplevelWindowEventHandler to clean up.
class ToplevelWindowEventHandler::ScopedWindowResizer
    : public aura::WindowObserver,
      public WindowStateObserver {
 public:
  ScopedWindowResizer(ToplevelWindowEventHandler* handler,
                      std::unique_ptr<WindowResizer> resizer,
                      bool grab_capture);

  ScopedWindowResizer(const ScopedWindowResizer&) = delete;
  ScopedWindowResizer& operator=(const ScopedWindowResizer&) = delete;

  ~ScopedWindowResizer() override;

  // Returns true if the drag moves the window and does not resize.
  bool IsMove() const;

  // Returns true if the window may be resized.
  bool IsResize() const;

  WindowResizer* resizer() { return resizer_.get(); }

  // WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  // WindowStateObserver overrides:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType type) override;

 private:
  raw_ptr<ToplevelWindowEventHandler> handler_;
  std::unique_ptr<WindowResizer> resizer_;

  // Whether ScopedWindowResizer grabbed capture.
  bool grabbed_capture_;

  // Set to true if OnWindowDestroying() is received.
  bool window_destroying_ = false;
};

ToplevelWindowEventHandler::ScopedWindowResizer::ScopedWindowResizer(
    ToplevelWindowEventHandler* handler,
    std::unique_ptr<WindowResizer> resizer,
    bool grab_capture)
    : handler_(handler), resizer_(std::move(resizer)), grabbed_capture_(false) {
  aura::Window* target = resizer_->GetTarget();
  target->AddObserver(this);
  WindowState::Get(target)->AddObserver(this);

  if (IsResize())
    target->NotifyResizeLoopStarted();

  if (grab_capture && !target->HasCapture()) {
    grabbed_capture_ = true;
    target->SetCapture();
  }
}

ToplevelWindowEventHandler::ScopedWindowResizer::~ScopedWindowResizer() {
  aura::Window* target = resizer_->GetTarget();
  target->RemoveObserver(this);
  WindowState::Get(target)->RemoveObserver(this);
  if (grabbed_capture_)
    target->ReleaseCapture();
  if (!window_destroying_ && IsResize())
    target->NotifyResizeLoopEnded();
}

bool ToplevelWindowEventHandler::ScopedWindowResizer::IsMove() const {
  return resizer_->details().bounds_change ==
         WindowResizer::kBoundsChange_Repositions;
}

bool ToplevelWindowEventHandler::ScopedWindowResizer::IsResize() const {
  return (resizer_->details().bounds_change &
          WindowResizer::kBoundsChange_Resizes) != 0;
}

void ToplevelWindowEventHandler::ScopedWindowResizer::
    OnPreWindowStateTypeChange(WindowState* window_state,
                               chromeos::WindowStateType old) {
  handler_->CompleteDrag(DragResult::SUCCESS);
}

void ToplevelWindowEventHandler::ScopedWindowResizer::OnWindowDestroying(
    aura::Window* window) {
  DCHECK_EQ(resizer_->GetTarget(), window);
  window_destroying_ = true;
  handler_->ResizerWindowDestroyed();
}

// -----------------------------------------------------------------------------
// ToplevelWindowEventHandler:

ToplevelWindowEventHandler::ToplevelWindowEventHandler()
    : first_finger_hittest_(HTNOWHERE) {
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
}

ToplevelWindowEventHandler::~ToplevelWindowEventHandler() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
  // It's possible that `ToplevelWindowEventHandler` was not removed as the
  // window observer of its observed window `gesture_target_` yet, so remove it
  // here to avoid hitting the CHECK error in WindowObserver's destructor.
  // Please see https://crbug.com/1378259 for more details.
  UpdateGestureTarget(nullptr);
}

void ToplevelWindowEventHandler::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!window_resizer_ || !(metrics & DISPLAY_METRIC_ROTATION))
    return;

  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          window_resizer_->resizer()->GetTarget());
  if (display.id() != current_display.id())
    return;

  RevertDrag();
}

void ToplevelWindowEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (window_resizer_.get() && event->type() == ui::EventType::kKeyPressed &&
      event->key_code() == ui::VKEY_ESCAPE) {
    CompleteDrag(DragResult::REVERT);
  }
}

void ToplevelWindowEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  UpdateGestureTarget(nullptr);

  if (event->handled())
    return;
  if ((event->flags() &
       (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return;

  if (event->type() == ui::EventType::kMouseCaptureChanged) {
    // Capture is grabbed when both gesture and mouse drags start. Handle
    // capture loss regardless of which type of drag is in progress.
    HandleCaptureLost(event);
    return;
  }

  if (in_gesture_drag_)
    return;
  aura::Window* target = static_cast<aura::Window*>(event->target());
  switch (event->type()) {
    case ui::EventType::kMousePressed:
      HandleMousePressed(target, event);
      break;
    case ui::EventType::kMouseDragged:
      HandleDrag(target, event);
      break;
    case ui::EventType::kMouseReleased:
      HandleMouseReleased(target, event);
      break;
    case ui::EventType::kMouseMoved:
      HandleMouseMoved(target, event);
      break;
    case ui::EventType::kMouseExited:
      HandleMouseExited(target, event);
      break;
    default:
      break;
  }
}

void ToplevelWindowEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (event->phase() != ui::EP_PRETARGET)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());

  int component = window_util::GetNonClientComponent(target, event->location());
  gfx::PointF event_location = event->location_f();

  aura::Window* original_target = target;
  bool client_area_drag = false;
  if (component == HTCLIENT) {
    // When dragging on a client area starts a gesture drag, |this| stops the
    // propagation of the EventType::kGestureScrollBegin event. Subsequent
    // gestures on the HTCLIENT area should also be stopped lest the client
    // receive an EventType::kGestureScrollUpdate without the
    // EventType::kGestureScrollBegin.
    if (in_gesture_drag_ && target != gesture_target_) {
      event->StopPropagation();
      return;
    }

    aura::Window* new_target = GetTargetForClientAreaGesture(event, target);

    client_area_drag = !!new_target;
    if (new_target && (target != new_target)) {
      DCHECK_EQ(ui::EventType::kGestureScrollBegin, event->type());
      aura::Window::ConvertPointToTarget(target, new_target, &event_location);

      aura::Env::GetInstance()->gesture_recognizer()->TransferEventsTo(
          original_target, new_target, ui::TransferTouchesBehavior::kCancel);
      UpdateGestureTarget(new_target, event_location);
      target = new_target;
    }
  }

  if (event->type() == ui::EventType::kGesturePinchBegin) {
    in_pinch_ = true;
  } else if (event->type() == ui::EventType::kGestureScrollBegin ||
             event->type() == ui::EventType::kGesturePinchEnd) {
    in_pinch_ = false;
  }

  if (event->handled()) {
    return;
  }
  if (!target->delegate()) {
    return;
  }

  if (window_resizer_ && !in_gesture_drag_) {
    return;
  }

  if (window_resizer_ && window_resizer_->resizer()->GetTarget() != target &&
      !target->bounds().IsEmpty()) {
    CompleteDrag(DragResult::SUCCESS);
    return;
  }

  if (event->type() == ui::EventType::kGestureEnd &&
      event->details().touch_points() == 1) {
    UpdateGestureTarget(nullptr);
  } else if (event->type() == ui::EventType::kGestureBegin) {
    // We don't always process EventType::kGestureEnd events (i.e. on a fling or
    // swipe), so reset `is_moving_floated_window_` in EventType::kGestureBegin.
    is_moving_floated_window_ = false;
  }

  if (event->type() == ui::EventType::kGestureEnd &&
      event->details().touch_points() == 1) {
    UpdateGestureTarget(nullptr);
  }

  if (!gesture_target_) {
    if (event->type() == ui::EventType::kGestureBegin) {
      // If `gesture_target_` does not exist then this is the start of a
      // completely new gesture. We sometimes cannot wait for
      // event-type-specific `BEGIN` event to set `gesture_target_`
      // because client may call `AttemptToStartDrag()` before that.
      UpdateGestureTarget(target, event_location);
      in_pinch_ = event->details().touch_points() != 1;
    }
  } else if (!in_gesture_drag_ &&
             (event->type() == ui::EventType::kGestureScrollBegin ||
              event->type() == ui::EventType::kGesturePinchBegin)) {
    // If `gesture_target_` exists but `in_gesture_drag_` is false then
    // the gesture has been received by `this` but the client has not
    // called `AttemptToStartDrag()` yet. We should update the
    // `event_location_in_gesture_target` because there could have been
    // a gesture type change.
    gfx::PointF location_in_target = event_location;
    aura::Window::ConvertPointToTarget(target, gesture_target_,
                                       &location_in_target);
    UpdateGestureTarget(gesture_target_, location_in_target);
  }

  if (event->details().touch_points() > 2) {
    if (CompleteDrag(DragResult::SUCCESS))
      event->StopPropagation();
    return;
  }

  switch (event->type()) {
    case ui::EventType::kGestureTapDown: {
      if (!(WindowResizer::GetBoundsChangeForWindowComponent(component) &
            WindowResizer::kBoundsChange_Resizes) ||
          (!client_area_drag && !CanStartOneFingerDrag(component)))
        return;

      ShowResizeShadow(target, component);

      AttemptToStartDrag(
          target, ConvertToLocationInParent(target, event_location), component,
          ::wm::WINDOW_MOVE_SOURCE_TOUCH, EndClosure(),
          /*update_gesture_target=*/false);
      event->StopPropagation();
      return;
    }
    case ui::EventType::kGestureEnd: {
      HideResizeShadow(target);

      if (window_resizer_ && (event->details().touch_points() == 1 ||
                              !CanStartOneFingerDrag(first_finger_hittest_))) {
        CompleteDrag(DragResult::SUCCESS);
        event->StopPropagation();
      }
      return;
    }
    case ui::EventType::kGestureBegin: {
      if (event->details().touch_points() == 1) {
        first_finger_touch_point_ = event_location;
        aura::Window::ConvertPointToTarget(target, target->parent(),
                                           &first_finger_touch_point_);
        first_finger_hittest_ = component;
      } else if (window_resizer_) {
        if (!window_resizer_->IsMove()) {
          // The transition from resizing with one finger to resizing with two
          // fingers causes unintended resizing because the location of
          // EventType::kGestureScrollUpdate jumps from the position of the
          // first finger to the position in the middle of the two fingers. For
          // this reason two finger resizing is not supported.
          CompleteDrag(DragResult::SUCCESS);
          event->StopPropagation();
        }
      } else {
        int second_finger_hittest = component;
        if (CanStartTwoFingerMove(target, first_finger_hittest_,
                                  second_finger_hittest)) {
          AttemptToStartDrag(target, first_finger_touch_point_, HTCAPTION,
                             ::wm::WINDOW_MOVE_SOURCE_TOUCH, EndClosure(),
                             /*update_gesture_target=*/false);
          event->StopPropagation();
        }
      }
      return;
    }
    case ui::EventType::kGestureScrollBegin: {
      // The one finger drag is not started in EventType::kGestureBegin to avoid
      // the window jumping upon initiating a two finger drag. When a one finger
      // drag is converted to a two finger drag, a jump occurs because the
      // location of the EventType::kGestureScrollUpdate event switches from the
      // single finger's position to the position in the middle of the two
      // fingers.
      if (window_resizer_.get()) {
        return;
      }

      if (!client_area_drag && !CanStartOneFingerDrag(component)) {
        return;
      }

      gfx::PointF location_in_parent = event_location;
      aura::Window::ConvertPointToTarget(target, target->parent(),
                                         &location_in_parent);
      AttemptToStartDrag(
          target, ConvertToLocationInParent(target, event_location), component,
          ::wm::WINDOW_MOVE_SOURCE_TOUCH, EndClosure(),
          /*update_gesture_target=*/false);
      event->StopPropagation();
      return;
    }
    case ui::EventType::kGesturePinchBegin: {
      if (AttemptToStartPinch(target,
                              ConvertToLocationInParent(target, event_location),
                              component, /*update_gesture_target=*/false)) {
        event->StopPropagation();
      }
      return;
    }
    case ui::EventType::kGestureTap:
      if (features::IsPipDoubleTapToResizeEnabled() &&
          Shell::Get()->pip_controller()->HandleDoubleTap(*event)) {
        event->StopPropagation();
        return;
      }
      break;
    default:
      break;
  }

  if (is_moving_floated_window_) {
    event->StopPropagation();
  }

  if (!window_resizer_.get()) {
    return;
  }

  switch (event->type()) {
    case ui::EventType::kGestureScrollUpdate: {
      // `EventType::kGestureScrollUpdate` is also called during a pinch but
      // should be ignored.
      if (in_pinch_) {
        return;
      }

      // `EventType::kGestureScrollBegin` is not called after a pinch ends, so
      // if a drag is ongoing after a pinch then `window_resizer_` has to be
      // reinitialized here.
      if (requires_reinitialization_) {
        if (!window_resizer_) {
          requires_reinitialization_ = false;
          return;
        }

        AttemptToStartDrag(
            target, ConvertToLocationInParent(target, event_location),
            component, wm::WINDOW_MOVE_SOURCE_TOUCH, EndClosure(),
            /*update_gesture_target=*/false);
        event->StopPropagation();
        return;
      }

      gfx::Rect bounds_in_screen = target->GetRootWindow()->GetBoundsInScreen();
      gfx::PointF screen_location = event->location_f();
      ::wm::ConvertPointToScreen(target, &screen_location);

      // It is physically not possible to move a touch pointer from one display
      // to another, so constrain the bounds to the display. This is important,
      // as it is possible for touch points to extend outside the bounds of the
      // display (as happens with gestures on the bezel), and dragging via touch
      // should not trigger moving to a new display.(see
      // https://crbug.com/917060)
      if (!bounds_in_screen.Contains(gfx::ToRoundedPoint(screen_location))) {
        float x = std::max(
            std::min(screen_location.x(), bounds_in_screen.right() - 1.f),
            static_cast<float>(bounds_in_screen.x()));
        float y = std::max(
            std::min(screen_location.y(), bounds_in_screen.bottom() - 1.f),
            static_cast<float>(bounds_in_screen.y()));
        gfx::PointF updated_location(x, y);
        ::wm::ConvertPointFromScreen(target, &updated_location);
        event->set_location_f(updated_location);
      }

      HandleDrag(target, event);
      event->StopPropagation();
      return;
    }
    case ui::EventType::kGestureScrollEnd:
      // We must complete the drag here instead of as a result of
      // EventType::kGestureEnd because otherwise the drag will be reverted when
      // EndMoveLoop() is called.
      // TODO(pkotwicz): Pass drag completion status to
      // WindowMoveClient::EndMoveLoop().
      CompleteDrag(DragResult::SUCCESS);
      event->StopPropagation();
      return;
    case ui::EventType::kGesturePinchEnd: {
      CompletePinch();
      event->StopPropagation();
      return;
    }
    case ui::EventType::kGesturePinchUpdate:
      // `EventType::kGesturePinchUpdate` is also called during two-finger edge
      // resize, but is handled with `EventType::kGestureScrollUpdate`.
      if (window_resizer_ && window_resizer_->IsResize()) {
        return;
      }

      HandlePinch(target, event);
      event->StopPropagation();
      return;
    case ui::EventType::kScrollFlingStart:
    case ui::EventType::kGestureSwipe:
      // Ignore swipe during pinch.
      if (in_pinch_ || (gesture_target_ && !in_gesture_drag_)) {
        return;
      }
      HandleFlingOrSwipe(event);
      return;
    default:
      return;
  }
}

wm::WindowMoveResult ToplevelWindowEventHandler::RunMoveLoop(
    aura::Window* source,
    const gfx::Vector2d& drag_offset,
    ::wm::WindowMoveSource move_source) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.
  aura::Window* root_window = source->GetRootWindow();
  DCHECK(root_window);
  gfx::PointF drag_location;
  if (move_source == ::wm::WINDOW_MOVE_SOURCE_TOUCH &&
      aura::Env::GetInstance()->is_touch_down()) {
    gfx::PointF drag_location_f;
    bool has_point = aura::Env::GetInstance()
                         ->gesture_recognizer()
                         ->GetLastTouchPointForTarget(source, &drag_location_f);
    drag_location = drag_location_f;
    DCHECK(has_point);
  } else {
    drag_location = gfx::PointF(
        root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot());
    aura::Window::ConvertPointToTarget(root_window, source->parent(),
                                       &drag_location);
  }
  // Set the cursor before calling AttemptToStartDrag(), as that will
  // eventually call LockCursor() and prevent the cursor from changing.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client)
    cursor_client->SetCursor(ui::mojom::CursorType::kPointer);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  DragResult result = DragResult::SUCCESS;
  if (!AttemptToStartDrag(source, drag_location, HTCAPTION, move_source,
                          base::BindOnce(OnDragCompleted, &result, &run_loop),
                          /*update_gesture_target=*/false)) {
    return ::wm::MOVE_CANCELED;
  }

  in_move_loop_ = true;
  base::WeakPtr<ToplevelWindowEventHandler> weak_ptr(
      weak_factory_.GetWeakPtr());

  // Disable window position auto management while dragging and restore it
  // aftrewards.
  WindowState* window_state = WindowState::Get(source);
  const bool window_position_managed = window_state->GetWindowPositionManaged();
  window_state->SetWindowPositionManaged(false);
  aura::WindowTracker tracker({source});

  run_loop.Run();

  if (!weak_ptr)
    return ::wm::MOVE_CANCELED;

  // Make sure the window hasn't been deleted.
  if (tracker.Contains(source))
    window_state->SetWindowPositionManaged(window_position_managed);

  in_move_loop_ = false;
  return result == DragResult::SUCCESS ? ::wm::MOVE_SUCCESSFUL
                                       : ::wm::MOVE_CANCELED;
}

void ToplevelWindowEventHandler::EndMoveLoop() {
  if (in_move_loop_)
    RevertDrag();
}

bool ToplevelWindowEventHandler::AttemptToStartDrag(
    aura::Window* window,
    const gfx::PointF& point_in_parent,
    int window_component,
    ToplevelWindowEventHandler::EndClosure end_closure) {
  if (first_finger_hittest_ == HTNOWHERE) {
    first_finger_hittest_ = window_component;
  }

  // This function is called from client to start either a drag or a
  // pinch gesture. There is a delay from when `this` first receives a
  // gesture begin event to when the client asks the gesture to be
  // initiated, during which time the gesture type may have changed. To
  // start the appropriate gesture, `this` keeps track of if the current
  // gesture is a drag or a pinch.
  if (in_pinch_) {
    return AttemptToStartPinch(window, point_in_parent, window_component,
                               /*update_gesture_target=*/true);
  }
  ::wm::WindowMoveSource source = gesture_target_
                                      ? ::wm::WINDOW_MOVE_SOURCE_TOUCH
                                      : ::wm::WINDOW_MOVE_SOURCE_MOUSE;
  return AttemptToStartDrag(window, point_in_parent, window_component, source,
                            std::move(end_closure),
                            /*update_gesture_target=*/true);
}

bool ToplevelWindowEventHandler::AttemptToStartDrag(
    aura::Window* window,
    const gfx::PointF& point_in_parent,
    int window_component,
    ::wm::WindowMoveSource source,
    EndClosure end_closure,
    bool update_gesture_target,
    bool grab_capture) {
  auto* env = aura::Env::GetInstance();
  // This may be called asynchronosly from remote client, and the mouse/touch
  // might have already been released.
  if ((source == ::wm::WINDOW_MOVE_SOURCE_TOUCH && !env->is_touch_down()) ||
      (source == ::wm::WINDOW_MOVE_SOURCE_MOUSE && !env->IsMouseButtonDown())) {
    LOG(WARNING) << "AttemptToStartDrag called when mouse/touch are not in "
                    "pressed state";
    return false;
  }

  if (gesture_target_ != nullptr && update_gesture_target) {
    DCHECK_EQ(source, ::wm::WINDOW_MOVE_SOURCE_TOUCH);
    // Transfer events for gesture if switching to new target.
    aura::Env::GetInstance()->gesture_recognizer()->TransferEventsTo(
        gesture_target_, window, ui::TransferTouchesBehavior::kDontCancel);
  }

  if (!PrepareForDrag(window, point_in_parent, window_component, source,
                      grab_capture)) {
    in_gesture_drag_ = false;

    // Treat failure to start as a revert.
    if (end_closure)
      std::move(end_closure).Run(DragResult::REVERT);

    return false;
  }

  end_closure_ = std::move(end_closure);
  in_gesture_drag_ = (source == ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  // `gesture_target_` and `first_finger_hittest_` need to be updated if the
  // drag originated from a client (i.e. `this` never handled
  // EventType::kGestureEventBegin).
  if (in_gesture_drag_ && (!gesture_target_ || update_gesture_target)) {
    UpdateGestureTarget(window);
  }

  if (auto* window_state = WindowState::Get(window);
      window_component == HTCAPTION && window_state &&
      window_state->IsFloated()) {
    // When a window is floated and we start a drag from the caption area,
    // stop propagation of any events that may show the tab strip.
    is_moving_floated_window_ = true;
  }

  // Mark the currently dragged or resized window as excluded for occlusion
  // purposes.
  scoped_exclude_.emplace(window);

  return true;
}

bool ToplevelWindowEventHandler::AttemptToStartPinch(
    aura::Window* window,
    const gfx::PointF& point_in_parent,
    int window_component,
    bool update_gesture_target) {
  if (gesture_target_ != nullptr && update_gesture_target) {
    // Transfer events for gesture if switching to new target.
    aura::Env::GetInstance()->gesture_recognizer()->TransferEventsTo(
        gesture_target_, window, ui::TransferTouchesBehavior::kDontCancel);
  }

  // `EventType::kGesturePinchBegin` is also called during two-finger edge
  // resize, in which case should be ignored. `IsResize()` is determined by the
  // gesture start location, so even though pinch resizes the window
  // `IsResize()` should be false.
  if (window_resizer_ && window_resizer_->IsResize()) {
    return false;
  }

  // Only gesture drag move can switch to pinch to resize. No other existing
  // resizer is allowed.
  bool in_gesture_drag_move =
      in_gesture_drag_ && window_resizer_ && window_resizer_->IsMove();
  if (window_resizer_ && !in_gesture_drag_move) {
    return false;
  }

  WindowState* window_state = WindowState::Get(window);
  // Pinch to resize is only applied to PiP windows.
  if (!window_state || !window_state->IsPip()) {
    return false;
  }

  if (!PrepareForPinch(window, point_in_parent, window_component)) {
    in_gesture_drag_ = false;
    return false;
  }

  in_gesture_drag_ = true;

  // `gesture_target_` and `first_finger_hittest_` need to be updated if
  // the drag originated from a client (i.e. `this` never handled
  // EventType::kGestureEventBegin).
  if (!gesture_target_ || update_gesture_target) {
    UpdateGestureTarget(window);
  }
  return true;
}

bool ToplevelWindowEventHandler::PrepareForPinch(
    aura::Window* window,
    const gfx::PointF& point_in_parent,
    int window_component) {
  // Do not allow resizing if the window's state is not managed by the window
  // manager.
  if (!WindowState::Get(window)) {
    return false;
  }

  // Reset `window_resizer_` if there is an ongoing drag move.
  if (window_resizer_ && in_gesture_drag_ && window_resizer_->IsMove()) {
    window_resizer_.reset();
    window_component = first_finger_hittest_;
  }

  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window, point_in_parent, window_component, wm::WINDOW_MOVE_SOURCE_TOUCH));
  if (!resizer) {
    return false;
  }
  window_resizer_ =
      std::make_unique<ScopedWindowResizer>(this, std::move(resizer), false);
  Shell::Get()->multi_display_metrics_controller()->OnWindowMovedOrResized(
      window);

  return true;
}

void ToplevelWindowEventHandler::RevertDrag() {
  CompleteDrag(DragResult::REVERT);
}

aura::Window* ToplevelWindowEventHandler::GetTargetForClientAreaGesture(
    ui::GestureEvent* event,
    aura::Window* target) {
  if (event->type() != ui::EventType::kGestureScrollBegin) {
    return nullptr;
  }

  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(target);
  if (!widget)
    return nullptr;

  aura::Window* toplevel = widget->GetNativeWindow();

  if (!display::Screen::GetScreen()->InTabletMode()) {
    return nullptr;
  }
  WindowState* window_state = WindowState::Get(toplevel);
  if (!window_state ||
      (!window_state->IsMaximized() && !window_state->IsFullscreen() &&
       !window_state->IsSnapped())) {
    return nullptr;
  }

  auto app_type = toplevel->GetProperty(chromeos::kAppTypeKey);
  if (app_type == chromeos::AppType::BROWSER ||
      app_type == chromeos::AppType::LACROS) {
    return nullptr;
  }

  if (event->details().scroll_y_hint() < 0)
    return nullptr;

  const gfx::Point location_in_screen =
      event->target()->GetScreenLocation(*event);
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(static_cast<aura::Window*>(event->target()))
          .work_area();

  gfx::Rect hit_bounds_in_screen(work_area_bounds);
  hit_bounds_in_screen.set_height(kDragStartTopEdgeInset);

  // There may be a bezel sensor off screen logically above
  // |hit_bounds_in_screen|. Handles the EventType::kGestureScrollBegin event
  // triggered in the bezel area too.
  bool in_bezel = location_in_screen.y() < hit_bounds_in_screen.y() &&
                  location_in_screen.x() >= hit_bounds_in_screen.x() &&
                  location_in_screen.x() < hit_bounds_in_screen.right();

  if (hit_bounds_in_screen.Contains(location_in_screen) || in_bezel)
    return toplevel;

  return nullptr;
}

void ToplevelWindowEventHandler::CompleteDragForTesting(DragResult result) {
  CompleteDrag(result);
}

void ToplevelWindowEventHandler::ResetWindowResizerForTesting() {
  window_resizer_.reset();
}

bool ToplevelWindowEventHandler::PrepareForDrag(
    aura::Window* window,
    const gfx::PointF& point_in_parent,
    int window_component,
    ::wm::WindowMoveSource source,
    bool grab_capture) {
  // Only gesture drag move can switch to pinch to resize. No other existing
  // resizer is allowed.
  bool in_gesture_drag_move = in_gesture_drag_ && window_resizer_->IsMove();
  if ((window_resizer_ && !in_gesture_drag_move) || !WindowState::Get(window)) {
    return false;
  }

  // If an ongoing resizing event exists (e.g. during transition from pinch to
  // drag), reset the resizer here, but use the old `window_component`.
  if (window_resizer_) {
    window_resizer_.reset();
    window_component = first_finger_hittest_;
  }

  std::unique_ptr<WindowResizer> resizer(
      CreateWindowResizer(window, point_in_parent, window_component, source));
  if (!resizer)
    return false;
  window_resizer_ = std::make_unique<ScopedWindowResizer>(
      this, std::move(resizer), grab_capture);
  Shell::Get()->multi_display_metrics_controller()->OnWindowMovedOrResized(
      window);

  requires_reinitialization_ = false;
  return true;
}

bool ToplevelWindowEventHandler::CompleteDrag(DragResult result) {
  scoped_exclude_.reset();

  if (!window_resizer_) {
    return false;
  }

  std::unique_ptr<ScopedWindowResizer> resizer(std::move(window_resizer_));
  switch (result) {
    case DragResult::SUCCESS:
      resizer->resizer()->CompleteDrag();
      break;
    case DragResult::REVERT:
      UpdateGestureTarget(nullptr);
      resizer->resizer()->RevertDrag();
      break;
    case DragResult::WINDOW_DESTROYED:
      UpdateGestureTarget(nullptr);

      // We explicitly do not invoke RevertDrag() since that may do things to
      // the window that was destroyed.
      break;
  }

  first_finger_hittest_ = HTNOWHERE;
  in_gesture_drag_ = false;
  in_pinch_ = false;
  requires_reinitialization_ = false;
  if (end_closure_)
    std::move(end_closure_).Run(result);
  return true;
}

bool ToplevelWindowEventHandler::CompletePinch() {
  if (!window_resizer_) {
    return false;
  }

  // Reinitialize the `window_resizer_` if an `EventType::kGestureScrollUpdate`
  // event is called right after pinch is completed. This is necessary because
  // `EventType::kGestureScrollBegin` event is not called after
  // `EventType::kGesturePinchEnd`.
  requires_reinitialization_ = true;
  return true;
}

void ToplevelWindowEventHandler::HandleMousePressed(aura::Window* target,
                                                    ui::MouseEvent* event) {
  if (event->phase() != ui::EP_PRETARGET || !target->delegate())
    return;

  if (features::IsPipDoubleTapToResizeEnabled() &&
      Shell::Get()->pip_controller()->HandleDoubleTap(*event)) {
    event->SetHandled();
    return;
  }

  // We also update the current window component here because for the
  // mouse-drag-release-press case, where the mouse is released and
  // pressed without mouse move event.
  int component = window_util::GetNonClientComponent(target, event->location());
  if ((event->flags() & (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK)) ==
          0 &&
      WindowResizer::GetBoundsChangeForWindowComponent(component)) {
    AttemptToStartDrag(target,
                       ConvertToLocationInParent(target, event->location_f()),
                       component, ::wm::WINDOW_MOVE_SOURCE_MOUSE, EndClosure(),
                       /*update_gesture_target=*/false);
    // Set as handled so that other event handlers do no act upon the event
    // but still receive it so that they receive both parts of each pressed/
    // released pair.
    event->SetHandled();
  } else {
    CompleteDrag(DragResult::SUCCESS);
  }
}

void ToplevelWindowEventHandler::HandleMouseReleased(aura::Window* target,
                                                     ui::MouseEvent* event) {
  if (event->phase() == ui::EP_PRETARGET)
    CompleteDrag(DragResult::SUCCESS);
}

void ToplevelWindowEventHandler::HandleDrag(aura::Window* target,
                                            ui::LocatedEvent* event) {
  // This function only be triggered to move window
  // by mouse drag or touch move event.
  DCHECK(event->type() == ui::EventType::kMouseDragged ||
         event->type() == ui::EventType::kTouchMoved ||
         event->type() == ui::EventType::kGestureScrollUpdate);

  // Drag actions are performed pre-target handling to prevent spurious mouse
  // moves from the move/size operation from being sent to the target.
  if (event->phase() != ui::EP_PRETARGET)
    return;

  // Break the Snap Group when dragging a window out of it. Check
  // `window_resizer_` to avoid breaking the group if it is tab dragging.
  if (SnapGroupController* snap_group_controller = SnapGroupController::Get()) {
    if (SnapGroup* snap_group =
            snap_group_controller->GetSnapGroupForGivenWindow(target);
        snap_group && window_resizer_) {
      snap_group->OnLocatedEvent(event);
    }
  }

  // `window_resizer_` may have been reset, early return in this case.
  if (!window_resizer_) {
    return;
  }

  gfx::PointF location_in_parent = event->location_f();
  aura::Window::ConvertPointToTarget(
      target, window_resizer_->resizer()->GetTarget()->parent(),
      &location_in_parent);
  window_resizer_->resizer()->Drag(location_in_parent, event->flags());
  event->StopPropagation();
}

void ToplevelWindowEventHandler::HandlePinch(aura::Window* target,
                                             ui::GestureEvent* event) {
  // This function is only to be triggered to move and resize
  // a PiP window with a pinch event.
  CHECK_EQ(event->type(), ui::EventType::kGesturePinchUpdate);

  if (!window_resizer_ || !in_pinch_) {
    return;
  }

  gfx::PointF location_in_parent = event->location_f();
  aura::Window::ConvertPointToTarget(
      target, window_resizer_->resizer()->GetTarget()->parent(),
      &location_in_parent);
  window_resizer_->resizer()->Pinch(location_in_parent,
                                    event->details().scale());
  event->StopPropagation();
}

void ToplevelWindowEventHandler::HandleMouseMoved(aura::Window* target,
                                                  ui::LocatedEvent* event) {
  // Shadow effects are applied after target handling. Note that we don't
  // respect ER_HANDLED here right now since we have not had a reason to allow
  // the target to cancel shadow rendering.
  if (event->phase() != ui::EP_POSTTARGET || !target->delegate())
    return;

  // TODO(jamescook): Move the resize cursor update code into here from
  // CompoundEventFilter?
  if (event->flags() & ui::EF_IS_NON_CLIENT) {
    int component =
        window_util::GetNonClientComponent(target, event->location());
    ShowResizeShadow(target, component);
  } else {
    HideResizeShadow(target);
  }
}

void ToplevelWindowEventHandler::HandleMouseExited(aura::Window* target,
                                                   ui::LocatedEvent* event) {
  // Shadow effects are applied after target handling. Note that we don't
  // respect ER_HANDLED here right now since we have not had a reason to allow
  // the target to cancel shadow rendering.
  if (event->phase() != ui::EP_POSTTARGET)
    return;

  HideResizeShadow(target);
}

void ToplevelWindowEventHandler::HandleCaptureLost(ui::LocatedEvent* event) {
  if (event->phase() == ui::EP_PRETARGET) {
    // We complete the drag instead of reverting it, as reverting it will result
    // in a weird behavior when a dragged tab produces a modal dialog while the
    // drag is in progress. crbug.com/558201.
    CompleteDrag(DragResult::SUCCESS);
  }
}

void ToplevelWindowEventHandler::HandleFlingOrSwipe(ui::GestureEvent* event) {
  UpdateGestureTarget(nullptr);
  if (!window_resizer_)
    return;

  std::unique_ptr<ScopedWindowResizer> resizer(std::move(window_resizer_));
  resizer->resizer()->FlingOrSwipe(event);
  first_finger_hittest_ = HTNOWHERE;
  in_gesture_drag_ = false;
  requires_reinitialization_ = false;
  in_pinch_ = false;
  if (end_closure_)
    std::move(end_closure_).Run(DragResult::SUCCESS);
}

void ToplevelWindowEventHandler::ResizerWindowDestroyed() {
  CompleteDrag(DragResult::WINDOW_DESTROYED);
}

void ToplevelWindowEventHandler::OnWillApplyDisplayChanges() {
  CompleteDrag(DragResult::REVERT);
}

void ToplevelWindowEventHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(gesture_target_, window);
  if (gesture_target_ == window)
    UpdateGestureTarget(nullptr);
}

void ToplevelWindowEventHandler::UpdateGestureTarget(
    aura::Window* target,
    const gfx::PointF& location) {
  event_location_in_gesture_target_ = location;
  if (gesture_target_ == target)
    return;

  if (gesture_target_)
    gesture_target_->RemoveObserver(this);
  gesture_target_ = target;
  if (gesture_target_)
    gesture_target_->AddObserver(this);
}

}  // namespace ash
