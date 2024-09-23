// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_capture_delegate.h"
#include "ash/drag_drop/drag_drop_tracker.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

void NotifyWindowOfTouchDispatchGestureEnd(aura::Window* window) {
  if (!window->delegate())
    return;
  DispatchGestureEndToWindow(window);

  ui::PointerDetails touch_details(ui::EventPointerType::kTouch,
                                   /*pointer_id=*/0, 1.0f, 1.0f, 1.0f);
  ui::TouchEvent touch_cancel_event(ui::EventType::kTouchCancelled,
                                    gfx::Point(), ui::EventTimeForNow(),
                                    touch_details);
  window->delegate()->OnTouchEvent(&touch_cancel_event);
}

}  // namespace

void DispatchGestureEndToWindow(aura::Window* window) {
  DCHECK(window && window->delegate());
  ui::GestureEventDetails details(ui::EventType::kGestureEnd);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_end(0, 0, 0, ui::EventTimeForNow(), details);
  window->delegate()->OnGestureEvent(&gesture_end);
}

DragDropCaptureDelegate::DragDropCaptureDelegate() = default;
DragDropCaptureDelegate::~DragDropCaptureDelegate() = default;

bool DragDropCaptureDelegate::TakeCapture(
    aura::Window* root_window,
    aura::Window* source_window,
    CancelDragDropCallback callback,
    ui::TransferTouchesBehavior behavior) {
  drag_drop_tracker_.reset(new DragDropTracker(root_window, callback));
  // We need to transfer the current gesture sequence and the GR's touch event
  // queue to the |drag_drop_tracker_|'s capture window so that when it takes
  // capture, it still gets a valid gesture state.
  aura::Window* capture_window = drag_drop_tracker_->capture_window();
  aura::WindowTracker tracker({source_window, capture_window});
  auto* gesture_recognizer = aura::Env::GetInstance()->gesture_recognizer();
  gesture_recognizer->TransferEventsTo(
      source_window, drag_drop_tracker_->capture_window(), behavior);
  if (tracker.Contains(source_window)) {
    // We also send a gesture end and touch cancel to the source window so it
    // can clear state.  TODO(varunjain): Remove this whole block when gesture
    // sequence transferring is properly done in the GR
    // (http://crbug.com/160558)
    NotifyWindowOfTouchDispatchGestureEnd(source_window);
  }
  if (!tracker.Contains(capture_window)) {
    // This means the drag was cancelled during event transfer.
    // See: crbug.com/1297209.
    return false;
  }
  drag_drop_tracker_->TakeCapture();
  return true;
}

aura::Window* DragDropCaptureDelegate::GetTarget(
    const ui::LocatedEvent& event) {
  return drag_drop_tracker_ ? drag_drop_tracker_->GetTarget(event) : nullptr;
}

std::unique_ptr<ui::LocatedEvent> DragDropCaptureDelegate::ConvertEvent(
    aura::Window* target,
    const ui::LocatedEvent& event) {
  return drag_drop_tracker_ ? drag_drop_tracker_->ConvertEvent(target, event)
                            : nullptr;
}

aura::Window* DragDropCaptureDelegate::capture_window() {
  return drag_drop_tracker_ ? drag_drop_tracker_->capture_window() : nullptr;
}

void DragDropCaptureDelegate::ReleaseCapture() {
  drag_drop_tracker_.reset();
}

}  // namespace ash
