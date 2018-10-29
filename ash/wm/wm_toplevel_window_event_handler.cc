// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_toplevel_window_event_handler.h"

#include "ash/shell.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"

namespace ash {
namespace wm {

namespace {

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
  if (!GetWindowState(window)->IsNormalOrSnapped() ||
      window->type() != aura::client::WINDOW_TYPE_NORMAL) {
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
  // Window resize in tablet mode is disabled (except in splitscreen).
  if (Shell::Get()
          ->tablet_mode_controller()
          ->IsTabletModeWindowManagerEnabled()) {
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
  if (resize_shadow_controller)
    resize_shadow_controller->HideShadow(window);
}

}  // namespace

// ScopedWindowResizer ---------------------------------------------------------

// Wraps a WindowResizer and installs an observer on its target window.  When
// the window is destroyed ResizerWindowDestroyed() is invoked back on the
// WmToplevelWindowEventHandler to clean up.
class WmToplevelWindowEventHandler::ScopedWindowResizer
    : public aura::WindowObserver,
      public wm::WindowStateObserver {
 public:
  ScopedWindowResizer(WmToplevelWindowEventHandler* handler,
                      std::unique_ptr<WindowResizer> resizer);
  ~ScopedWindowResizer() override;

  // Returns true if the drag moves the window and does not resize.
  bool IsMove() const;

  WindowResizer* resizer() { return resizer_.get(); }

  // WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  // WindowStateObserver overrides:
  void OnPreWindowStateTypeChange(wm::WindowState* window_state,
                                  mojom::WindowStateType type) override;

 private:
  WmToplevelWindowEventHandler* handler_;
  std::unique_ptr<WindowResizer> resizer_;

  // Whether ScopedWindowResizer grabbed capture.
  bool grabbed_capture_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowResizer);
};

WmToplevelWindowEventHandler::ScopedWindowResizer::ScopedWindowResizer(
    WmToplevelWindowEventHandler* handler,
    std::unique_ptr<WindowResizer> resizer)
    : handler_(handler), resizer_(std::move(resizer)), grabbed_capture_(false) {
  aura::Window* target = resizer_->GetTarget();
  target->AddObserver(this);
  GetWindowState(target)->AddObserver(this);

  if (!target->HasCapture()) {
    grabbed_capture_ = true;
    target->SetCapture();
  }
}

WmToplevelWindowEventHandler::ScopedWindowResizer::~ScopedWindowResizer() {
  aura::Window* target = resizer_->GetTarget();
  target->RemoveObserver(this);
  GetWindowState(target)->RemoveObserver(this);
  if (grabbed_capture_)
    target->ReleaseCapture();
}

bool WmToplevelWindowEventHandler::ScopedWindowResizer::IsMove() const {
  return resizer_->details().bounds_change ==
         WindowResizer::kBoundsChange_Repositions;
}

void WmToplevelWindowEventHandler::ScopedWindowResizer::
    OnPreWindowStateTypeChange(wm::WindowState* window_state,
                               mojom::WindowStateType old) {
  handler_->CompleteDrag(DragResult::SUCCESS);
}

void WmToplevelWindowEventHandler::ScopedWindowResizer::OnWindowDestroying(
    aura::Window* window) {
  DCHECK_EQ(resizer_->GetTarget(), window);
  handler_->ResizerWindowDestroyed();
}

// WmToplevelWindowEventHandler
// --------------------------------------------------

WmToplevelWindowEventHandler::WmToplevelWindowEventHandler()
    : first_finger_hittest_(HTNOWHERE) {
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
}

WmToplevelWindowEventHandler::~WmToplevelWindowEventHandler() {
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
}

void WmToplevelWindowEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (window_resizer_.get() && event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    CompleteDrag(DragResult::REVERT);
  }
}

void WmToplevelWindowEventHandler::OnMouseEvent(ui::MouseEvent* event,
                                                aura::Window* target) {
  UpdateGestureTarget(nullptr);

  if (event->handled())
    return;
  if ((event->flags() &
       (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return;

  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED) {
    // Capture is grabbed when both gesture and mouse drags start. Handle
    // capture loss regardless of which type of drag is in progress.
    HandleCaptureLost(event);
    return;
  }

  if (in_gesture_drag_)
    return;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      HandleMousePressed(target, event);
      break;
    case ui::ET_MOUSE_DRAGGED:
      HandleDrag(target, event);
      break;
    case ui::ET_MOUSE_RELEASED:
      HandleMouseReleased(target, event);
      break;
    case ui::ET_MOUSE_MOVED:
      HandleMouseMoved(target, event);
      break;
    case ui::ET_MOUSE_EXITED:
      HandleMouseExited(target, event);
      break;
    default:
      break;
  }
}

void WmToplevelWindowEventHandler::OnGestureEvent(ui::GestureEvent* event,
                                                  aura::Window* target) {
  if (event->type() == ui::ET_GESTURE_END)
    UpdateGestureTarget(nullptr);
  else if (event->type() == ui::ET_GESTURE_BEGIN)
    UpdateGestureTarget(target, event->location());

  if (event->handled())
    return;
  if (!target->delegate())
    return;

  if (window_resizer_.get() && !in_gesture_drag_)
    return;

  if (window_resizer_.get() &&
      window_resizer_->resizer()->GetTarget() != target) {
    return;
  }

  if (event->details().touch_points() > 2) {
    if (CompleteDrag(DragResult::SUCCESS))
      event->StopPropagation();
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      int component = GetNonClientComponent(target, event->location());
      if (!(WindowResizer::GetBoundsChangeForWindowComponent(component) &
            WindowResizer::kBoundsChange_Resizes))
        return;
      ShowResizeShadow(target, component);
      return;
    }
    case ui::ET_GESTURE_END: {
      HideResizeShadow(target);

      if (window_resizer_.get() &&
          (event->details().touch_points() == 1 ||
           !CanStartOneFingerDrag(first_finger_hittest_))) {
        CompleteDrag(DragResult::SUCCESS);
        event->StopPropagation();
      }
      return;
    }
    case ui::ET_GESTURE_BEGIN: {
      if (event->details().touch_points() == 1) {
        first_finger_touch_point_ = event->location();
        aura::Window::ConvertPointToTarget(target, target->parent(),
                                           &first_finger_touch_point_);
        first_finger_hittest_ =
            GetNonClientComponent(target, event->location());
      } else if (window_resizer_.get()) {
        if (!window_resizer_->IsMove()) {
          // The transition from resizing with one finger to resizing with two
          // fingers causes unintended resizing because the location of
          // ET_GESTURE_SCROLL_UPDATE jumps from the position of the first
          // finger to the position in the middle of the two fingers. For this
          // reason two finger resizing is not supported.
          CompleteDrag(DragResult::SUCCESS);
          event->StopPropagation();
        }
      } else {
        int second_finger_hittest =
            GetNonClientComponent(target, event->location());
        if (CanStartTwoFingerMove(target, first_finger_hittest_,
                                  second_finger_hittest)) {
          AttemptToStartDrag(target, first_finger_touch_point_, HTCAPTION,
                             ::wm::WINDOW_MOVE_SOURCE_TOUCH, EndClosure());
          event->StopPropagation();
        }
      }
      return;
    }
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      // The one finger drag is not started in ET_GESTURE_BEGIN to avoid the
      // window jumping upon initiating a two finger drag. When a one finger
      // drag is converted to a two finger drag, a jump occurs because the
      // location of the ET_GESTURE_SCROLL_UPDATE event switches from the single
      // finger's position to the position in the middle of the two fingers.
      if (window_resizer_.get())
        return;
      int component = GetNonClientComponent(target, event->location());
      if (!CanStartOneFingerDrag(component))
        return;
      gfx::Point location_in_parent = event->location();
      aura::Window::ConvertPointToTarget(target, target->parent(),
                                         &location_in_parent);
      AttemptToStartDrag(target, location_in_parent, component,
                         ::wm::WINDOW_MOVE_SOURCE_TOUCH, EndClosure());
      event->StopPropagation();
      return;
    }
    default:
      break;
  }

