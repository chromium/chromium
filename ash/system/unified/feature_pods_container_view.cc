// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pods_container_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"

namespace ash {

FeaturePodsContainerView::FeaturePodsContainerView(
    UnifiedSystemTrayController* controller,
    bool initially_expanded)
    : controller_(controller),
      pagination_model_(controller->model()->pagination_model()),
      expanded_amount_(initially_expanded ? 1.0 : 0.0),
      feature_pod_rows_(kUnifiedFeaturePodMaxRows) {
  pagination_model_->AddObserver(this);
}

FeaturePodsContainerView::~FeaturePodsContainerView() {
  DCHECK(pagination_model_);
  pagination_model_->RemoveObserver(this);
}

void FeaturePodsContainerView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  if (expanded_amount_ == expanded_amount)
    return;
  expanded_amount_ = expanded_amount;

  int visible_index = 0;
  for (auto* view : children()) {
    FeaturePodButton* button = static_cast<FeaturePodButton*>(view);
    // When collapsing from page 1, buttons below the second row fade out
    // while the rest move up into a single row for the collapsed state.
    // When collapsing from page > 1, each row of buttons fades out one by one
    // and once expanded_amount is less than kCollapseThreshold we begin to
    // fade in the single row of buttons for the collapsed state.
    if (expanded_amount_ < kCollapseThreshold &&
        pagination_model_->selected_page() > 0) {
      button->SetExpandedAmount(1.0 - expanded_amount,
                                true /* fade_icon_button */);
    } else if (visible_index > kUnifiedFeaturePodMaxItemsInCollapsed) {
      int row =
          (visible_index / kUnifiedFeaturePodItemsInRow) % feature_pod_rows_;
      double button_expanded_amount =
          expanded_amount
              ? std::min(1.0, expanded_amount +
                                  (0.25 * (feature_pod_rows_ - row - 1)))
              : expanded_amount;
      button->SetExpandedAmount(button_expanded_amount,
                                true /* fade_icon_button */);
    } else {
      button->SetExpandedAmount(expanded_amount, false /* fade_icon_button */);
    }
    if (view->GetVisible())
      visible_index++;
  }
  UpdateChildVisibility();
  // We have to call Layout() explicitly here.
  Layout();
}

int FeaturePodsContainerView::GetExpandedHeight() const {
  const int visible_count = GetVisibleCount();

  // floor(visible_count / kUnifiedFeaturePodItemsInRow)
  int number_of_lines = (visible_count + kUnifiedFeaturePodItemsInRow - 1) /
                        kUnifiedFeaturePodItemsInRow;

  if (features::IsUnifiedMessageCenterRefactorEnabled())
    number_of_lines = std::min(number_of_lines, feature_pod_rows_);

  return kUnifiedFeaturePodBottomPadding +
         (kUnifiedFeaturePodVerticalPadding + kUnifiedFeaturePodSize.height()) *
             number_of_lines;
}

int FeaturePodsContainerView::GetCollapsedHeight() const {
  return 2 * kUnifiedFeaturePodCollapsedVerticalPadding +
         kUnifiedFeaturePodCollapsedSize.height();
}

gfx::Size FeaturePodsContainerView::CalculatePreferredSize() const {
  return gfx::Size(
      kTrayMenuWidth,
      static_cast<int>(GetCollapsedHeight() * (1.0 - expanded_amount_) +
                       GetExpandedHeight() * expanded_amount_));
}

void FeaturePodsContainerView::ChildVisibilityChanged(View* child) {
  // ChildVisibilityChanged can change child visibility using
  // SetVisibleByContainer() in UpdateChildVisibility(), so we have to prevent
  // reentrancy.
  if (changing_visibility_)
    return;

  // Visibility change is caused by the child's SetVisible(), so update actual
  // visibility and propagate the container size change to the parent.
  UpdateChildVisibility();
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void FeaturePodsContainerView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  UpdateChildVisibility();
}

void FeaturePodsContainerView::Layout() {
  UpdateCollapsedSidePadding();
  CalculateIdealBoundsForFeaturePods();
  for (int i = 0; i < visible_buttons_.view_size(); ++i) {
    auto* button = visible_buttons_.view_at(i);
    button->SetBoundsRect(visible_buttons_.ideal_bounds(i));
  }
}

const char* FeaturePodsContainerView::GetClassName() const {
  return "FeaturePodsContainerView";
}

void FeaturePodsContainerView::UpdateChildVisibility() {
  DCHECK(!changing_visibility_);
  changing_visibility_ = true;

  int visible_count = 0;
  for (auto* view : children()) {
    auto* child = static_cast<FeaturePodButton*>(view);
    bool visible = IsButtonVisible(child, visible_count);
    child->SetVisibleByContainer(visible);
    if (visible) {
      if (visible_buttons_.GetIndexOfView(child) < 0)
        visible_buttons_.Add(child, visible_count);
      ++visible_count;
    } else {
      if (visible_buttons_.GetIndexOfView(child))
        visible_buttons_.Remove(visible_buttons_.GetIndexOfView(child));
    }
  }
  UpdateTotalPages();
  changing_visibility_ = false;
}

