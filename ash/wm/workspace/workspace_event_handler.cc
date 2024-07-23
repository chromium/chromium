// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"

namespace ash {

WorkspaceEventHandler::WorkspaceEventHandler(aura::Window* workspace_window)
    : workspace_window_(workspace_window), click_component_(HTNOWHERE) {
  workspace_window_->AddPreTargetHandler(this);

  if (workspace_window->GetId() != kShellWindowId_FloatContainer) {
    multi_window_resize_controller_ =
        std::make_unique<MultiWindowResizeController>();
  }
}

WorkspaceEventHandler::~WorkspaceEventHandler() {
  workspace_window_->RemovePreTargetHandler(this);
}

void WorkspaceEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (event->type() == ui::EventType::kMousePressed &&
      event->IsOnlyLeftMouseButton() &&
      ((event->flags() & (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK)) ==
       0)) {
    click_component_ =
        window_util::GetNonClientComponent(target, event->location());
  }

  if (event->handled())
    return;

  switch (event->type()) {
    case ui::EventType::kMouseMoved: {
      if (multi_window_resize_controller_) {
        const int component =
            window_util::GetNonClientComponent(target, event->location());
        multi_window_resize_controller_->Show(target, component,
                                              event->location());
      }
      break;
    }
    case ui::EventType::kMouseEntered:
      break;
    case ui::EventType::kMouseCaptureChanged:
    case ui::EventType::kMouseExited:
      break;
    case ui::EventType::kMousePressed: {
      WindowState* target_state = WindowState::Get(target->GetToplevelWindow());
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

      HandleResizeDoubleClick(target_state, event);
      break;
    }
    default:
      break;
  }
}

void WorkspaceEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (event->handled() || event->type() != ui::EventType::kGestureTap) {
    return;
  }

  aura::Window* target = static_cast<aura::Window*>(event->target());
  int previous_target_component = click_component_;
  click_component_ =
      window_util::GetNonClientComponent(target, event->location());

  if (click_component_ != HTCAPTION)
    return;

  if (event->details().tap_count() != 2)
    return;

  if (click_component_ == previous_target_component) {
    base::RecordAction(
        base::UserMetricsAction("Caption_GestureTogglesMaximize"));
    const WMEvent wm_event(WM_EVENT_TOGGLE_MAXIMIZE_CAPTION);
    WindowState::Get(target)->OnWMEvent(&wm_event);
    event->StopPropagation();
  }
  click_component_ = HTNOWHERE;
}

void WorkspaceEventHandler::HandleResizeDoubleClick(WindowState* target_state,
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

      // If overview is in session, meaning that |target| is a clamshell split
      // view window, then end overview (thereby ending clamshell split view).
      OverviewController* overview_controller =
          Shell::Get()->overview_controller();
      if (overview_controller->InOverviewSession()) {
        DCHECK(SplitViewController::Get(target)->InClamshellSplitViewMode());
        DCHECK(SplitViewController::Get(target)->IsWindowInSplitView(target));
        // For |target| to have a snapped window state (in split view or not),
        // it must have no maximum size (see |WindowState::CanSnap|). That is
        // important here because when |target| has a maximum width, the
        // |WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE| event will do nothing, meaning
        // it would be rather inappropriate to end overview as below, and of
        // course it would be blatantly inappropriate to make the following call
        // to |OverviewSession::SetWindowListNotAnimatedWhenExiting|.
        DCHECK_EQ(gfx::Size(), target->delegate()->GetMaximumSize());
        overview_controller->overview_session()
            ->SetWindowListNotAnimatedWhenExiting(target->GetRootWindow());
        overview_controller->EndOverview(OverviewEndAction::kSplitView);
      }

      const WMEvent wm_event(WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE);
      target_state->OnWMEvent(&wm_event);
      event->StopPropagation();
    }
  }
}

}  // namespace ash
