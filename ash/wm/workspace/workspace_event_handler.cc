// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler.h"

#include "ash/public/cpp/touch_uma.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"

namespace ash {

WorkspaceEventHandler::WorkspaceEventHandler(aura::Window* workspace_window)
    : workspace_window_(workspace_window), click_component_(HTNOWHERE) {
  workspace_window_->AddPreTargetHandler(this);
}

WorkspaceEventHandler::~WorkspaceEventHandler() {
  workspace_window_->RemovePreTargetHandler(this);
}

void WorkspaceEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (event->type() == ui::ET_MOUSE_PRESSED && event->IsOnlyLeftMouseButton() &&
      ((event->flags() & (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK)) ==
       0)) {
    click_component_ =
        window_util::GetNonClientComponent(target, event->location());
  }

  if (event->handled())
    return;

  switch (event->type()) {
    case ui::ET_MOUSE_MOVED: {
      int component =
          window_util::GetNonClientComponent(target, event->location());
      multi_window_resize_controller_.Show(target, component,
                                           event->location());
      break;
    }
    case ui::ET_MOUSE_ENTERED:
      break;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_EXITED:
      break;
    case ui::ET_MOUSE_PRESSED: {
      WindowState* target_state = WindowState::Get(target);
      // No action for windows that aren't managed by WindowState.
      if (!target_state)
        return;

      if (event->IsOnlyLeftMouseButton()) {
        if (event->flags() & ui::EF_IS_DOUBLE_CLICK) {
          int component =
              window_util::GetNonClientComponent(target, event->location());
          if (component == HTCAPTION && component == click_component_) {
            base::RecordAction(
                base::UserMetricsAction("Caption_ClickTogglesMaximize"));
            const WMEvent wm_event(WM_EVENT_TOGGLE_MAXIMIZE_CAPTION);
            target_state->OnWMEvent(&wm_event);
            event->StopPropagation();
          }
          click_component_ = HTNOWHERE;
        }
      } else {
        click_component_ = HTNOWHERE;
      }

      HandleVerticalResizeDoubleClick(target_state, event);
      break;
    }
    default:
      break;
  }
}

void WorkspaceEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (event->handled() || event->type() != ui::ET_GESTURE_TAP)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  int previous_target_component = click_component_;
  click_component_ =
      window_util::GetNonClientComponent(target, event->location());

  if (click_component_ != HTCAPTION)
    return;

  if (event->details().tap_count() != 2) {
    TouchUMA::RecordGestureAction(GESTURE_FRAMEVIEW_TAP);
    return;
  }

  if (click_component_ == previous_target_component) {
    base::RecordAction(
        base::UserMetricsAction("Caption_GestureTogglesMaximize"));
    TouchUMA::RecordGestureAction(GESTURE_MAXIMIZE_DOUBLETAP);
    const WMEvent wm_event(WM_EVENT_TOGGLE_MAXIMIZE_CAPTION);
    WindowState::Get(target)->OnWMEvent(&wm_event);
    event->StopPropagation();
  }
  click_component_ = HTNOWHERE;
}

void WorkspaceEventHandler::HandleVerticalResizeDoubleClick(
    WindowState* target_state,
    ui::MouseEvent* event) {
  aura::Window* target = target_state->window();
  if ((event->flags() & ui::EF_IS_DOUBLE_CLICK) != 0 && target->delegate()) {
    const int component =
        window_util::GetNonClientComponent(target, event->location());
    if (component == HTBOTTOM || component == HTTOP) {
      base::RecordAction(base::UserMetricsAction(
          "WindowBorder_ClickTogglesSingleAxisMaximize"));
      const WMEvent wm_event(WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE);
      target_state->OnWMEvent(&wm_event);
      event->StopPropagation();
    } else if (component == HTLEFT || component == HTRIGHT) {
      base::RecordAction(base::UserMetricsAction(
          "WindowBorder_ClickTogglesSingleAxisMaximize"));
      const WMEvent wm_event(WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE);
      target_state->OnWMEvent(&wm_event);
      event->StopPropagation();
    }
  }
}

}  // namespace ash