bool FeaturePodsContainerView::IsButtonVisible(FeaturePodButton* button,
                                               int index) {
  return button->visible_preferred() &&
         (expanded_amount_ > 0.0 ||
          index < kUnifiedFeaturePodMaxItemsInCollapsed);
}

int FeaturePodsContainerView::GetVisibleCount() const {
  return std::count_if(
      children().cbegin(), children().cend(), [](const auto* v) {
        return static_cast<const FeaturePodButton*>(v)->visible_preferred();
      });
}

void FeaturePodsContainerView::EnsurePageWithButton(views::View* button) {
  int index = visible_buttons_.GetIndexOfView(button->parent());
  if (index < 0)
    return;

  int tiles_per_page = GetTilesPerPage();
  int first_index = pagination_model_->selected_page() * tiles_per_page;
  int last_index =
      ((pagination_model_->selected_page() + 1) * tiles_per_page) - 1;
  if (index < first_index || index > last_index) {
    int page = ((index + 1) / tiles_per_page) +
               ((index + 1) % tiles_per_page ? 1 : 0) - 1;

    pagination_model_->SelectPage(page, true /*animate*/);
  }
}

gfx::Point FeaturePodsContainerView::GetButtonPosition(
    int visible_index) const {
  int row = visible_index / kUnifiedFeaturePodItemsInRow;
  int column = visible_index % kUnifiedFeaturePodItemsInRow;
  int x = kUnifiedFeaturePodHorizontalSidePadding +
          (kUnifiedFeaturePodSize.width() +
           kUnifiedFeaturePodHorizontalMiddlePadding) *
              column;
  int y = kUnifiedFeaturePodTopPadding + (kUnifiedFeaturePodSize.height() +
                                          kUnifiedFeaturePodVerticalPadding) *
                                             row;

  // Only feature pods visible in the collapsed state (i.e. the first 5 pods)
  // move during expansion/collapse. Otherwise, the button position will always
  // be constant.
  if (expanded_amount_ == 1.0 ||
      visible_index > kUnifiedFeaturePodMaxItemsInCollapsed ||
      (pagination_model_->selected_page() > 0 &&
       expanded_amount_ >= kCollapseThreshold)) {
    return gfx::Point(x, y);
  }

  int collapsed_x =
      collapsed_side_padding_ + (kUnifiedFeaturePodCollapsedSize.width() +
                                 kUnifiedFeaturePodCollapsedHorizontalPadding) *
                                    visible_index;
  int collapsed_y = kUnifiedFeaturePodCollapsedVerticalPadding;

  // When fully collapsed or collapsing from a different page to the first
  // page, just return the collapsed position.
  if (expanded_amount_ == 0.0 || (expanded_amount_ < kCollapseThreshold &&
                                  pagination_model_->selected_page() > 0))
    return gfx::Point(collapsed_x, collapsed_y);

  // Button width is different between expanded and collapsed states.
  // During the transition, expanded width is used, so it should be adjusted.
  collapsed_x -= (kUnifiedFeaturePodSize.width() -
                  kUnifiedFeaturePodCollapsedSize.width()) /
                 2;

  return gfx::Point(
      x * expanded_amount_ + collapsed_x * (1.0 - expanded_amount_),
      y * expanded_amount_ + collapsed_y * (1.0 - expanded_amount_));
}

int FeaturePodsContainerView::CalculateRowsFromHeight(int height) {
  int available_height =
      height - kUnifiedFeaturePodBottomPadding - kUnifiedFeaturePodTopPadding;
  int row_height =
      kUnifiedFeaturePodSize.height() + kUnifiedFeaturePodVerticalPadding;

  // Only use the max number of rows when there is enough space
  // to show the fully expanded message center and quick settings.
  if (available_height > (kUnifiedFeaturePodMaxRows * row_height) &&
      available_height - (kUnifiedFeaturePodMaxRows * row_height) >
          kMessageCenterCollapseThreshold) {
    return kUnifiedFeaturePodMaxRows;
  }

  // Use 1 less than the max number of rows when there is enough
  // space to show the message center in the collapsed state along
  // with the expanded quick settings.
  int feature_pod_rows = kUnifiedFeaturePodMaxRows - 1;
  if (available_height > (feature_pod_rows * row_height) &&
      available_height - (feature_pod_rows * row_height) >
          kStackedNotificationBarHeight) {
    return feature_pod_rows;
  }

  return kUnifiedFeaturePodMinRows;
}

void FeaturePodsContainerView::SetMaxHeight(int max_height) {
  int feature_pod_rows = CalculateRowsFromHeight(max_height);

  if (feature_pod_rows_ != feature_pod_rows) {
    feature_pod_rows_ = feature_pod_rows;
    UpdateTotalPages();
  }
}

