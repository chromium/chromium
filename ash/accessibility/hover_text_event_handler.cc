// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/hover_text_event_handler.h"

#include "ash/accessibility/accessibility_event_handler_manager.h"
#include "ash/accessibility/hover_text_event_handler_delegate.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/aura/env.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_target.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

namespace {

// Rate-limits mouse and touch coordinate dispatches to prevent saturating
// extension IPC during high-report-rate pointer input. Both mice and
// touchscreens can report positions much faster than the extension needs to
// hit-test them.
constexpr base::TimeDelta kMinPointDispatchInterval = base::Milliseconds(30);

bool IsControlKey(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_LCONTROL ||
         key_code == ui::VKEY_RCONTROL;
}

bool IsSearchKey(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_LWIN || key_code == ui::VKEY_RWIN;
}

gfx::Point GetScreenLocationFromEvent(const ui::LocatedEvent& event) {
  return event.target() ? event.target()->GetScreenLocation(event)
                        : event.root_location();
}

}  // namespace

HoverTextEventHandler::HoverTextEventHandler(
    HoverTextEventHandlerDelegate* delegate)
    : delegate_(delegate) {
  Shell::Get()->AddAccessibilityEventHandler(
      this, AccessibilityEventHandlerManager::HandlerType::kHoverText);
}

HoverTextEventHandler::~HoverTextEventHandler() {
  if (IsActive()) {
    delegate_->OnHoverTextKeyEvent(false, /*stop_speech=*/true);
  }
  Shell::Get()->RemoveAccessibilityEventHandler(this);
}

bool HoverTextEventHandler::IsActive() const {
  return activation_key_held_ || tray_toggled_active_;
}

void HoverTextEventHandler::OnTrayToggled() {
  const bool was_active = IsActive();
  tray_toggled_active_ = !tray_toggled_active_;
  if (tray_toggled_active_) {
    activation_key_held_ = false;
    chord_cancelled_ = false;
    bubble_shown_during_search_hold_ = false;
  }
  const bool is_active = IsActive();
  if (!was_active && is_active) {
    aura::Env::GetInstance()->gesture_recognizer()->CancelActiveTouchesExcept(
        nullptr);
  } else if (!is_active) {
    OnHoverTextDeactivated();
  }
  delegate_->OnHoverTextKeyEvent(is_active, /*stop_speech=*/!is_active);
}

void HoverTextEventHandler::OnHoverTextBubbleShown() {
  if (activation_key_held_ && !tray_toggled_active_) {
    bubble_shown_during_search_hold_ = true;
  }
}

void HoverTextEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if ((event->type() == ui::EventType::kMouseDragged ||
       event->type() == ui::EventType::kMousePressed) &&
      activation_key_held_) {
    // It appears the user is holding Search to do something other than Hover
    // Text, so cancel the activation.
    CancelActivationChord();
  } else if (event->type() == ui::EventType::kMouseMoved) {
    MaybeDispatchMousePoint(*event);
  }
}

void HoverTextEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  // TODO(b/568813139): Pass through touches on `HoverTextTray` and the virtual
  // keyboard so touch-only users can toggle Hover Text off or hide the virtual
  // keyboard.
  const ui::EventType event_type = event->type();
  const ui::PointerId pointer_id = event->pointer_details().id;
  if (event_type == ui::EventType::kTouchPressed && IsActive()) {
    // Only the first finger pressed in a touch sequence drives hover points.
    if (touch_id_ == ui::kPointerIdUnknown && consumed_touch_ids_.empty()) {
      touch_id_ = pointer_id;
    }
    // Track every finger pressed while active so it stays consumed through its
    // release, even if the tracked finger lifts first or Hover Text
    // deactivates mid-touch.
    consumed_touch_ids_.insert(pointer_id);
    consuming_touch_sequence_ = true;
  }

  // Touches that started before Hover Text became active were already
  // cancelled on activation and are ignored here.
  if (!consumed_touch_ids_.contains(pointer_id)) {
    return;
  }

  // Record the tracked finger's latest event ID and screen location for
  // `OnGestureEvent` to dispatch.
  if (touch_id_ == pointer_id) {
    tracked_touch_unique_event_id_ = event->unique_event_id();
    tracked_touch_location_ = GetScreenLocationFromEvent(*event);
  }

  if (event_type == ui::EventType::kTouchReleased ||
      event_type == ui::EventType::kTouchCancelled) {
    consumed_touch_ids_.erase(pointer_id);
  }

  // Stop raw touch propagation so touch exploration does not click or drag
  // underlying windows, while forcing gesture recognition so synthesized
  // gestures still reach `OnGestureEvent`.
  event->StopPropagation();
  event->ForceProcessGesture();
}

void HoverTextEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (!consuming_touch_sequence_) {
    return;
  }

  if (event->type() == ui::EventType::kGestureEnd &&
      consumed_touch_ids_.empty()) {
    consuming_touch_sequence_ = false;
  }

  event->SetHandled();
  event->StopPropagation();

  const uint32_t unique_touch_event_id = event->unique_touch_event_id();
  if (!IsActive() || touch_id_ == ui::kPointerIdUnknown ||
      tracked_touch_unique_event_id_ == 0 ||
      unique_touch_event_id != tracked_touch_unique_event_id_) {
    return;
  }

  switch (event->type()) {
    case ui::EventType::kGestureBegin:
      if (event->details().touch_points() == 1) {
        // A press is a single event rather than a stream, so it is sent
        // immediately, like a release.
        DispatchPoint(tracked_touch_location_,
                      HoverTextPointerType::kTouchPress);
      }
      break;
    case ui::EventType::kGestureScrollUpdate:
      // Only scrolls triggered by the tracked first finger's own touch events
      // reach here, so a second finger scrolling alongside it is ignored and
      // the hover point follows the first finger rather than the centroid.
      if (consumed_touch_ids_.contains(touch_id_)) {
        ScheduleDispatchPoint(tracked_touch_location_,
                              HoverTextPointerType::kTouchPress);
      }
      break;
    case ui::EventType::kGestureEnd:
      if (!consumed_touch_ids_.contains(touch_id_)) {
        touch_id_ = ui::kPointerIdUnknown;
        tracked_touch_unique_event_id_ = 0;
        // A pending hit test on lift uses the extension's last non-release
        // point, so send any waiting touch move before the release.
        if (point_dispatch_timer_.IsRunning() &&
            pending_point_pointer_type_ == HoverTextPointerType::kTouchPress) {
          DispatchPendingPoint();
        }
        DispatchPoint(tracked_touch_location_,
                      HoverTextPointerType::kTouchRelease);
      }
      break;
    default:
      break;
  }
}

void HoverTextEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (type != ui::EventType::kKeyPressed &&
      type != ui::EventType::kKeyReleased) {
    return;
  }

  const ui::KeyboardCode key_code = event->key_code();
  if (IsSearchKey(key_code)) {
    HandleSearchKeyEvent(event);
    return;
  }

  if (IsControlKey(key_code)) {
    HandleControlKeyEvent(*event);
    return;
  }

  // Pressing any other key while the Search activation key is held cancels the
  // key-hold state so keyboard shortcuts (e.g. Search+Shift+H) and normal
  // typing never trigger or leave behind a Hover Text bubble.
  if (type == ui::EventType::kKeyPressed && activation_key_held_) {
    CancelActivationChord();
  }
}

void HoverTextEventHandler::HandleSearchKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::EventType::kKeyPressed) {
    const bool is_repeat = (event->flags() & ui::EF_IS_REPEAT) != 0;
    if (is_repeat || activation_key_held_ || chord_cancelled_) {
      return;
    }
    if (tray_toggled_active_) {
      // When Hover Text is already locked active via the shelf tray button,
      // pressing Search stops active speech without arming Search-hold
      // tracking, so a bare Search tap still opens the launcher even if a
      // bubble update arrives while Search is down.
      delegate_->OnHoverTextKeyEvent(/*is_active=*/true, /*stop_speech=*/true);
      return;
    }
    bubble_shown_during_search_hold_ = false;
    // Pressing either `Ctrl` or `Search` stops active speech immediately.
    SetActivationKeyHeld(/*is_key_held=*/true, /*stop_speech=*/true);
    return;
  }

  // Search activation key released. Cancel the key-up event only if Hover
  // Text displayed a bubble during this Search hold so the launcher does not
  // open after hovering, while allowing a bare Search tap to open the
  // launcher normally.
  const bool was_held = activation_key_held_;
  const bool should_cancel_key_release =
      was_held && bubble_shown_during_search_hold_;
  chord_cancelled_ = false;
  bubble_shown_during_search_hold_ = false;
  if (was_held) {
    SetActivationKeyHeld(/*is_key_held=*/false, /*stop_speech=*/true);
  }
  if (should_cancel_key_release) {
    event->SetHandled();
    event->StopPropagation();
  }
}

