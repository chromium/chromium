// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_app_window_drag_controller.h"

#include "ash/shell.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "ash/wm/window_state.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The drag delegate for app windows. It not only includes the logic in
// TabletModeWindowDragDelegate, but also has special logic for app windows.
class TabletModeAppWindowDragDelegate : public TabletModeWindowDragDelegate {
 public:
  TabletModeAppWindowDragDelegate() = default;
  ~TabletModeAppWindowDragDelegate() override = default;

 private:
  // TabletModeWindowDragDelegate:
  void PrepareWindowDrag(const gfx::Point& location_in_screen) override {
    wm::GetWindowState(dragged_window_)
        ->CreateDragDetails(location_in_screen, HTCLIENT,
                            ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  }
  void UpdateWindowDrag(const gfx::Point& location_in_screen) override {}
  void EndingWindowDrag(wm::WmToplevelWindowEventHandler::DragResult result,
                        const gfx::Point& location_in_screen) override {
    wm::GetWindowState(dragged_window_)->DeleteDragDetails();
  }
  void EndedWindowDrag(const gfx::Point& location_in_screen) override {}
  void StartFling(const ui::GestureEvent* event) override {
    if (ShouldFlingIntoOverview(event)) {
      DCHECK(Shell::Get()->window_selector_controller()->IsSelecting());
      Shell::Get()->window_selector_controller()->window_selector()->AddItem(
          dragged_window_, /*reposition=*/true, /*animate=*/false);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(TabletModeAppWindowDragDelegate);
};

}  // namespace

TabletModeAppWindowDragController::TabletModeAppWindowDragController()
    : drag_delegate_(std::make_unique<TabletModeAppWindowDragDelegate>()) {
  display::Screen::GetScreen()->AddObserver(this);
}

TabletModeAppWindowDragController::~TabletModeAppWindowDragController() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

bool TabletModeAppWindowDragController::DragWindowFromTop(
    ui::GestureEvent* event) {
  previous_location_in_screen_ =
      drag_delegate_->GetEventLocationInScreen(event);
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN)
    return StartWindowDrag(event);

  if (!drag_delegate_->dragged_window())
    return false;

  if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateWindowDrag(event);
    return true;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_END) {
    EndWindowDrag(event, wm::WmToplevelWindowEventHandler::DragResult::SUCCESS);
    return true;
  }

  if (event->type() == ui::ET_SCROLL_FLING_START) {
    FlingOrSwipe(event);
    return true;
  }

  EndWindowDrag(event, wm::WmToplevelWindowEventHandler::DragResult::REVERT);
  return false;
}

bool TabletModeAppWindowDragController::StartWindowDrag(
    ui::GestureEvent* event) {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      static_cast<aura::Window*>(event->target()));
  if (!widget)
    return false;

  drag_delegate_->StartWindowDrag(
      widget->GetNativeWindow(),
      drag_delegate_->GetEventLocationInScreen(event));
  return true;
}

void TabletModeAppWindowDragController::UpdateWindowDrag(
    ui::GestureEvent* event) {
  // Update the dragged window's tranform during dragging.
  drag_delegate_->ContinueWindowDrag(
      drag_delegate_->GetEventLocationInScreen(event),
      TabletModeWindowDragDelegate::UpdateDraggedWindowType::UPDATE_TRANSFORM);
}

void TabletModeAppWindowDragController::EndWindowDrag(
    ui::GestureEvent* event,
    wm::WmToplevelWindowEventHandler::DragResult result) {
  drag_delegate_->EndWindowDrag(
      result, drag_delegate_->GetEventLocationInScreen(event));
}

void TabletModeAppWindowDragController::FlingOrSwipe(ui::GestureEvent* event) {
  drag_delegate_->FlingOrSwipe(event);
}

void TabletModeAppWindowDragController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!drag_delegate_->dragged_window() || !(metrics & DISPLAY_METRIC_ROTATION))
    return;

  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          drag_delegate_->dragged_window());
  if (display.id() != current_display.id())
    return;

  drag_delegate_->EndWindowDrag(
      wm::WmToplevelWindowEventHandler::DragResult::REVERT,
      previous_location_in_screen_);
}

}  // namespace ash
