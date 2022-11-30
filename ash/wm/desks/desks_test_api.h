// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_TEST_API_H_
#define ASH_WM_DESKS_DESKS_TEST_API_H_

#include <vector>

#include "base/time/clock.h"
#include "third_party/skia/include/core/SkColor.h"

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
class PersistentDesksBarContextMenu;
class PersistentDesksBarDeskButton;
class ScrollArrowButton;

// Helper class used by tests to access desks' internal elements. Including
// elements of multiple different objects of desks. E.g, DesksBarView, Desk,
// PersistentDesksBarView.
class DesksTestApi {
 public:
  // Don't instantiate, just use the static helpers below.
  DesksTestApi() = delete;

  // Getters for elements inside the desks.
  static ScrollArrowButton* GetDesksBarLeftScrollButton();
  static ScrollArrowButton* GetDesksBarRightScrollButton();
  static views::ScrollView* GetDesksBarScrollView();
  static const DeskMiniView* GetDesksBarDragView();
  static PersistentDesksBarContextMenu* GetDesksBarContextMenu();
  static SkColor GetNewDeskButtonBackgroundColor();
  static PersistentDesksBarContextMenu* GetPersistentDesksBarContextMenu();
  static const std::vector<PersistentDesksBarDeskButton*>
  GetPersistentDesksBarDeskButtons();
  static DeskActionContextMenu* GetContextMenuForDesk(int index);
  static views::LabelButton* GetCloseAllUndoToastDismissButton();
  static const ui::SimpleMenuModel& GetContextMenuModelForDesk(int index);
  static views::View* GetHighlightOverlayForDeskPreview(int index);
  static ui::LayerTreeOwner* GetMirroredContentsLayerTreeForRootAndDesk(
      aura::Window* root,
      Desk* desk);
  static bool HasVerticalDotsButton();
  static bool DesksControllerHasDesk(Desk* desk);
  static bool DesksControllerCanUndoDeskRemoval();
  static bool IsContextMenuRunningForDesk(int index);

  static bool IsDesksBarLeftGradientVisible();
  static bool IsDesksBarRightGradientVisible();

  // Resets `first_day_visited_` and `last_day_visited_` of `desk` for testing
  // to the current date.
  static void ResetDeskVisitedMetrics(Desk* desk);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_TEST_API_H_
