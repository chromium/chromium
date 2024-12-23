// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_
#define ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class ScrollView;
}  // namespace views

namespace ash {
// A selection view that allows users to pick which tabs and apps they want to
// move to a new desk. Its main child is a scroll view that contains many
// `TabAppSelectionItemView`'s representing tabs and apps.
// TODO(http://b/361326120): Add the experimental features view.
class ASH_EXPORT TabAppSelectionView : public views::BoxLayoutView {
  METADATA_HEADER(TabAppSelectionView, views::BoxLayoutView)

 public:
  TabAppSelectionView(const base::Token& group_id,
                      base::RepeatingClosure on_item_removed);
  TabAppSelectionView(const TabAppSelectionView&) = delete;
  TabAppSelectionView& operator=(const TabAppSelectionView&) = delete;
  ~TabAppSelectionView() override;

  // Unselects the current selected tab app view if any.
  void ClearSelection();

  void ProcessKeyEvent(ui::KeyEvent* event);

  // Removes the item associated with given `identifier` when corresponding
  // windows or desks are closed.
  void RemoveItemBySystem(std::string_view identifier);

 private:
  class TabAppSelectionItemView;
  class UserFeedbackView;

  FRIEND_TEST_ALL_PREFIXES(CoralPixelDiffTest, CoralSelectorView);
  FRIEND_TEST_ALL_PREFIXES(TabAppSelectionViewTest, CloseSelectorItems);
  FRIEND_TEST_ALL_PREFIXES(TabAppSelectionViewTest, RecordsHistogram);

  // We don't use an enum class to avoid too many explicit casts at callsites.
  enum ViewID : int {
    kTabSubtitleID = 1,
    kAppSubtitleID,
    kCloseButtonID,
    kThumbsUpID,
    kThumbsDownID,
  };

  void AdvanceSelection(bool reverse);

  // Destroys `sender` and destroys subtitles if necessary (`sender` was the
  // last tab or app).
  void OnCloseButtonPressed(TabAppSelectionItemView* sender);

  void RemoveItemView(TabAppSelectionItemView* item_view);

  // Deselects all items except `sender`.
  void OnItemTapped(TabAppSelectionItemView* sender);

  // Unique identifier for the contents of the selection view.
  const base::Token group_id_;

  base::RepeatingClosure on_item_removed_;

  raw_ptr<views::ScrollView> scroll_view_;

  std::vector<raw_ptr<TabAppSelectionItemView>> item_views_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_