void HoverTextEventHandler::HandleControlKeyEvent(const ui::KeyEvent& event) {
  const bool is_repeat = (event.flags() & ui::EF_IS_REPEAT) != 0;
  if (event.type() == ui::EventType::kKeyPressed && !is_repeat) {
    delegate_->OnHoverTextKeyEvent(IsActive(),
                                   /*stop_speech=*/true);
  }
}

void HoverTextEventHandler::CancelActivationChord() {
  chord_cancelled_ = true;
  bubble_shown_during_search_hold_ = false;
  SetActivationKeyHeld(/*is_key_held=*/false, /*stop_speech=*/false);
}

void HoverTextEventHandler::SetActivationKeyHeld(bool is_key_held,
                                                 bool stop_speech) {
  if (activation_key_held_ == is_key_held) {
    return;
  }
  const bool was_active = IsActive();
  activation_key_held_ = is_key_held;
  const bool now_active = IsActive();
  if (!was_active && now_active) {
    aura::Env::GetInstance()->gesture_recognizer()->CancelActiveTouchesExcept(
        nullptr);
  } else if (!now_active) {
    OnHoverTextDeactivated();
  }
  delegate_->OnHoverTextKeyEvent(now_active, stop_speech);
}

void HoverTextEventHandler::MaybeDispatchMousePoint(
    const ui::MouseEvent& event) {
  // Ignore synthetic mouse events generated from touch input so touch-triggered
  // bubbles are handled exclusively by `OnTouchEvent`.
  if (event.flags() & ui::EF_FROM_TOUCH) {
    return;
  }
  // Forward mouse coordinates only while Hover Text is active (Search held down
  // or toggled active via the shelf status area tray button) so pressing Search
  // alone at a static cursor never triggers Hover Text until the pointer moves
  // after activation.
  if (!IsActive()) {
    return;
  }
  ScheduleDispatchPoint(GetScreenLocationFromEvent(event),
                        HoverTextPointerType::kMouse);
}

void HoverTextEventHandler::ScheduleDispatchPoint(
    const gfx::Point& screen_point,
    HoverTextPointerType pointer_type) {
  const bool dispatch_now =
      last_point_dispatch_time_.is_null() ||
      base::TimeTicks::Now() - last_point_dispatch_time_ >=
          kMinPointDispatchInterval;
  if (dispatch_now) {
    DispatchPoint(screen_point, pointer_type);
    return;
  }

  // Too soon after the last dispatch. Replace any point already waiting to be
  // sent with this one, so the extension receives the most recent point about
  // once per dispatch interval, and the timer sends the final resting point
  // after the pointer stops moving.
  pending_point_ = screen_point;
  pending_point_pointer_type_ = pointer_type;
  point_dispatch_timer_.Start(
      FROM_HERE, kMinPointDispatchInterval,
      base::BindOnce(&HoverTextEventHandler::DispatchPendingPoint,
                     base::Unretained(this)));
}

void HoverTextEventHandler::DispatchPoint(const gfx::Point& screen_point,
                                          HoverTextPointerType pointer_type) {
  point_dispatch_timer_.Stop();
  last_point_dispatch_time_ = base::TimeTicks::Now();
  delegate_->OnHoverTextPointUpdated(screen_point, pointer_type);
}

void HoverTextEventHandler::DispatchPendingPoint() {
  if (!IsActive()) {
    return;
  }
  DispatchPoint(pending_point_, pending_point_pointer_type_);
}

void HoverTextEventHandler::OnHoverTextDeactivated() {
  point_dispatch_timer_.Stop();
  last_point_dispatch_time_ = base::TimeTicks();
  touch_id_ = ui::kPointerIdUnknown;
  tracked_touch_unique_event_id_ = 0;
}

}  // namespace ash
