// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_TILES_CONTAINER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_TILES_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

// TODO(crbug/1368717): use FeatureTile.
class FeaturePodButton;
class PaginationModel;
class UnifiedSystemTrayController;

// Container of FeatureTiles in the middle of QuickSettingsView.
// It can place buttons in a 1x2 to 4x2 grid given the available height.
// Implements pagination to be able to show all visible FeatureTiles.
class ASH_EXPORT FeatureTilesContainerView : public views::View,
                                             public PaginationModelObserver {
 public:
  METADATA_HEADER(FeatureTilesContainerView);

  explicit FeatureTilesContainerView(UnifiedSystemTrayController* controller);

  FeatureTilesContainerView(const FeatureTilesContainerView&) = delete;
  FeatureTilesContainerView& operator=(const FeatureTilesContainerView&) =
      delete;

  ~FeatureTilesContainerView() override;

  // Adds FeatureTileRow elements with FeatureTile placeholders.
  // This function is only used for prototyping.
  void AddPlaceholderFeatureTiles();

  // Sets the number of rows of feature tiles based on the max height the
  // container can have.
  void SetRowsFromHeight(int max_height);

  // Makes sure button is visible by switching page if needed.
  void ShowPageWithButton(views::View* button);

  // PaginationModelObserver:
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionChanged() override;

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  int row_count() const { return feature_tile_rows_; }

 private:
  friend class FeatureTilesContainerViewTest;

  // Calculates the number of feature tile rows based on the available `height`.
  int CalculateRowsFromHeight(int height);

  // Returns the number of tiles per page.
  int GetTilesPerPage() const;

  // Updates page splits for feature tiles.
  void UpdateTotalPages();

  // Owned by UnifiedSystemTrayBubble.
  UnifiedSystemTrayController* const controller_;

  // Owned by UnifiedSystemTrayModel.
  PaginationModel* const pagination_model_;

  // Number of rows of feature tiles to display. Updated based on the available
  // max height for FeatureTilesContainer.
  int feature_tile_rows_ = 0;

  // Used for preventing reentrancy issue in ChildVisibilityChanged. Should be
  // always false if FeatureTilesContainerView is not in the call stack.
  bool changing_visibility_ = false;

  // A view model that contains all visible feature tiles.
  // Used to calculate required number of pages.
  // TODO(crbug/1368717): use FeatureTile.
  views::ViewModelT<FeaturePodButton> visible_buttons_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_TILES_CONTAINER_VIEW_H_
