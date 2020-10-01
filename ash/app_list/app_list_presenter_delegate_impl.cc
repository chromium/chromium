// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_delegate_impl.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Whether the shelf is oriented on the side, not on the bottom.
bool IsSideShelf(Shelf* shelf) {
  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return false;
    case ShelfAlignment::kLeft:
    case ShelfAlignment::kRight:
      return true;
  }
  return false;
}

// Whether the shelf background type indicates that shelf has rounded corners.
bool IsShelfBackgroundTypeWithRoundedCorners(
    ShelfBackgroundType background_type) {
  switch (background_type) {
    case ShelfBackgroundType::kDefaultBg:
    case ShelfBackgroundType::kAppList:
    case ShelfBackgroundType::kOverview:
      return true;
    case ShelfBackgroundType::kMaximized:
    case ShelfBackgroundType::kMaximizedWithAppList:
    case ShelfBackgroundType::kOobe:
    case ShelfBackgroundType::kHomeLauncher:
    case ShelfBackgroundType::kLogin:
    case ShelfBackgroundType::kLoginNonBlurredWallpaper:
    case ShelfBackgroundType::kInApp:
      return false;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, public:

AppListPresenterDelegateImpl::AppListPresenterDelegateImpl(
    AppListControllerImpl* controller)
    : controller_(controller) {
  display_observer_.Add(display::Screen::GetScreen());
}

AppListPresenterDelegateImpl::~AppListPresenterDelegateImpl() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void AppListPresenterDelegateImpl::SetPresenter(
    AppListPresenterImpl* presenter) {
  presenter_ = presenter;
}

void AppListPresenterDelegateImpl::Init(AppListView* view, int64_t display_id) {
  view_ = view;
  view->InitView(controller_->GetContainerForDisplayId(display_id));

  // By setting us as DnD recipient, the app list knows that we can
  // handle items.
  Shelf* shelf = Shelf::ForWindow(Shell::GetRootWindowForDisplayId(display_id));
  view->SetDragAndDropHostOfCurrentAppList(
      shelf->shelf_widget()->GetDragAndDropHostForAppList());
}

void AppListPresenterDelegateImpl::ShowForDisplay(int64_t display_id) {
  is_visible_ = true;

  controller_->UpdateLauncherContainer(display_id);

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shell::GetPrimaryRootWindowController()
      ->GetShelfLayoutManager()
      ->UpdateAutoHideState();

  Shelf* shelf =
      Shelf::ForWindow(view_->GetWidget()->GetNativeView()->GetRootWindow());
  if (!shelf_observer_.IsObserving(shelf))
    shelf_observer_.Add(shelf);

  view_->SetShelfHasRoundedCorners(
      IsShelfBackgroundTypeWithRoundedCorners(shelf->GetBackgroundType()));
  view_->Show(IsSideShelf(shelf));

  SnapAppListBoundsToDisplayEdge();

  Shell::Get()->AddPreTargetHandler(this);
  controller_->ViewShown(display_id);
}

void AppListPresenterDelegateImpl::OnClosing() {
  DCHECK(is_visible_);
  DCHECK(view_);
  is_visible_ = false;
  Shell::Get()->RemovePreTargetHandler(this);
  controller_->ViewClosing();
}

void AppListPresenterDelegateImpl::OnClosed() {
  if (!is_visible_)
    shelf_observer_.RemoveAll();
  controller_->ViewClosed();
}

bool AppListPresenterDelegateImpl::IsTabletMode() const {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

AppListViewDelegate* AppListPresenterDelegateImpl::GetAppListViewDelegate() {
  return controller_;
}

bool AppListPresenterDelegateImpl::GetOnScreenKeyboardShown() {
  return controller_->onscreen_keyboard_shown();
}

aura::Window* AppListPresenterDelegateImpl::GetContainerForWindow(
    aura::Window* window) {
  return ash::GetContainerForWindow(window);
}

aura::Window* AppListPresenterDelegateImpl::GetRootWindowForDisplayId(
    int64_t display_id) {
  return Shell::Get()->GetRootWindowForDisplayId(display_id);
}

void AppListPresenterDelegateImpl::OnVisibilityChanged(bool visible,
                                                       int64_t display_id) {
  controller_->OnVisibilityChanged(visible, display_id);
}

void AppListPresenterDelegateImpl::OnVisibilityWillChange(bool visible,
                                                          int64_t display_id) {
  controller_->OnVisibilityWillChange(visible, display_id);
}

bool AppListPresenterDelegateImpl::IsVisible(
    const base::Optional<int64_t>& display_id) {
  return controller_->IsVisible(display_id);
}

void AppListPresenterDelegateImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!presenter_->GetWindow())
    return;

  view_->OnParentWindowBoundsChanged();
  SnapAppListBoundsToDisplayEdge();
}

void AppListPresenterDelegateImpl::OnBackgroundTypeChanged(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  view_->SetShelfHasRoundedCorners(
      IsShelfBackgroundTypeWithRoundedCorners(background_type));
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, private:

void AppListPresenterDelegateImpl::ProcessLocatedEvent(
    ui::LocatedEvent* event) {
  if (!view_ || !is_visible_)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!target)
    return;
  // If the event happened on a menu, then the event should not close the app
  // list.
  RootWindowController* root_controller =
      RootWindowController::ForWindow(target);
  if (root_controller) {
    aura::Window* menu_container =
        root_controller->GetContainer(kShellWindowId_MenuContainer);
    if (menu_container->Contains(target))
      return;
    aura::Window* keyboard_container =
        root_controller->GetContainer(kShellWindowId_VirtualKeyboardContainer);
    if (keyboard_container->Contains(target))
      return;
  }

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
  if (!window->Contains(target) && !presenter_->HandleCloseOpenFolder() &&
      !switches::ShouldNotDismissOnBlur() && !IsTabletMode()) {
    // Do not dismiss the app list if the event is targeting shelf area
    // containing app icons.
    if (target == shelf->hotseat_widget()->GetNativeWindow() &&
        shelf->hotseat_widget()->EventTargetsShelfView(*event)) {
      return;
    }

    // Don't dismiss the auto-hide shelf if event happened in status area. Then
    // the event can still be propagated.
    base::Optional<Shelf::ScopedAutoHideLock> auto_hide_lock;
    const aura::Window* status_window =
        shelf->shelf_widget()->status_area_widget()->GetNativeWindow();
    if (status_window && status_window->Contains(target))
      auto_hide_lock.emplace(shelf);

    // Record the current AppListViewState to be used later for metrics. The
    // AppListViewState will change on app launch, so this will record the
    // AppListViewState before the app was launched.
    controller_->RecordAppListState();
    presenter_->Dismiss(event->time_stamp());
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, aura::EventFilter implementation:

void AppListPresenterDelegateImpl::OnMouseEvent(ui::MouseEvent* event) {
  // Moving the mouse shouldn't hide focus rings.
  if (event->IsAnyButton())
    controller_->SetKeyboardTraversalMode(false);

  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(event);
}

void AppListPresenterDelegateImpl::OnGestureEvent(ui::GestureEvent* event) {
  controller_->SetKeyboardTraversalMode(false);

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TWO_FINGER_TAP ||
      event->type() == ui::ET_GESTURE_LONG_PRESS) {
    ProcessLocatedEvent(event);
  }
}

void AppListPresenterDelegateImpl::OnKeyEvent(ui::KeyEvent* event) {
  // If keyboard traversal is already engaged, no-op.
  if (controller_->KeyboardTraversalEngaged())
    return;

  // If the home launcher is not shown in tablet mode, ignore events.
  if (IsTabletMode() && !IsVisible(base::nullopt))
    return;

  // Don't absorb the first event for the search box while it is open
  if (view_->search_box_view()->is_search_box_active())
    return;

  // Don't absorb the first event when showing Assistant.
  if (view_->IsShowingEmbeddedAssistantUI())
    return;

  // Don't absorb the first event when renaming folder.
  if (view_->IsFolderBeingRenamed())
    return;

  // Arrow keys or Tab will engage the traversal mode.
  if ((IsUnhandledArrowKeyEvent(*event) || event->key_code() == ui::VKEY_TAB)) {
    // Handle the first arrow key event to just show the focus rings.
    event->SetHandled();
    controller_->SetKeyboardTraversalMode(true);
  }
}

void AppListPresenterDelegateImpl::SnapAppListBoundsToDisplayEdge() {
  CHECK(view_ && view_->GetWidget());
  aura::Window* window = view_->GetWidget()->GetNativeView();
  const gfx::Rect bounds =
      controller_->SnapBoundsToDisplayEdge(window->bounds());
  window->SetBounds(bounds);
}

}  // namespace ash
