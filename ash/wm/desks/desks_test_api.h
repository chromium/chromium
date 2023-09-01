// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_TEST_API_H_
#define ASH_WM_DESKS_DESKS_TEST_API_H_

#include "ash/wm/desks/desk_bar_view_base.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class LabelButton;
class ScrollView;
class View;
}  // namespace views

namespace ui {
class LayerTreeOwner;
class SimpleMenuModel;
}  // namespace ui

namespace ash {

class Desk;
class DeskActionContextMenu;
class DeskMiniView;
class ScrollArrowButton;

// Helper class used by tests to access desks' internal elements. Including
// elements of multiple different objects of desks. E.g, LegacyDeskBarView,
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
  static DeskActionContextMenu* GetContextMenuForDesk(
      DeskBarViewBase::Type type,
      int index);
  static views::LabelButton* GetCloseAllUndoToastDismissButton();
  static const ui::SimpleMenuModel& GetContextMenuModelForDesk(
      DeskBarViewBase::Type type,
      int index);
  static views::View* GetHighlightOverlayForDeskPreview(
      DeskBarViewBase::Type type,
      int index);
  static ui::LayerTreeOwner* GetMirroredContentsLayerTreeForRootAndDesk(
      aura::Window* root,
      Desk* desk);
  static views::Label* GetDeskShortcutLabel(DeskMiniView* mini_view);
  static bool IsDeskShortcutViewVisible(DeskMiniView* mini_view);
  static bool HasVerticalDotsButton();
  static bool DesksControllerHasDesk(Desk* desk);
  static bool DesksControllerCanUndoDeskRemoval();
  static bool IsContextMenuRunningForDesk(DeskBarViewBase::Type type,
                                          int index);

  static bool IsDeskBarLeftGradientVisible(DeskBarViewBase::Type type);
  static bool IsDeskBarRightGradientVisible(DeskBarViewBase::Type type);

  // Resets `first_day_visited_` and `last_day_visited_` of `desk` for testing
  // to the current date.
  static void ResetDeskVisitedMetrics(Desk* desk);

  // Waits for `desk_bar_view` to finish its UI update.
  static void WaitForDeskBarUiUpdate(DeskBarViewBase* desk_bar_view);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_TEST_API_H_