void FeaturePodsContainerView::UpdateCollapsedSidePadding() {
  const int visible_count =
      std::min(GetVisibleCount(), kUnifiedFeaturePodMaxItemsInCollapsed);

  int contents_width =
      visible_count * kUnifiedFeaturePodCollapsedSize.width() +
      (visible_count - 1) * kUnifiedFeaturePodCollapsedHorizontalPadding;

  collapsed_side_padding_ = (kTrayMenuWidth - contents_width) / 2;
  DCHECK(collapsed_side_padding_ > 0);
}

void FeaturePodsContainerView::AddFeaturePodButton(FeaturePodButton* button) {
  int view_size = visible_buttons_.view_size();
  if (IsButtonVisible(button, view_size)) {
    visible_buttons_.Add(button, view_size);
  }
  AddChildView(button);

  UpdateTotalPages();
}

const gfx::Vector2d FeaturePodsContainerView::CalculateTransitionOffset(
    int page_of_view) const {
  gfx::Size grid_size = CalculatePreferredSize();

  // If there is a transition, calculates offset for current and target page.
  const int current_page = pagination_model_->selected_page();
  const PaginationModel::Transition& transition =
      pagination_model_->transition();
  const bool is_valid =
      pagination_model_->is_valid_page(transition.target_page);

  // Transition to previous page means negative offset.
  const int direction = transition.target_page > current_page ? -1 : 1;

  int x_offset = 0;
  int y_offset = 0;

  // Page size including padding pixels. A tile.x + page_width means the same
  // tile slot in the next page.
  const int page_width = grid_size.width() + kUnifiedFeaturePodsPageSpacing;
  if (page_of_view < current_page)
    x_offset = -page_width;
  else if (page_of_view > current_page)
    x_offset = page_width;

  if (is_valid) {
    if (page_of_view == current_page ||
        page_of_view == transition.target_page) {
      x_offset += transition.progress * page_width * direction;
    }
  }

  return gfx::Vector2d(x_offset, y_offset);
}

void FeaturePodsContainerView::CalculateIdealBoundsForFeaturePods() {
  for (int i = 0; i < visible_buttons_.view_size(); ++i) {
    gfx::Rect tile_bounds;
    gfx::Size child_size;
    // When we are on the first page we calculate bounds for an expanded tray
    // when expanded_amount is greater than zero. However, when not on the first
    // page, we only calculate bounds for an expanded tray until expanded_amount
    // is above kCollapseThreshold. Below kCollapseThreshold we return collapsed
    // bounds.
    if ((expanded_amount_ > 0.0 && pagination_model_->selected_page() == 0) ||
        expanded_amount_ >= kCollapseThreshold) {
      child_size = kUnifiedFeaturePodSize;

      // Flexibly give more height if the child view doesn't fit into the
      // default height, so that label texts won't be broken up in the middle.
      child_size.set_height(std::max(
          child_size.height(),
          visible_buttons_.view_at(i)->GetHeightForWidth(child_size.height())));

      tile_bounds =
          gfx::Rect(GetButtonPosition(i % GetTilesPerPage()), child_size);
      // TODO(amehfooz): refactor this logic so that the ideal_bounds are set
      // once when the transition starts and the actual feature pod bounds are
      // interpolated using the ideal_bounds as the transition progresses.
      tile_bounds.Offset(CalculateTransitionOffset(i / GetTilesPerPage()));
    } else {
      child_size = kUnifiedFeaturePodCollapsedSize;
      tile_bounds = gfx::Rect(GetButtonPosition(i), child_size);
    }

    visible_buttons_.set_ideal_bounds(i, tile_bounds);
  }
}

int FeaturePodsContainerView::GetTilesPerPage() const {
  if (features::IsUnifiedMessageCenterRefactorEnabled())
    return kUnifiedFeaturePodItemsInRow * feature_pod_rows_;
  else
    return children().size();
}

void FeaturePodsContainerView::UpdateTotalPages() {
  int total_pages = 0;

  int total_visible = visible_buttons_.view_size();
  int tiles_per_page = GetTilesPerPage();

  if (!visible_buttons_.view_size() || !tiles_per_page) {
    total_pages = 0;
  } else {
    total_pages = (total_visible / tiles_per_page) +
                  (total_visible % tiles_per_page ? 1 : 0);
  }
  pagination_model_->SetTotalPages(total_pages);
}

void FeaturePodsContainerView::TransitionChanged() {
  const PaginationModel::Transition& transition =
      pagination_model_->transition();
  if (pagination_model_->is_valid_page(transition.target_page))
    Layout();
}

void FeaturePodsContainerView::OnGestureEvent(ui::GestureEvent* event) {
  if (controller_->pagination_controller()->OnGestureEvent(*event,
                                                           GetContentsBounds()))
    event->SetHandled();
}

void FeaturePodsContainerView::OnScrollEvent(ui::ScrollEvent* event) {
  controller_->pagination_controller()->OnScroll(
      gfx::Vector2d(event->x_offset(), event->y_offset()), event->type());
  event->SetHandled();
}

bool FeaturePodsContainerView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return controller_->pagination_controller()->OnScroll(event.offset(),
                                                        event.type());
}

}  // namespace ash
