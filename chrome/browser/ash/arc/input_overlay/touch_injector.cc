// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"

#include <list>

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"
#include "ui/aura/window.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace arc {
// Calculate the window content bounds (excluding caption if it exists) in the
// root window.
gfx::RectF input_overlay::CalculateWindowContentBounds(aura::Window* window) {
  DCHECK(window);
  auto* widget = views::Widget::GetWidgetForNativeView(window);
  DCHECK(widget->non_client_view());
  auto* frame_view = widget->non_client_view()->frame_view();
  DCHECK(frame_view);
  int height = frame_view->GetBoundsForClientView().y();
  auto bounds = gfx::RectF(window->bounds());
  bounds.Inset(0, height, 0, 0);
  return bounds;
}

TouchInjector::TouchInjector(aura::Window* top_level_window)
    : target_window_(top_level_window) {}

TouchInjector::~TouchInjector() {
  UnRegisterEventRewriter();
}

void TouchInjector::ParseActions(const base::Value& root) {
  auto parsed_actions = ParseJsonToActions(target_window_, root);
  if (!parsed_actions)
    return;
  std::move(parsed_actions->begin(), parsed_actions->end(),
            std::back_inserter(actions_));
}

void TouchInjector::NotifyTextInputState(bool active) {
  if (text_input_active_ != active && active)
    DispatchTouchCancelEvent();
  text_input_active_ = active;
}

void TouchInjector::RegisterEventRewriter() {
  if (observation_.IsObserving())
    return;
  observation_.Observe(target_window_->GetHost()->GetEventSource());
}

void TouchInjector::UnRegisterEventRewriter() {
  if (!observation_.IsObserving())
    return;
  DispatchTouchCancelEvent();
  observation_.Reset();
}

void TouchInjector::DispatchTouchCancelEvent() {
  for (auto& action : actions_) {
    auto cancel = action->GetTouchCancelEvent();
    if (!cancel)
      continue;
    if (SendEventFinally(continuation_, &*cancel).dispatcher_destroyed) {
      VLOG(0) << "Undispatched event due to destroyed dispatcher for canceling "
                 "touch event.";
    }
  }
}

void TouchInjector::SendTouchMoveEvent(
    const ui::EventRewriter::Continuation continuation,
    const ui::TouchEvent& event) {
  if (SendEventFinally(continuation, &event).dispatcher_destroyed) {
    VLOG(0) << "Undispatched event due to destroyed dispatcher for "
               "touch move event.";
  }
}

ui::EventDispatchDetails TouchInjector::RewriteEvent(
    const ui::Event& event,
    const ui::EventRewriter::Continuation continuation) {
  continuation_ = continuation;
  if (text_input_active_)
    return SendEvent(continuation, &event);

  auto bounds = input_overlay::CalculateWindowContentBounds(target_window_);

  std::list<ui::TouchEvent> touch_events;
  for (auto& action : actions_) {
    bool rewritten = action->RewriteEvent(event, touch_events, bounds);
    if (!rewritten)
      continue;
    if (touch_events.empty())
      return DiscardEvent(continuation);
    if (touch_events.size() == 1)
      return SendEventFinally(continuation, &touch_events.front());
    if (touch_events.size() == 2) {
      // Some apps can't process correctly on the touch move event which follows
      // touch press event immediately, so send the touch move event delayed
      // here.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&TouchInjector::SendTouchMoveEvent,
                         weak_ptr_factory_.GetWeakPtr(), continuation,
                         touch_events.back()),
          input_overlay::kSendTouchMoveDelay);
      return SendEventFinally(continuation, &touch_events.front());
    }
  }
  return SendEvent(continuation, &event);
}

}  // namespace arc
