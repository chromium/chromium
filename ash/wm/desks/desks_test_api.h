// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_TEST_API_H_
#define ASH_WM_DESKS_DESKS_TEST_API_H_

#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desk_bar_view_base.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class LabelButton;
class MenuItemView;
class ScrollView;
class View;
}  // namespace views

namespace ui {
class LayerTreeOwner;
class SimpleMenuModel;
}  // namespace ui

namespace ash {

class Desk;
class DeskMiniView;
class ScrollArrowButton;

// Helper class used by tests to access desks' internal elements. Including
// elements of multiple different objects of desks. E.g, OverviewDeskBarView,
// Desk.
class DesksTestApi {
 public:
  // Don't instantiate, just use the static helpers below.
  DesksTestApi() = delete;

  // Getters for elements inside the desks.
  static ScrollArrowButton* GetDeskBarLeftScrollButton(
      DeskBarViewBase::Type type);
  static ScrollArrowButton* GetDeskBarRightScrollButton(
      DeskBarViewBase::Type type);
  static views::ScrollView* GetDeskBarScrollView(DeskBarViewBase::Type type);
  static const DeskMiniView* GetDeskBarDragView(DeskBarViewBase::Type type);
  static views::LabelButton* GetCloseAllUndoToastDismissButton();
  static views::View* GetHighlightOverlayForDeskPreview(
      DeskBarViewBase::Type type,
      int index);
  static ui::LayerTreeOwner* GetMirroredContentsLayerTreeForRootAndDesk(
      aura::Window* root,
      Desk* desk);
  static views::Label* GetDeskShortcutLabel(DeskMiniView* mini_view);
  static bool IsDeskShortcutViewVisible(DeskMiniView* mini_view);
  static DeskProfilesButton* GetDeskProfileButton(DeskMiniView* mini_view);
  static bool HasVerticalDotsButton();
  static bool DesksControllerHasDesk(Desk* desk);
  static bool DesksControllerCanUndoDeskRemoval();

  static bool IsDeskBarLeftGradientVisible(DeskBarViewBase::Type type);
  static bool IsDeskBarRightGradientVisible(DeskBarViewBase::Type type);

  // Resets `first_day_visited_` and `last_day_visited_` of `desk` for testing
  // to the current date.
  static void ResetDeskVisitedMetrics(Desk* desk);

  // Waits for `desk_bar_view` to finish its UI update.
  static void WaitForDeskBarUiUpdate(DeskBarViewBase* desk_bar_view);

  // Invoke `done` when `desk_bar_view` finishes its UI updates.
  static void SetDeskBarUiUpdateCallback(DeskBarViewBase* desk_bar_view,
                                         base::OnceClosure done);

  // Desk context menu related. `GetContextMenuForDesk()` and
  // `GetContextMenuModelForDesk()` open a context menu.
  static DeskActionContextMenu* GetContextMenuForDesk(
      DeskBarViewBase::Type type,
      int index);
  static const ui::SimpleMenuModel& GetContextMenuModelForDesk(
      DeskBarViewBase::Type type,
      int index);
  static bool IsContextMenuRunningForDesk(DeskBarViewBase::Type type,
                                          int index);
  static views::MenuItemView* GetDeskActionContextMenuItem(
      DeskActionContextMenu* menu,
      int command_id);
  // Opens the context menu associated with the overview desk bar on `root`
  // window and the mini view with `index`. Then return the
  // `views::MenuItemView` associated with `command_id`. Expands the desk bar
  // from zero state if necessary.
  static views::MenuItemView* OpenDeskContextMenuAndGetMenuItem(
      aura::Window* root,
      DeskBarViewBase::Type bar_type,
      size_t index,
      DeskActionContextMenu::CommandId command_id);

  static void MaybeCloseContextMenuForGrid(OverviewGrid* overview_grid);

  static base::TimeDelta GetCloseAllWindowCloseTimeout();
  static base::AutoReset<base::TimeDelta> SetCloseAllWindowCloseTimeout(
      base::TimeDelta interval);
  static base::AutoReset<base::TimeDelta> SetScrollTimeInterval(
      base::TimeDelta interval);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_TEST_API_H_
