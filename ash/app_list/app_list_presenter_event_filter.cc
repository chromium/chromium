// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_event_filter.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

AppListPresenterEventFilter::AppListPresenterEventFilter(
    AppListControllerImpl* controller,
    AppListPresenterImpl* presenter,
    AppListView* view)
    : controller_(controller), presenter_(presenter), view_(view) {
  DCHECK(controller_);
  DCHECK(presenter_);
  DCHECK(view_);
  Shell::Get()->AddPreTargetHandler(this);
}

AppListPresenterEventFilter::~AppListPresenterEventFilter() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void AppListPresenterEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  // Moving the mouse shouldn't hide focus rings.
  if (event->IsAnyButton())
    controller_->SetKeyboardTraversalMode(false);

  if (event->type() == ui::EventType::kMousePressed) {
    ProcessLocatedEvent(event);
  }
}

void AppListPresenterEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  controller_->SetKeyboardTraversalMode(false);

  // Checks tap types instead of ui::EventType::kTouchPressed so that swipes on
  // the shelf do not close the launcher. https://crbug.com/750274
  if (event->type() == ui::EventType::kGestureTap ||
      event->type() == ui::EventType::kGestureTwoFingerTap ||
      event->type() == ui::EventType::kGestureLongPress) {
    ProcessLocatedEvent(event);
  }
}

void AppListPresenterEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // If keyboard traversal is already engaged, no-op.
  if (controller_->KeyboardTraversalEngaged())
    return;

  // If the home launcher is not shown in tablet mode, ignore events.
  if (Shell::Get()->IsInTabletMode() && !controller_->IsVisible())
    return;

  // Don't absorb the first event for the search box while it is open.
  if (view_->search_box_view()->is_search_box_active())
    return;

  // Don't absorb the first event when renaming folder.
  if (view_->IsFolderBeingRenamed())
    return;

  // Arrow keys or Tab will engage the traversal mode.
  if ((IsUnhandledArrowKeyEvent(*event) || event->key_code() == ui::VKEY_TAB)) {
    // Handle the first arrow key event to just show the focus rings (if not
    // showing Assistant). Don't absorb the first event when showing Assistant.
    if (!view_->IsShowingEmbeddedAssistantUI())
      event->SetHandled();
    controller_->SetKeyboardTraversalMode(true);
  }
}

void AppListPresenterEventFilter::ProcessLocatedEvent(ui::LocatedEvent* event) {
  // Check the general rules for closing bubbles.
  if (!bubble_utils::ShouldCloseBubbleForEvent(*event))
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!target)
    return;

  // If the event happened on the home button's widget, it'll get handled by the
  // button.
  Shelf* shelf = Shelf::ForWindow(target);
  HomeButton* home_button = shelf->navigation_widget()->GetHomeButton();
  if (home_button && home_button->GetWidget() &&
      target == home_button->GetWidget()->GetNativeWindow()) {
    gfx::Point location_in_home_button = event->location();
    views::View::ConvertPointFromWidget(home_button, &location_in_home_button);
    if (home_button->HitTestPoint(location_in_home_button))
      return;
  }

  // If the event happened on the back button, it'll get handled by the
  // button.
  BackButton* back_button = shelf->navigation_widget()->GetBackButton();
  if (back_button && back_button->GetWidget() &&
      target == back_button->GetWidget()->GetNativeWindow()) {
    gfx::Point location_in_back_button = event->location();
    views::View::ConvertPointFromWidget(back_button, &location_in_back_button);
    if (back_button->HitTestPoint(location_in_back_button))
      return;
  }

  aura::Window* window = view_->GetWidget()->GetNativeView()->parent();
  if (window->Contains(target))
    return;

  // Try to close an open folder window: return if an open folder view was
  // closed successfully.
  if (presenter_->HandleCloseOpenFolder())
    return;

  if (!Shell::Get()->IsInTabletMode()) {
    // Don't dismiss the auto-hide shelf if event happened in status area. Then
    // the event can still be propagated.
    const aura::Window* status_window =
        shelf->shelf_widget()->status_area_widget()->GetNativeWindow();
    if (status_window && status_window->Contains(target))
      return;

    // Record the current AppListViewState to be used later for metrics. The
    // AppListViewState will change on app launch, so this will record the
    // AppListViewState before the app was launched.
    controller_->RecordAppListState();
    presenter_->Dismiss(event->time_stamp());
  }
}

}  // namespace ash
