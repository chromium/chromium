// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_TILES_CONTAINER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_TILES_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace ash {

class FeatureTile;
class PaginationModel;
class UnifiedSystemTrayController;

// Container of FeatureTiles in the middle of QuickSettingsView.
// It can place buttons in a 1x2 to 4x2 grid given the available height.
// Implements pagination to be able to show all visible FeatureTiles.
class ASH_EXPORT FeatureTilesContainerView : public views::View,
                                             public PaginationModelObserver,
                                             public views::FocusChangeListener {
  METADATA_HEADER(FeatureTilesContainerView, views::View)

 public:
  explicit FeatureTilesContainerView(UnifiedSystemTrayController* controller);

  FeatureTilesContainerView(const FeatureTilesContainerView&) = delete;
  FeatureTilesContainerView& operator=(const FeatureTilesContainerView&) =
      delete;

  ~FeatureTilesContainerView() override;

  // Adds feature tiles to display in the tiles container.
  void AddTiles(std::vector<std::unique_ptr<FeatureTile>> tiles);

  // Lays out the existing tiles into rows. Used when the visibility of a tile
  // changes, which might change the number of required rows.
  void RelayoutTiles();

  // Sets the number of rows of feature tiles based on the max height the
  // container can have.
  void SetRowsFromHeight(int max_height);

  // Caps the number of rows of feature tiles when media view is shown, based on
  // the `max_height` the container can have.
  void AdjustRowsForMediaViewVisibility(bool visible, int max_height);

  // PaginationModelObserver:
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionChanged() override;

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void Layout(PassKey) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  // Returns the number of children that are visible.
  int GetVisibleFeatureTileCount() const;

  int displayable_rows() const { return displayable_rows_; }

  int row_count() { return rows_.size(); }

  int page_count() { return pages_.size(); }

 private:
  friend class FeatureTilesContainerViewTest;
  friend class QuickSettingsViewTest;

  class RowContainer;
  class PageContainer;

  // Calculates the number of rows based on the available `height`.
  int CalculateRowsFromHeight(int height);

  // Calculates and sets the position of the container pages that are animating
  // through a scroll, drag gesture or by clicking on a pagination dot.
  // This function is called multiple times per page transition.
  // After animation ends, `SelectedPageChanged` will be called to update bounds
  // of all pages, including those that were not part of the transition.
  void UpdateAnimatingPagesBounds(int old_selected, int new_selected);

  // Updates page splits for feature tiles.
  void UpdateTotalPages();

  // Owned by `UnifiedSystemTrayBubble`.
  const raw_ptr<UnifiedSystemTrayController> controller_;

  // Owned by `UnifiedSystemTrayModel`.
  const raw_ptr<PaginationModel> pagination_model_;

  // List of pages that contain `RowContainer` elements.
  // Owned by views hierarchy.
  std::vector<raw_ptr<PageContainer, VectorExperimental>> pages_;

  // List of rows that contain `FeatureTile` elements.
  // Owned by views hierarchy.
  std::vector<raw_ptr<RowContainer, VectorExperimental>> rows_;

  // Number of rows that can be displayed based on the available
  // max height.
  int displayable_rows_ = 0;

  bool is_media_view_shown_ = false;

  // Used for preventing reentrancy issue in ChildVisibilityChanged. Should be
  // always false if FeatureTilesContainerView is not in the call stack.
  bool changing_visibility_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_TILES_CONTAINER_VIEW_H_
