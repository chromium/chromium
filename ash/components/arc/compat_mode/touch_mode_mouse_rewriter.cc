// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/touch_mode_mouse_rewriter.h"

#include <tuple>

#include "ash/components/arc/compat_mode/metrics.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"

namespace arc {

namespace {
// In Android, the default long press threshold is 500ms.
constexpr base::TimeDelta kLongPressInterval = base::Milliseconds(700);

// The scale from MouseWheelEvent's |y_offset| to ScrollEvent's |y_offset|.
// TODO(b/202679170): Verify that this value is working well, as the value was
// chosen tentatively.
constexpr int kWheelToSmoothScrollScale = 3;

// The total time from the first simulated smooth scroll event to the last one.
constexpr base::TimeDelta kSmoothScrollTimeout = base::Milliseconds(500);
// The interval between simulated smooth scroll events.
constexpr base::TimeDelta kSmoothScrollEventInterval = base::Milliseconds(20);
}  // namespace

TouchModeMouseRewriter::TouchModeMouseRewriter() = default;

TouchModeMouseRewriter::~TouchModeMouseRewriter() {
  std::set<aura::WindowTreeHost*> unique_hosts(hosts_.begin(), hosts_.end());
  for (aura::WindowTreeHost* host : unique_hosts)
    host->GetEventSource()->RemoveEventRewriter(this);
}

void TouchModeMouseRewriter::EnableForWindow(aura::Window* window) {
  if (window_observations_.IsObservingSource(window))
    return;
  enabled_windows_.insert(window);
  OnWindowAddedToRootWindow(window);
  window_observations_.AddObservation(window);
}

void TouchModeMouseRewriter::DisableForWindow(aura::Window* window) {
  OnWindowDestroying(window);
}

void TouchModeMouseRewriter::OnWindowDestroying(aura::Window* window) {
  if (!window_observations_.IsObservingSource(window))
    return;
  window_observations_.RemoveObservation(window);
  OnWindowRemovingFromRootWindow(window, nullptr);
  enabled_windows_.erase(window);
}

void TouchModeMouseRewriter::OnWindowAddedToRootWindow(aura::Window* window) {
  aura::WindowTreeHost* host = window->GetHost();
  if (!host)
    return;
  // |hosts_| is a multiset, so if this is the first one, add the EventRewriter
  // to that WindowTreeHost.
  if (hosts_.count(host) == 0)
    host->GetEventSource()->AddEventRewriter(this);
  hosts_.insert(host);
}

void TouchModeMouseRewriter::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  aura::WindowTreeHost* host = window->GetHost();
  if (!host)
    return;
  auto it = hosts_.find(host);
  DCHECK(it != hosts_.end());
  hosts_.erase(it);
  // |hosts_| is a multiset, so if this is the last one, remove the
  // EventRewriter from that WindowTreeHost.
  if (hosts_.count(host) == 0)
    host->GetEventSource()->RemoveEventRewriter(this);
}

ui::EventDispatchDetails TouchModeMouseRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!event.IsMouseEvent())
    return SendEvent(continuation, &event);

  auto* screen = display::Screen::GetScreen();
  aura::Window* target =
      screen->GetWindowAtScreenPoint(screen->GetCursorScreenPoint());
  // Only handle clicks to the content area of an Exo window i.e. exclude the
  // caption bar.
  if (!target || target->GetType() != aura::client::WINDOW_TYPE_CONTROL)
    return SendEvent(continuation, &event);

  const bool in_resize_locked = IsInResizeLockedWindow(target);
  if (event.IsMouseWheelEvent()) {
    if (!in_resize_locked)
      return SendEvent(continuation, &event);

    return RewriteMouseWheelEvent(*event.AsMouseWheelEvent(), continuation);
  }

  const ui::MouseEvent& mouse_event = *event.AsMouseEvent();
  if (mouse_event.IsRightMouseButton() || mouse_event.IsLeftMouseButton()) {
    if (!in_resize_locked) {
      return SendEvent(continuation, &event);
    }

    return RewriteMouseClickEvent(mouse_event, continuation);
  }

  return SendEvent(continuation, &event);
}

