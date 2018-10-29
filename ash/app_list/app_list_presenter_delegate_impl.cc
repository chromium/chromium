// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_delegate_impl.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/presenter/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Whether the shelf is oriented on the side, not on the bottom.
bool IsSideShelf(aura::Window* root_window) {
  Shelf* shelf = Shelf::ForWindow(root_window);
  switch (shelf->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return false;
    case SHELF_ALIGNMENT_LEFT:
    case SHELF_ALIGNMENT_RIGHT:
      return true;
  }
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, public:

AppListPresenterDelegateImpl::AppListPresenterDelegateImpl(
    AppListControllerImpl* controller)
    : controller_(controller), display_observer_(this) {
  display_observer_.Add(display::Screen::GetScreen());
}

AppListPresenterDelegateImpl::~AppListPresenterDelegateImpl() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void AppListPresenterDelegateImpl::SetPresenter(
    app_list::AppListPresenterImpl* presenter) {
  presenter_ = presenter;
}

void AppListPresenterDelegateImpl::Init(app_list::AppListView* view,
                                        int64_t display_id,
                                        int current_apps_page) {
  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shell::GetPrimaryRootWindowController()
      ->GetShelfLayoutManager()
      ->UpdateAutoHideState();
  view_ = view;
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);

  app_list::AppListView::InitParams params;
  aura::Window* parent_window =
      RootWindowController::ForWindow(root_window)
          ->GetContainer(IsHomeLauncherEnabledInTabletMode()
                             ? kShellWindowId_AppListTabletModeContainer
                             : kShellWindowId_AppListContainer);

  // Snap the window bounds to fit the screen size (See
  // https://crbug.com/884889).
  const gfx::Rect bounds = ash::screen_util::SnapBoundsToDisplayEdge(
      parent_window->GetBoundsInScreen(), parent_window);
  parent_window->SetBoundsInScreen(
      bounds, Shell::Get()->display_manager()->GetDisplayForId(display_id));
  params.parent = parent_window;
  params.initial_apps_page = current_apps_page;
  params.is_tablet_mode = Shell::Get()
                              ->tablet_mode_controller()
                              ->IsTabletModeWindowManagerEnabled();
  params.is_side_shelf = IsSideShelf(root_window);

  view->Initialize(params);

  wm::GetWindowState(view->GetWidget()->GetNativeWindow())
      ->set_ignored_by_shelf(true);
  Shell::Get()->AddPreTargetHandler(this);

  // By setting us as DnD recipient, the app list knows that we can
  // handle items.
  Shelf* shelf = Shelf::ForWindow(root_window);
  view->SetDragAndDropHostOfCurrentAppList(
      shelf->shelf_widget()->GetDragAndDropHostForAppList());
}

void AppListPresenterDelegateImpl::OnShown(int64_t display_id) {
  is_visible_ = true;
  controller_->ViewShown(display_id);
}

void AppListPresenterDelegateImpl::OnClosing() {
  DCHECK(is_visible_);
  DCHECK(view_);
  is_visible_ = false;
  controller_->ViewClosing();
}

void AppListPresenterDelegateImpl::OnClosed() {
  controller_->ViewClosed();
}

gfx::Vector2d AppListPresenterDelegateImpl::GetVisibilityAnimationOffset(
    aura::Window* root_window) {
  DCHECK(Shell::HasInstance());

  Shelf* shelf = Shelf::ForWindow(root_window);

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  int app_list_y = view_->GetBoundsInScreen().y();
  return gfx::Vector2d(0, IsSideShelf(root_window)
                              ? 0
                              : shelf->GetIdealBounds().y() - app_list_y);
}

base::TimeDelta AppListPresenterDelegateImpl::GetVisibilityAnimationDuration(
    aura::Window* root_window,
    bool is_visible) {
  // If the view is below the shelf, just hide immediately.
  if (view_->GetBoundsInScreen().y() >
      Shelf::ForWindow(root_window)->GetIdealBounds().y()) {
    return base::TimeDelta::FromMilliseconds(0);
  }
  return GetAnimationDurationFullscreen(IsSideShelf(root_window),
                                        view_->is_fullscreen());
}

