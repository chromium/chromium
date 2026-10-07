// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_pointer_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// The amount of time used to estimate time from VSYNC event to when
// visible light can be noticed by the user.
const int kPresentationDelayMs = 18;

}  // namespace

FastInkPointerController::FastInkPointerController()
    : presentation_delay_(base::Milliseconds(kPresentationDelayMs)) {
  auto* local_state = Shell::Get()->local_state();
  // |local_state| could be null in tests.
  if (!local_state)
    return;

  pref_change_registrar_local_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_local_->Init(local_state);
  pref_change_registrar_local_->Add(
      prefs::kHasSeenStylus,
      base::BindRepeating(&FastInkPointerController::OnHasSeenStylusPrefChanged,
                          base::Unretained(this)));

  OnHasSeenStylusPrefChanged();
}

FastInkPointerController::~FastInkPointerController() {}

void FastInkPointerController::SetEnabled(bool enabled) {
  enabled_ = enabled;
  // Not calling DestroyPointerView when disabling, leaving the control over
  // the pointer view lifetime to the specific controller implementation.
  // For instance, a controller might prefer to keep the pointer view around
  // while it is being animated away.
}

bool FastInkPointerController::CanStartNewGesture(ui::LocatedEvent* event) {
  // If the input device changes (e.g., from mouse to touch), we treat it as a
  // new gesture. This ensures that if the previous gesture's view is being kept
  // alive (e.g. fading out), it gets reset and reused for the new gesture.
  bool is_touch_event = event->IsTouchEvent();
  if (pointer_view_created_by_touch_.has_value() &&
      pointer_view_created_by_touch_.value() != is_touch_event) {
    return true;
  }

  // 1) The stylus/finger is pressed.
  // 2) The stylus/finger is moving, but the pointer session has not started yet
  // (most likely because the preceding press event was consumed by another
  // handler).
  bool can_start_on_touch_event =
      event->type() == ui::EventType::kTouchPressed ||
      (event->type() == ui::EventType::kTouchMoved && !GetPointerView());
  if (can_start_on_touch_event)
    return true;

  // 1) The mouse is pressed.
  // 2) The mouse is moving, but the pointer session has not started yet
  // (most likely because the preceding press event was consumed by another
  // handler).
  bool can_start_on_mouse_event =
      event->type() == ui::EventType::kMousePressed ||
      (event->type() == ui::EventType::kMouseMoved && !GetPointerView());
  if (can_start_on_mouse_event)
    return true;

  return false;
}

bool FastInkPointerController::ShouldProcessEvent(ui::LocatedEvent* event) {
  return event->type() == ui::EventType::kTouchReleased ||
         event->type() == ui::EventType::kTouchMoved ||
         event->type() == ui::EventType::kTouchPressed ||
         event->type() == ui::EventType::kTouchCancelled ||
         event->type() == ui::EventType::kMousePressed ||
         event->type() == ui::EventType::kMouseReleased ||
         event->type() == ui::EventType::kMouseMoved;
}

bool FastInkPointerController::IsPointerInExcludedRegion(
    ui::LocatedEvent* event) const {
  return false;
}

bool FastInkPointerController::IsEnabledForMouseEvent() const {
  return !has_seen_stylus_ && !pointer_view_created_by_touch_.value_or(false);
}

bool FastInkPointerController::MaybeCreatePointerView(
    ui::LocatedEvent* event,
    bool can_start_new_gesture) {
  aura::Window* root_window =
      static_cast<aura::Window*>(event->target())->GetRootWindow();
  views::View* pointer_view = GetPointerView();
  const bool has_valid_view =
      pointer_view && !pointer_view->GetWidget()->IsClosed() &&
      pointer_view->GetWidget()->GetNativeWindow()->GetRootWindow() ==
          root_window;

  if (can_start_new_gesture) {
    if (!has_valid_view) {
      DestroyPointerView();
      CreatePointerView(presentation_delay_, root_window);
    } else {
      ResetPointerView();
    }
    pointer_view_created_by_touch_ = event->IsTouchEvent();
  } else {
    if (!has_valid_view) {
      // The pointer widget is no longer valid, end the current pointer session.
      DestroyPointerView();
      return false;
    }
  }

  return true;
}

void FastInkPointerController::OnTouchEvent(ui::TouchEvent* event) {
  const int touch_id = event->pointer_details().id;
  // Keep track of touch point count.
  if (event->type() == ui::EventType::kTouchPressed) {
    touch_ids_.insert(touch_id);
  }
  if (event->type() == ui::EventType::kTouchReleased ||
      event->type() == ui::EventType::kTouchCancelled) {
    auto iter = touch_ids_.find(touch_id);

    // Can happen if this object is constructed while fingers were down.
    if (iter == touch_ids_.end())
      return;

    touch_ids_.erase(touch_id);
  }

  if (!enabled_)
    return;

  // Disable on touch events if the device has stylus.
  if (has_seen_stylus_ &&
      event->pointer_details().pointer_type != ui::EventPointerType::kPen) {
    return;
  }

  // Disable for multiple fingers touch.
  if (touch_ids_.size() > 1) {
    DestroyPointerView();
    return;
  }

  if (!ShouldProcessEvent(event))
    return;

  // End any ongoing gesture and let the event through so that the UI in the
  // excluded region can handle it.
  if (IsPointerInExcludedRegion(event)) {
    DestroyPointerView();
    return;
  }

  // Update pointer view and stop event propagation if pointer view is
  // available.
  if (MaybeCreatePointerView(event, CanStartNewGesture(event))) {
    UpdatePointerView(event);
    event->StopPropagation();
  }
}

void FastInkPointerController::OnMouseEvent(ui::MouseEvent* event) {
  if (!enabled_ || !IsEnabledForMouseEvent() || !ShouldProcessEvent(event))
    return;

  // End any ongoing gesture and let the event through so that the UI in the
  // excluded region can handle it.
  if (IsPointerInExcludedRegion(event)) {
    DestroyPointerView();
    return;
  }

  // Update pointer view and stop event propagation if pointer view is
  // available.
  if (MaybeCreatePointerView(event, CanStartNewGesture(event))) {
    UpdatePointerView(event);
    event->StopPropagation();
  }
}

void FastInkPointerController::OnHasSeenStylusPrefChanged() {
  auto* local_state = pref_change_registrar_local_->prefs();
  has_seen_stylus_ =
      local_state && local_state->GetBoolean(prefs::kHasSeenStylus);
}

void FastInkPointerController::DestroyPointerView() {
  pointer_view_created_by_touch_.reset();
}

void FastInkPointerController::ResetPointerView() {
  pointer_view_created_by_touch_.reset();
}

}  // namespace ash
