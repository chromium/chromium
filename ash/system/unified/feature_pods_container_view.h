// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

class FeaturePodButton;
class PaginationModel;
class UnifiedSystemTrayController;

// Container of feature pods buttons in the middle of UnifiedSystemTrayView.
// The container has number of buttons placed in 3x3 plane at regular distances.
// FeaturePodButtons implements these individual buttons.
// The container also implements collapsed state where the top 5 buttons are
// horizontally placed and others are hidden.
class ASH_EXPORT FeaturePodsContainerView : public views::View,
                                            public PaginationModelObserver {
 public:
  FeaturePodsContainerView(UnifiedSystemTrayController* controller,
                           bool initially_expanded);
  ~FeaturePodsContainerView() override;

  // Add a FeaturePodButton as a child view and if it's visible add it to the
  // view structure and update the pagination model.
  void AddFeaturePodButton(FeaturePodButton* button);

  // Change the expanded state. 0.0 if collapsed, and 1.0 if expanded.
  // Otherwise, it shows intermediate state. If collapsed, all the buttons are
  // horizontally placed.
  void SetExpandedAmount(double expanded_amount);

  // Set the number of rows of feature pods based on the max height the
  // container can have.
  void SetMaxHeight(int max_height);

  // Get height of the view when |expanded_amount| is set to 1.0.
  int GetExpandedHeight() const;

  // Get the height of the view when |expanded_amount| is set to 0.0.
  int GetCollapsedHeight() const;

  // Returns the number of children that prefer to be visible.
  int GetVisibleCount() const;

  // Make sure button is visible by switching page if needed.
  void EnsurePageWithButton(views::View* button);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildVisibilityChanged(View* child) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void Layout() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  const char* GetClassName() const override;

  int row_count() const { return feature_pod_rows_; }

 private:
  friend class FeaturePodsContainerViewTest;

  // Calculate the current position of the button from |visible_index| and
  // |expanded_amount_|.
  gfx::Point GetButtonPosition(int visible_index) const;

  void UpdateChildVisibility();

  // Update |collapsed_state_padding_|.
  void UpdateCollapsedSidePadding();

  // Calculates the ideal bounds for all feature pods.
  void CalculateIdealBoundsForFeaturePods();

  // Calculate the number of feature pod rows based on available height.
  int CalculateRowsFromHeight(int height);

  // Calculates the offset for |page_of_view| based on current page and
  // transition target page.
  const gfx::Vector2d CalculateTransitionOffset(int page_of_view) const;

  // Returns true if button at provided index is visible.
  bool IsButtonVisible(FeaturePodButton* button, int index);

  // Returns the number of tiles per page.
  int GetTilesPerPage() const;

  // Updates page splits for feature pod buttons.
  void UpdateTotalPages();

  // PaginationModelObserver:
  void TransitionChanged() override;

  UnifiedSystemTrayController* const controller_;

  // Owned by UnifiedSystemTrayModel.
  PaginationModel* const pagination_model_;

  // The last |expanded_amount| passed to SetExpandedAmount().
  double expanded_amount_;

  // Number of rows of feature pods to display. Updated based on the available
  // max height for FeaturePodsContainer.
  int feature_pod_rows_ = 0;

  // Horizontal side padding in dip for collapsed state.
  int collapsed_side_padding_ = 0;

  // Used for preventing reentrancy issue in ChildVisibilityChanged. Should be
  // always false if FeaturePodsContainerView is not in the call stack.
  bool changing_visibility_ = false;

  // A view model that contains all visible feature pod buttons.
  // Used to calculate required number of pages.
  views::ViewModelT<FeaturePodButton> visible_buttons_;

  DISALLOW_COPY_AND_ASSIGN(FeaturePodsContainerView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_PODS_CONTAINER_VIEW_H_
