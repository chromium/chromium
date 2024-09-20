// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_test_api.h"

#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_button.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_icon_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/run_loop.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

const DeskBarViewBase* GetDeskBarView(DeskBarViewBase::Type type,
                                      aura::Window* root_window) {
  if (type == DeskBarViewBase::Type::kOverview) {
    auto* overview_controller = Shell::Get()->overview_controller();
    CHECK(overview_controller->InOverviewSession());
    return overview_controller->overview_session()
        ->GetGridWithRootWindow(root_window)
        ->desks_bar_view();
  } else {
    auto* desk_bar_controller = DesksController::Get()->desk_bar_controller();
    CHECK(desk_bar_controller);
    return desk_bar_controller->GetDeskBarView(root_window);
  }
}

const DeskBarViewBase* GetDeskBarView(DeskBarViewBase::Type type) {
  return GetDeskBarView(type, Shell::GetPrimaryRootWindow());
}

}  // namespace

// static
ScrollArrowButton* DesksTestApi::GetDeskBarLeftScrollButton(
    DeskBarViewBase::Type type) {
  return GetDeskBarView(type)->left_scroll_button_;
}

// static
ScrollArrowButton* DesksTestApi::GetDeskBarRightScrollButton(
    DeskBarViewBase::Type type) {
  return GetDeskBarView(type)->right_scroll_button_;
}

// static
views::ScrollView* DesksTestApi::GetDeskBarScrollView(
    DeskBarViewBase::Type type) {
  return GetDeskBarView(type)->scroll_view_;
}

// static
const DeskMiniView* DesksTestApi::GetDeskBarDragView(
    DeskBarViewBase::Type type) {
  return GetDeskBarView(type)->drag_view_;
}

// static
views::LabelButton* DesksTestApi::GetCloseAllUndoToastDismissButton() {
  ToastManagerImpl* toast_manager = Shell::Get()->toast_manager();
  return toast_manager->GetCurrentOverlayForTesting()
      ->dismiss_button_for_testing();
}

// static
views::View* DesksTestApi::GetHighlightOverlayForDeskPreview(
    DeskBarViewBase::Type type,
    int index) {
  return GetDeskBarView(type)
      ->mini_views()[index]
      ->desk_preview()
      ->highlight_overlay_;
}

// static
ui::LayerTreeOwner* DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(
    aura::Window* root,
    Desk* desk) {
  auto& mini_views =
      GetDeskBarView(DeskBarViewBase::Type::kOverview, root)->mini_views();
  for (DeskMiniView* mini_view : mini_views) {
    if (mini_view->desk() == desk) {
      return mini_view->desk_preview()
          ->desk_mirrored_contents_layer_tree_owner_.get();
    }
  }
  return nullptr;
}

// static
views::Label* DesksTestApi::GetDeskShortcutLabel(DeskMiniView* mini_view) {
  return mini_view->desk_shortcut_label_;
}

// static
bool DesksTestApi::IsDeskShortcutViewVisible(DeskMiniView* mini_view) {
  // If the mini_view is in the overview desk bar desk_shortcut_view_ will be
  // nullptr.
  return mini_view->desk_shortcut_view_ &&
         mini_view->desk_shortcut_view_->GetVisible();
}

// static
DeskProfilesButton* DesksTestApi::GetDeskProfileButton(
    DeskMiniView* mini_view) {
  return mini_view->desk_profile_button_;
}

// static
bool DesksTestApi::DesksControllerHasDesk(Desk* desk) {
  return DesksController::Get()->HasDesk(desk);
}

// static
bool DesksTestApi::DesksControllerCanUndoDeskRemoval() {
  return DesksController::Get()->temporary_removed_desk_ != nullptr;
}

// static
bool DesksTestApi::IsDeskBarLeftGradientVisible(DeskBarViewBase::Type type) {
  views::View* const scroll_view = GetDeskBarView(type)->scroll_view_;
  if (!scroll_view) {
    return false;
  }
  const auto& gradient_mask =
      GetDeskBarView(type)->scroll_view_->layer()->gradient_mask();
  return !gradient_mask.IsEmpty() &&
         cc::MathUtil::IsWithinEpsilon(gradient_mask.steps()[0].fraction, 0.f);
}

// static
bool DesksTestApi::IsDeskBarRightGradientVisible(DeskBarViewBase::Type type) {
  views::View* const scroll_view = GetDeskBarView(type)->scroll_view_;
  if (!scroll_view) {
    return false;
  }
  const auto& gradient_mask =
      GetDeskBarView(type)->scroll_view_->layer()->gradient_mask();
  return !gradient_mask.IsEmpty() &&
         cc::MathUtil::IsWithinEpsilon(
             gradient_mask.steps()[gradient_mask.step_count() - 1].fraction,
             1.f);
}

// static
void DesksTestApi::ResetDeskVisitedMetrics(Desk* desk) {
  const int current_date = desks_restore_util::GetDaysFromLocalEpoch();
  desk->first_day_visited_ = current_date;
  desk->last_day_visited_ = current_date;
}