void TouchModeMouseRewriter::SendReleaseEvent(
    const ui::MouseEvent& original_event,
    const Continuation continuation) {
  release_event_scheduled_ = false;
  ui::MouseEvent release_event(
      ui::EventType::kMouseReleased, original_event.location(),
      original_event.root_location(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  std::ignore = SendEvent(continuation, &release_event);
}

void TouchModeMouseRewriter::SendScrollEvent(
    const ui::MouseWheelEvent& original_event,
    const Continuation continuation) {
  if (scroll_timeout_.is_zero())
    return;
  const int y_step =
      scroll_y_offset_ * (kSmoothScrollEventInterval / scroll_timeout_);
  const int x_step =
      scroll_x_offset_ * (kSmoothScrollEventInterval / scroll_timeout_);
  ui::ScrollEvent scroll_event(
      ui::EventType::kScroll, original_event.location_f(),
      original_event.root_location_f(), ui::EventTimeForNow(), 0, x_step,
      y_step, x_step, y_step, 1);
  std::ignore = SendEvent(continuation, &scroll_event);
  scroll_y_offset_ -= y_step;
  scroll_x_offset_ -= x_step;
  scroll_timeout_ -= kSmoothScrollEventInterval;
  if (scroll_timeout_.is_zero()) {
    ui::ScrollEvent fling_start_event(ui::EventType::kScrollFlingStart,
                                      original_event.location_f(),
                                      original_event.root_location_f(),
                                      ui::EventTimeForNow(), 0, 0, 0, 0, 0, 0);
    std::ignore = SendEvent(continuation, &fling_start_event);
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TouchModeMouseRewriter::SendScrollEvent,
                       weak_ptr_factory_.GetWeakPtr(), original_event,
                       continuation),
        kSmoothScrollEventInterval);
  }
}

ui::EventDispatchDetails TouchModeMouseRewriter::RewriteMouseWheelEvent(
    const ui::MouseWheelEvent& event,
    const Continuation continuation) {
  const bool started = !scroll_timeout_.is_zero();
  scroll_y_offset_ += kWheelToSmoothScrollScale * event.y_offset();
  scroll_x_offset_ += kWheelToSmoothScrollScale * event.x_offset();
  scroll_timeout_ = kSmoothScrollTimeout;
  if (!started) {
    ui::ScrollEvent fling_cancel_event(
        ui::EventType::kScrollFlingCancel, event.location_f(),
        event.root_location_f(), event.time_stamp(), 0, 0, 0, 0, 0, 0);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TouchModeMouseRewriter::SendScrollEvent,
                       weak_ptr_factory_.GetWeakPtr(), event, continuation));
    return SendEvent(continuation, &fling_cancel_event);
  }
  return DiscardEvent(continuation);
}

ui::EventDispatchDetails TouchModeMouseRewriter::RewriteMouseClickEvent(
    const ui::MouseEvent& event,
    const Continuation continuation) {
  if (event.IsRightMouseButton()) {
    // 1. If there is already an ongoing simulated long press, discard the
    //    subsequent right click.
    // 2. If the left button is currently pressed, discard the right click.
    // 3. Discard events that is not a right press.
    if (release_event_scheduled_ || left_pressed_ ||
        event.type() != ui::EventType::kMousePressed) {
      return DiscardEvent(continuation);
    }
    // Schedule the release event after |kLongPressInterval|.
    release_event_scheduled_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TouchModeMouseRewriter::SendReleaseEvent,
                       weak_ptr_factory_.GetWeakPtr(), event, continuation),
        kLongPressInterval);

    // Send the press event now.
    ui::MouseEvent press_event(
        ui::EventType::kMousePressed, event.location(), event.root_location(),
        event.time_stamp(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    return SendEvent(continuation, &press_event);
  } else if (event.IsLeftMouseButton()) {
    if (event.type() == ui::EventType::kMousePressed) {
      left_pressed_ = true;
    } else if (event.type() == ui::EventType::kMouseReleased) {
      left_pressed_ = false;
    }
    // Discard a release event that corresponds to a previously discarded press
    // event.
    if (discard_next_left_release_ &&
        event.type() == ui::EventType::kMouseReleased) {
      discard_next_left_release_ = false;
      return DiscardEvent(continuation);
    }
    // Discard the left click if there is an ongoing simulated long press.
    if (release_event_scheduled_) {
      if (event.type() == ui::EventType::kMousePressed) {
        discard_next_left_release_ = true;
      }
      return DiscardEvent(continuation);
    }
    return SendEvent(continuation, &event);
  }

  return SendEvent(continuation, &event);
}

bool TouchModeMouseRewriter::IsInResizeLockedWindow(
    const aura::Window* window) const {
  // TODO(b/202679170): Verify that it does not affect performance before
  // flipping the flag, and fix it if necessary.
  while (window != nullptr) {
    if (enabled_windows_.count(window))
      return true;
    window = window->parent();
  }
  return false;
}

}  // namespace arc