bool AppListPresenterDelegateImpl::IsHomeLauncherEnabledInTabletMode() {
  return controller_->IsHomeLauncherEnabledInTabletMode();
}

app_list::AppListViewDelegate*
AppListPresenterDelegateImpl::GetAppListViewDelegate() {
  return controller_;
}

bool AppListPresenterDelegateImpl::GetOnScreenKeyboardShown() {
  return controller_->onscreen_keyboard_shown();
}

aura::Window* AppListPresenterDelegateImpl::GetRootWindowForDisplayId(
    int64_t display_id) {
  return ash::Shell::Get()->GetRootWindowForDisplayId(display_id);
}

void AppListPresenterDelegateImpl::OnVisibilityChanged(
    bool visible,
    aura::Window* root_window) {
  // Notify Chrome the visibility change.
  controller_->OnVisibilityChanged(visible);
  // Notify Ash the visibility change
  ash::Shell::Get()->NotifyAppListVisibilityChanged(visible, root_window);
}

void AppListPresenterDelegateImpl::OnTargetVisibilityChanged(bool visible) {
  // Notify Chrome the target visibility change.
  controller_->OnTargetVisibilityChanged(visible);
}

void AppListPresenterDelegateImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!presenter_->GetWindow())
    return;

  // Snap the window bounds to fit the screen size (See
  // https://crbug.com/884889).
  aura::Window* parent_window = presenter_->GetWindow()->parent();
  const gfx::Rect bounds = ash::screen_util::SnapBoundsToDisplayEdge(
      parent_window->GetBoundsInScreen(), parent_window);
  parent_window->SetBoundsInScreen(bounds, display);
  view_->OnParentWindowBoundsChanged();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, private:

void AppListPresenterDelegateImpl::ProcessLocatedEvent(
    ui::LocatedEvent* event) {
  if (!view_ || !is_visible_)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (target) {
    // If the event happened on a menu, then the event should not close the app
    // list.
    RootWindowController* root_controller =
        RootWindowController::ForWindow(target);
    if (root_controller) {
      aura::Window* menu_container =
          root_controller->GetContainer(kShellWindowId_MenuContainer);
      if (menu_container->Contains(target))
        return;
      aura::Window* keyboard_container = root_controller->GetContainer(
          kShellWindowId_VirtualKeyboardContainer);
      if (keyboard_container->Contains(target))
        return;
    }

    // If the event happened on the app list button, it'll get handled by the
    // button.
    AppListButton* app_list_button =
        Shelf::ForWindow(target)->shelf_widget()->GetAppListButton();
    if (app_list_button && app_list_button->GetWidget() &&
        target == app_list_button->GetWidget()->GetNativeWindow() &&
        app_list_button->bounds().Contains(event->location())) {
      return;
    }

    // If the event happened on the back button, it'll get handled by the
    // button.
    BackButton* back_button =
        Shelf::ForWindow(target)->shelf_widget()->GetBackButton();
    if (back_button && back_button->GetWidget() &&
        target == back_button->GetWidget()->GetNativeWindow() &&
        back_button->bounds().Contains(event->location())) {
      return;
    }
  }

  aura::Window* window = view_->GetWidget()->GetNativeView()->parent();
  if (!window->Contains(target) && !presenter_->CloseOpenedPage() &&
      !app_list::switches::ShouldNotDismissOnBlur() &&
      !IsHomeLauncherEnabledInTabletMode()) {
    presenter_->Dismiss(event->time_stamp());
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, aura::EventFilter implementation:

void AppListPresenterDelegateImpl::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(event);
}

void AppListPresenterDelegateImpl::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TWO_FINGER_TAP ||
      event->type() == ui::ET_GESTURE_LONG_PRESS) {
    ProcessLocatedEvent(event);
  }
}

}  // namespace ash