  if (!window_resizer_.get())
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_UPDATE:
      HandleDrag(target, event);
      event->StopPropagation();
      return;
    case ui::ET_GESTURE_SCROLL_END:
      // We must complete the drag here instead of as a result of ET_GESTURE_END
      // because otherwise the drag will be reverted when EndMoveLoop() is
      // called.
      // TODO(pkotwicz): Pass drag completion status to
      // WindowMoveClient::EndMoveLoop().
      CompleteDrag(DragResult::SUCCESS);
      event->StopPropagation();
      return;
    case ui::ET_SCROLL_FLING_START:
      FALLTHROUGH;
    case ui::ET_GESTURE_SWIPE:
      HandleFlingOrSwipe(event);
      return;
    default:
      return;
  }
}

bool WmToplevelWindowEventHandler::AttemptToStartDrag(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component,
    ::wm::WindowMoveSource source,
    EndClosure end_closure) {
  if (!PrepareForDrag(window, point_in_parent, window_component, source)) {
    // Treat failure to start as a revert.
    if (end_closure)
      std::move(end_closure).Run(DragResult::REVERT);
    return false;
  }

  end_closure_ = std::move(end_closure);
  in_gesture_drag_ = (source == ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  return true;
}

void WmToplevelWindowEventHandler::RevertDrag() {
  CompleteDrag(DragResult::REVERT);
}

bool WmToplevelWindowEventHandler::PrepareForDrag(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component,
    ::wm::WindowMoveSource source) {
  if (window_resizer_)
    return false;

  std::unique_ptr<WindowResizer> resizer(
      CreateWindowResizer(window, point_in_parent, window_component, source));
  if (!resizer)
    return false;
  window_resizer_ =
      std::make_unique<ScopedWindowResizer>(this, std::move(resizer));
  return true;
}

bool WmToplevelWindowEventHandler::CompleteDrag(DragResult result) {
  UpdateGestureTarget(nullptr);

  if (!window_resizer_)
    return false;

  std::unique_ptr<ScopedWindowResizer> resizer(std::move(window_resizer_));
  switch (result) {
    case DragResult::SUCCESS:
      resizer->resizer()->CompleteDrag();
      break;
    case DragResult::REVERT:
      resizer->resizer()->RevertDrag();
      break;
    case DragResult::WINDOW_DESTROYED:
      // We explicitly do not invoke RevertDrag() since that may do things to
      // the window that was destroyed.
      break;
  }

  first_finger_hittest_ = HTNOWHERE;
  in_gesture_drag_ = false;
  if (end_closure_)
    std::move(end_closure_).Run(result);
  return true;
}

void WmToplevelWindowEventHandler::HandleMousePressed(aura::Window* target,
                                                      ui::MouseEvent* event) {
  if (event->phase() != ui::EP_PRETARGET || !target->delegate())
    return;

  // We also update the current window component here because for the
  // mouse-drag-release-press case, where the mouse is released and
  // pressed without mouse move event.
  int component = GetNonClientComponent(target, event->location());
  if ((event->flags() & (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK)) ==
          0 &&
      WindowResizer::GetBoundsChangeForWindowComponent(component)) {
    gfx::Point location_in_parent = event->location();
    aura::Window::ConvertPointToTarget(target, target->parent(),
                                       &location_in_parent);
    AttemptToStartDrag(target, location_in_parent, component,
                       ::wm::WINDOW_MOVE_SOURCE_MOUSE, EndClosure());
    // Set as handled so that other event handlers do no act upon the event
    // but still receive it so that they receive both parts of each pressed/
    // released pair.
    event->SetHandled();
  } else {
    CompleteDrag(DragResult::SUCCESS);
  }
}

void WmToplevelWindowEventHandler::HandleMouseReleased(aura::Window* target,
                                                       ui::MouseEvent* event) {
  if (event->phase() == ui::EP_PRETARGET)
    CompleteDrag(DragResult::SUCCESS);
}

void WmToplevelWindowEventHandler::HandleDrag(aura::Window* target,
                                              ui::LocatedEvent* event) {
  // This function only be triggered to move window
  // by mouse drag or touch move event.
  DCHECK(event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED ||
         event->type() == ui::ET_GESTURE_SCROLL_UPDATE);

  // Drag actions are performed pre-target handling to prevent spurious mouse
  // moves from the move/size operation from being sent to the target.
  if (event->phase() != ui::EP_PRETARGET)
    return;

  if (!window_resizer_)
    return;
  gfx::Point location_in_parent = event->location();
  aura::Window::ConvertPointToTarget(target, target->parent(),
                                     &location_in_parent);
  window_resizer_->resizer()->Drag(location_in_parent, event->flags());
  event->StopPropagation();
}

void WmToplevelWindowEventHandler::HandleMouseMoved(aura::Window* target,
                                                    ui::LocatedEvent* event) {
  // Shadow effects are applied after target handling. Note that we don't
  // respect ER_HANDLED here right now since we have not had a reason to allow
  // the target to cancel shadow rendering.
  if (event->phase() != ui::EP_POSTTARGET || !target->delegate())
    return;

  // TODO(jamescook): Move the resize cursor update code into here from
  // CompoundEventFilter?
  if (event->flags() & ui::EF_IS_NON_CLIENT) {
    int component = GetNonClientComponent(target, event->location());
    ShowResizeShadow(target, component);
  } else {
    HideResizeShadow(target);
  }
}

void WmToplevelWindowEventHandler::HandleMouseExited(aura::Window* target,
                                                     ui::LocatedEvent* event) {
  // Shadow effects are applied after target handling. Note that we don't
  // respect ER_HANDLED here right now since we have not had a reason to allow
  // the target to cancel shadow rendering.
  if (event->phase() != ui::EP_POSTTARGET)
    return;

  HideResizeShadow(target);
}

void WmToplevelWindowEventHandler::HandleCaptureLost(ui::LocatedEvent* event) {
  if (event->phase() == ui::EP_PRETARGET) {
    // We complete the drag instead of reverting it, as reverting it will result
    // in a weird behavior when a dragged tab produces a modal dialog while the
    // drag is in progress. crbug.com/558201.
    CompleteDrag(DragResult::SUCCESS);
  }
}

void WmToplevelWindowEventHandler::HandleFlingOrSwipe(ui::GestureEvent* event) {
  UpdateGestureTarget(nullptr);
  if (!window_resizer_)
    return;

  std::unique_ptr<ScopedWindowResizer> resizer(std::move(window_resizer_));
  resizer->resizer()->FlingOrSwipe(event);
  first_finger_hittest_ = HTNOWHERE;
  in_gesture_drag_ = false;
  if (end_closure_)
    std::move(end_closure_).Run(DragResult::SUCCESS);
}

void WmToplevelWindowEventHandler::ResizerWindowDestroyed() {
  CompleteDrag(DragResult::WINDOW_DESTROYED);
}

void WmToplevelWindowEventHandler::OnDisplayConfigurationChanging() {
  CompleteDrag(DragResult::REVERT);
}

void WmToplevelWindowEventHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(gesture_target_, window);
  if (gesture_target_ == window)
    UpdateGestureTarget(nullptr);
}

void WmToplevelWindowEventHandler::UpdateGestureTarget(
    aura::Window* target,
    const gfx::Point& location) {
  event_location_in_gesture_target_ = location;
  if (gesture_target_ == target)
    return;

  if (gesture_target_)
    gesture_target_->RemoveObserver(this);
  gesture_target_ = target;
  if (gesture_target_)
    gesture_target_->AddObserver(this);
}

}  // namespace wm
}  // namespace ash