// static
void DesksTestApi::WaitForDeskBarUiUpdate(DeskBarViewBase* desk_bar_view) {
  base::RunLoop run_loop;
  desk_bar_view->on_update_ui_closure_for_testing_ = run_loop.QuitClosure();
  run_loop.Run();
}

// static
void DesksTestApi::SetDeskBarUiUpdateCallback(DeskBarViewBase* desk_bar_view,
                                              base::OnceClosure done) {
  desk_bar_view->on_update_ui_closure_for_testing_ = std::move(done);
}

// static
DeskActionContextMenu* DesksTestApi::GetContextMenuForDesk(
    DeskBarViewBase::Type type,
    int index) {
  DeskMiniView* mini_view = GetDeskBarView(type)->mini_views()[index];

  // The context menu is not created until it is opened, so open it first.
  mini_view->OpenContextMenu(ui::MENU_SOURCE_MOUSE);
  return mini_view->context_menu();
}

// static
const ui::SimpleMenuModel& DesksTestApi::GetContextMenuModelForDesk(
    DeskBarViewBase::Type type,
    int index) {
  return GetContextMenuForDesk(type, index)->context_menu_model_;
}

// static
bool DesksTestApi::IsContextMenuRunningForDesk(DeskBarViewBase::Type type,
                                               int index) {
  DeskMiniView* mini_view = GetDeskBarView(type)->mini_views()[index];
  DeskActionContextMenu* menu = mini_view->context_menu();
  if (!menu) {
    return false;
  }
  return menu->context_menu_runner_->IsRunning();
}

// static
views::MenuItemView* DesksTestApi::GetDeskActionContextMenuItem(
    DeskActionContextMenu* menu,
    int command_id) {
  // Verify that the menu is active.
  CHECK(menu->context_menu_runner_);
  return menu->root_menu_item_view_->GetMenuItemByID(command_id);
}

// static
views::MenuItemView* DesksTestApi::OpenDeskContextMenuAndGetMenuItem(
    aura::Window* root,
    DeskBarViewBase::Type bar_type,
    size_t index,
    DeskActionContextMenu::CommandId command_id) {
  ui::test::EventGenerator event_generator(root);
  auto click_on_view = [&event_generator](const views::View* view) {
    event_generator.MoveMouseToInHost(view->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
  };

  auto* bar_view = const_cast<DeskBarViewBase*>(GetDeskBarView(bar_type));
  // Expand the desk bar if necessary.
  if (bar_view->IsZeroState()) {
    // Click the default desk button so it will expand to a `DeskMiniView`.
    auto* default_button = bar_view->default_desk_button();
    CHECK(default_button);
    click_on_view(default_button);

    // Wait for the desk bar to finish animating to the expanded state.
    if (!ui::ScopedAnimationDurationScaleMode::is_zero()) {
      DesksTestApi::WaitForDeskBarUiUpdate(bar_view);
    }

    CHECK(!bar_view->IsZeroState());
  }

  CHECK_LE(index, bar_view->mini_views().size());
  DeskMiniView* mini_view = bar_view->mini_views()[index];

  // If we have previously been using touch controls, open the menu using a long
  // press on the mini view. Otherwise, hover over the mini view to get the
  // action buttons to show, then click the menu button.
  if (!aura::client::GetCursorClient(root)->IsCursorVisible()) {
    // Long press until the context menu opens.
    LongGestureTap(
        mini_view->desk_action_view()->GetBoundsInScreen().CenterPoint(),
        &event_generator);
  } else {
    // Hover over the mini view to show the context menu button.
    event_generator.MoveMouseToInHost(
        mini_view->GetBoundsInScreen().CenterPoint());

    // The menu button container should be visible, along with the button
    // itself.
    CHECK(mini_view->desk_action_view()->GetVisible());
    DeskActionButton* menu_button =
        mini_view->desk_action_view()->context_menu_button();
    CHECK(menu_button);
    CHECK(menu_button->GetVisible());

    // Click the button to open the context menu.
    click_on_view(menu_button);
  }

  DeskActionContextMenu* menu = mini_view->context_menu();
  CHECK(menu);
  return GetDeskActionContextMenuItem(menu, command_id);
}

// static
void DesksTestApi::MaybeCloseContextMenuForGrid(OverviewGrid* overview_grid) {
  for (DeskMiniView* mini_view :
       overview_grid->desks_bar_view()->mini_views()) {
    mini_view->MaybeCloseContextMenu();

    // Closing the menu is asynchronous, so we want to wait until it has
    // actually closed.
    base::RunLoop().RunUntilIdle();
  }
}

// static
base::TimeDelta DesksTestApi::GetCloseAllWindowCloseTimeout() {
  return DesksController::GetCloseAllWindowCloseTimeoutForTest();
}

// static
base::AutoReset<base::TimeDelta> DesksTestApi::SetCloseAllWindowCloseTimeout(
    base::TimeDelta interval) {
  return DesksController::SetCloseAllWindowCloseTimeoutForTest(interval);
}

// static
base::AutoReset<base::TimeDelta> DesksTestApi::SetScrollTimeInterval(
    base::TimeDelta interval) {
  return ScrollArrowButton::SetScrollTimeIntervalForTest(interval);
}

}  // namespace ash
