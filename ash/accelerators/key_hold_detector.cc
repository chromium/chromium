// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/key_hold_detector.h"

#include <tuple>
#include <utility>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_sink.h"

namespace ash {
namespace {

void DispatchPressedEvent(const ui::KeyEvent& key_event,
                          std::unique_ptr<aura::WindowTracker> tracker) {
  // The target window may be gone.
  if (tracker->windows().empty())
    return;
  ui::KeyEvent event(key_event);
  aura::Window* target = *(tracker->windows().begin());
  std::ignore = target->GetHost()->GetEventSink()->OnEventFromSource(&event);
}

void PostPressedEvent(ui::KeyEvent* event) {
  // Modify RELEASED event to PRESSED event.
  const ui::KeyEvent pressed_event(
      ui::EventType::kKeyPressed, event->key_code(), event->code(),
      event->flags() | ui::EF_SHIFT_DOWN | ui::EF_IS_SYNTHESIZED);
  std::unique_ptr<aura::WindowTracker> tracker(new aura::WindowTracker);
  tracker->Add(static_cast<aura::Window*>(event->target()));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DispatchPressedEvent, pressed_event, std::move(tracker)));
}

}  // namespace

KeyHoldDetector::KeyHoldDetector(std::unique_ptr<Delegate> delegate)
    : state_(INITIAL), delegate_(std::move(delegate)) {}

KeyHoldDetector::~KeyHoldDetector() = default;

void KeyHoldDetector::OnKeyEvent(ui::KeyEvent* event) {
  if (!delegate_->ShouldProcessEvent(event))
    return;

  if (delegate_->IsStartEvent(event)) {
    switch (state_) {
      case INITIAL:
        // Pass through posted event.
        if (event->flags() & ui::EF_IS_SYNTHESIZED) {
          event->SetFlags(event->flags() & ~ui::EF_IS_SYNTHESIZED);
          return;
        }
        state_ = PRESSED;
        if (delegate_->ShouldStopEventPropagation()) {
          // Don't process EventType::kKeyPressed event yet. The
          // EventType::kKeyPressed event will be generated upon
          // EventType::kKeyReleaseed event below.
          event->StopPropagation();
        }
        break;
      case PRESSED:
        state_ = HOLD;
        [[fallthrough]];
      case HOLD:
        delegate_->OnKeyHold(event);
        if (delegate_->ShouldStopEventPropagation())
          event->StopPropagation();
        break;
    }
  } else if (event->type() == ui::EventType::kKeyReleased) {
    switch (state_) {
      case INITIAL:
        break;
      case PRESSED: {
        if (delegate_->ShouldStopEventPropagation()) {
          PostPressedEvent(event);
          event->StopPropagation();
        }
        break;
      }
      case HOLD: {
        delegate_->OnKeyUnhold(event);
        if (delegate_->ShouldStopEventPropagation())
          event->StopPropagation();
        break;
      }
    }
    state_ = INITIAL;
  }
}

}  // namespace ash
