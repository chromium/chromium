// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pods_container_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view_observer.h"

namespace ash {

class FeaturePodsContainerViewTest : public NoSessionAshTestBase,
                                     public FeaturePodControllerBase,
                                     public views::ViewObserver {
 public:
  FeaturePodsContainerViewTest() = default;
  ~FeaturePodsContainerViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
    container_ = std::make_unique<FeaturePodsContainerView>(
        controller_.get(), true /* initially_expanded */);
    container_->AddObserver(this);
    preferred_size_changed_count_ = 0;

    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  }

  void TearDown() override {
    controller_.reset();
    container_.reset();
    model_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override { return nullptr; }
  void OnIconPressed() override {}
  SystemTrayItemUmaType GetUmaType() const override {
    return SystemTrayItemUmaType::UMA_TEST;
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    ++preferred_size_changed_count_;
  }

 protected:
  void EnablePagination() {
    scoped_feature_list_->InitAndEnableFeature(
        features::kUnifiedMessageCenterRefactor);
  }

  void AddButtons(int count) {
    for (int i = 0; i < count; ++i) {
      buttons_.push_back(new FeaturePodButton(this));
      container()->AddChildView(buttons_.back());
    }
    container()->SetBoundsRect(gfx::Rect(container_->GetPreferredSize()));
    container()->Layout();
  }

  FeaturePodsContainerView* container() { return container_.get(); }

  PaginationModel* pagination_model() { return model_->pagination_model(); }

  UnifiedSystemTrayController* controller() { return controller_.get(); }

  int preferred_size_changed_count() const {
    return preferred_size_changed_count_;
  }

  std::vector<FeaturePodButton*> buttons_;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<FeaturePodsContainerView> container_;
  std::unique_ptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  int preferred_size_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FeaturePodsContainerViewTest);
};

TEST_F(FeaturePodsContainerViewTest, ExpandedAndCollapsed) {
  const int kNumberOfAddedButtons = kUnifiedFeaturePodItemsInRow * 3;
  EXPECT_LT(kUnifiedFeaturePodMaxItemsInCollapsed, kNumberOfAddedButtons);

  AddButtons(kNumberOfAddedButtons);

  // In expanded state, buttons are laid out in plane.
  EXPECT_LT(buttons_[0]->x(), buttons_[1]->x());
  EXPECT_EQ(buttons_[0]->y(), buttons_[1]->y());
  // If the row exceeds kUnifiedFeaturePodItemsInRow, the next button is placed
  // right under the first button.
  EXPECT_EQ(buttons_[0]->x(), buttons_[kUnifiedFeaturePodItemsInRow]->x());
  EXPECT_LT(buttons_[0]->y(), buttons_[kUnifiedFeaturePodItemsInRow]->y());
  // All buttons are visible.
  for (auto* button : buttons_)
    EXPECT_TRUE(button->GetVisible());

  container()->SetExpandedAmount(0.0);

  // In collapsed state, all buttons are laid out horizontally.
  for (int i = 1; i < kUnifiedFeaturePodMaxItemsInCollapsed; ++i)
    EXPECT_EQ(buttons_[0]->y(), buttons_[i]->y());

  // Buttons exceed kUnifiedFeaturePodMaxItemsInCollapsed are invisible.
  for (int i = 0; i < kNumberOfAddedButtons; ++i) {
    EXPECT_EQ(i < kUnifiedFeaturePodMaxItemsInCollapsed,
              buttons_[i]->GetVisible());
  }
}

TEST_F(FeaturePodsContainerViewTest, HiddenButtonRemainsHidden) {
  AddButtons(kUnifiedFeaturePodMaxItemsInCollapsed);
  // The button is invisible in expanded state.
  buttons_.front()->SetVisible(false);
  container()->SetExpandedAmount(0.0);
  EXPECT_FALSE(buttons_.front()->GetVisible());
  container()->SetExpandedAmount(1.0);
  EXPECT_FALSE(buttons_.front()->GetVisible());
}

TEST_F(FeaturePodsContainerViewTest, BecomeVisibleInCollapsed) {
  AddButtons(kUnifiedFeaturePodMaxItemsInCollapsed);
  // The button is invisible in expanded state.
  buttons_.back()->SetVisible(false);
  container()->SetExpandedAmount(0.0);
  // The button becomes visible in collapsed state.
  buttons_.back()->SetVisible(true);
  // As the container still has remaining space, the button will be visible.
  EXPECT_TRUE(buttons_.back()->GetVisible());
}

TEST_F(FeaturePodsContainerViewTest, StillHiddenInCollapsed) {
  AddButtons(kUnifiedFeaturePodMaxItemsInCollapsed + 1);
  // The button is invisible in expanded state.
  buttons_.back()->SetVisible(false);
  container()->SetExpandedAmount(0.0);
  // The button becomes visible in collapsed state.
  buttons_.back()->SetVisible(true);
  // As the container doesn't have remaining space, the button won't be visible.
  EXPECT_FALSE(buttons_.back()->GetVisible());

  container()->SetExpandedAmount(1.0);
  // The button becomes visible in expanded state.
  EXPECT_TRUE(buttons_.back()->GetVisible());
}

TEST_F(FeaturePodsContainerViewTest, DifferentButtonBecomeVisibleInCollapsed) {
  AddButtons(kUnifiedFeaturePodMaxItemsInCollapsed + 1);
  container()->SetExpandedAmount(0.0);
  // The last button is not visible as it doesn't have enough space.
  EXPECT_FALSE(buttons_.back()->GetVisible());
  // The first button becomes invisible.
  buttons_.front()->SetVisible(false);
  // The last button now has the space for it.
  EXPECT_TRUE(buttons_.back()->GetVisible());
}

TEST_F(FeaturePodsContainerViewTest, SizeChangeByExpanding) {
  // SetExpandedAmount() should not trigger PreferredSizeChanged().
  AddButtons(kUnifiedFeaturePodItemsInRow * 3 - 1);
  EXPECT_EQ(0, preferred_size_changed_count());
  container()->SetExpandedAmount(0.0);
  container()->SetExpandedAmount(0.5);
  container()->SetExpandedAmount(1.0);
  EXPECT_EQ(0, preferred_size_changed_count());
}

TEST_F(FeaturePodsContainerViewTest, SizeChangeByVisibility) {
  // Visibility change should trigger PreferredSizeChanged().
  AddButtons(kUnifiedFeaturePodItemsInRow * 2 + 1);
  EXPECT_EQ(0, preferred_size_changed_count());
  // The first button becomes invisible.
  buttons_.front()->SetVisible(false);
  EXPECT_EQ(1, preferred_size_changed_count());
  // The first button becomes visible.
  buttons_.front()->SetVisible(true);
  EXPECT_EQ(2, preferred_size_changed_count());
}

TEST_F(FeaturePodsContainerViewTest, NumberOfPagesChanged) {
  const int kNumberOfPages = 8;

  EnablePagination();
  AddButtons(kUnifiedFeaturePodItemsInRow * kUnifiedFeaturePodMaxRows *
             kNumberOfPages);

  // Adding buttons to fill kNumberOfPages should cause the the same number of
  // pages to be created.
  EXPECT_EQ(kNumberOfPages, pagination_model()->total_pages());

  // Adding an additional button causes a new page to be added.
  AddButtons(1);
  EXPECT_EQ(pagination_model()->total_pages(), kNumberOfPages + 1);
}

TEST_F(FeaturePodsContainerViewTest, PaginationTransition) {
  const int kNumberOfPages = 8;

  EnablePagination();
  AddButtons(kUnifiedFeaturePodItemsInRow * kUnifiedFeaturePodMaxRows *
             kNumberOfPages);

  // Position of a button should slide to the left during a page
  // transition to the next page.
  gfx::Rect current_bounds;
  gfx::Rect initial_bounds = buttons_[0]->bounds();
  gfx::Rect previous_bounds = initial_bounds;

  PaginationModel::Transition transition(
      pagination_model()->selected_page() + 1, 0);

  for (double i = 0.1; i <= 1.0; i += 0.1) {
    transition.progress = i;
    pagination_model()->SetTransition(transition);

    current_bounds = buttons_[0]->bounds();

    EXPECT_LT(current_bounds.x(), previous_bounds.x());
    EXPECT_EQ(current_bounds.y(), previous_bounds.y());

    previous_bounds = current_bounds;
  }

  // Button Position after page switch should move to the left by a page offset.
  int page_offset = container()->CalculatePreferredSize().width() +
                    kUnifiedFeaturePodsPageSpacing;
  gfx::Rect final_bounds =
      gfx::Rect(initial_bounds.x() - page_offset, initial_bounds.y(),
                initial_bounds.width(), initial_bounds.height());
  pagination_model()->SelectPage(1, false);
  container()->Layout();
  EXPECT_EQ(final_bounds, buttons_[0]->bounds());
}

TEST_F(FeaturePodsContainerViewTest, PaginationDynamicRows) {
  const int kNumberOfFeaturePods = kUnifiedFeaturePodItemsInRow * 3;
  const int padding =
      kUnifiedFeaturePodTopPadding + kUnifiedFeaturePodBottomPadding;
  int row_height =
      kUnifiedFeaturePodSize.height() + kUnifiedFeaturePodVerticalPadding;
  int min_height_for_three_rows = kUnifiedFeaturePodMaxRows * row_height +
                                  padding + kMessageCenterCollapseThreshold;

  EnablePagination();
  AddButtons(kNumberOfFeaturePods);

  // Expect 1 row of feature pods even if there is 0 height.
  container()->SetMaxHeight(0);
  int expected_number_of_pages =
      kNumberOfFeaturePods / kUnifiedFeaturePodItemsInRow;
  if (kNumberOfFeaturePods % kUnifiedFeaturePodItemsInRow)
    expected_number_of_pages += 1;
  EXPECT_EQ(expected_number_of_pages, pagination_model()->total_pages());

  // Expect 2 rows of feature pods when there is enough height to display them
  // but less than enough to display 3 rows.
  container()->SetMaxHeight(min_height_for_three_rows - 1);
  expected_number_of_pages =
      kNumberOfFeaturePods / (2 * kUnifiedFeaturePodItemsInRow);
  if (kNumberOfFeaturePods % (2 * kUnifiedFeaturePodItemsInRow))
    expected_number_of_pages += 1;
  EXPECT_EQ(expected_number_of_pages, pagination_model()->total_pages());

  // Expect 3 rows of feature pods at max even when the max height is very
  // large.
  container()->SetMaxHeight(min_height_for_three_rows + 1);
  expected_number_of_pages =
      kNumberOfFeaturePods / (3 * kUnifiedFeaturePodItemsInRow);
  if (kNumberOfFeaturePods % (3 * kUnifiedFeaturePodItemsInRow))
    expected_number_of_pages += 1;
  EXPECT_EQ(expected_number_of_pages, pagination_model()->total_pages());
}

TEST_F(FeaturePodsContainerViewTest, PaginationGestureHandling) {
  const int kNumberOfPages = 8;

  EnablePagination();
  AddButtons(kUnifiedFeaturePodItemsInRow * kUnifiedFeaturePodMaxRows *
             kNumberOfPages);

  gfx::Point container_origin = container()->GetBoundsInScreen().origin();
  ui::GestureEvent swipe_left_begin(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, -1, 0));
  ui::GestureEvent swipe_left_update(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, -1000, 0));
  ui::GestureEvent swipe_right_begin(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 1, 0));
  ui::GestureEvent swipe_right_update(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 1000, 0));
  ui::GestureEvent swipe_end(container_origin.x(), container_origin.y(), 0,
                             base::TimeTicks(),
                             ui::GestureEventDetails(ui::ET_GESTURE_END));

  int previous_page = pagination_model()->selected_page();

  // Swipe left takes to next page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate swipe left
    container()->OnGestureEvent(&swipe_left_begin);
    container()->OnGestureEvent(&swipe_left_update);
    container()->OnGestureEvent(&swipe_end);

    int current_page = pagination_model()->selected_page();
    // Expect next page
    EXPECT_EQ(previous_page + 1, current_page);
    previous_page = current_page;
  }

  // Swipe left on last page does nothing
  container()->OnGestureEvent(&swipe_left_begin);
  container()->OnGestureEvent(&swipe_left_update);
  container()->OnGestureEvent(&swipe_end);

  EXPECT_EQ(previous_page, pagination_model()->selected_page());

  // Swipe right takes to previous page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate swipe right
    container()->OnGestureEvent(&swipe_right_begin);
    container()->OnGestureEvent(&swipe_right_update);
    container()->OnGestureEvent(&swipe_end);

    int current_page = pagination_model()->selected_page();
    // Expect previous page
    EXPECT_EQ(previous_page - 1, current_page);
    previous_page = current_page;
  }

  // Swipe right on first page does nothing
  container()->OnGestureEvent(&swipe_right_begin);
  container()->OnGestureEvent(&swipe_right_update);
  container()->OnGestureEvent(&swipe_end);

  EXPECT_EQ(previous_page, pagination_model()->selected_page());
}

TEST_F(FeaturePodsContainerViewTest, PaginationScrollHandling) {
  const int kNumberOfPages = 8;
  const int num_fingers = 2;

  EnablePagination();
  AddButtons(kUnifiedFeaturePodItemsInRow * kUnifiedFeaturePodMaxRows *
             kNumberOfPages);

  EXPECT_EQ(kNumberOfPages, pagination_model()->total_pages());

  gfx::Point container_origin = container()->GetBoundsInScreen().origin();

  ui::ScrollEvent fling_up_start(ui::ET_SCROLL_FLING_START, container_origin,
                                 base::TimeTicks(), 0, 0, 100, 0, 10,
                                 num_fingers);

  ui::ScrollEvent fling_down_start(ui::ET_SCROLL_FLING_START, container_origin,
                                   base::TimeTicks(), 0, 0, -100, 0, 10,
                                   num_fingers);

  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL, container_origin,
                               base::TimeTicks(), 0, 0, 0, 0, 0, num_fingers);

  int previous_page = pagination_model()->selected_page();

  // Scroll down takes to next page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate Scroll left
    container()->OnScrollEvent(&fling_down_start);
    container()->OnScrollEvent(&fling_cancel);
    pagination_model()->FinishAnimation();

    int current_page = pagination_model()->selected_page();
    // Expect next page
    EXPECT_EQ(previous_page + 1, current_page);
    previous_page = current_page;
  }

  // Scroll up takes to previous page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate Scroll up
    container()->OnScrollEvent(&fling_up_start);
    container()->OnScrollEvent(&fling_cancel);
    pagination_model()->FinishAnimation();

    int current_page = pagination_model()->selected_page();
    // Expect previous page
    EXPECT_EQ(previous_page - 1, current_page);
    previous_page = current_page;
  }
}

TEST_F(FeaturePodsContainerViewTest, PaginationMouseWheelHandling) {
  const int kNumberOfPages = 8;

  EnablePagination();
  AddButtons(kUnifiedFeaturePodItemsInRow * kUnifiedFeaturePodMaxRows *
             kNumberOfPages);

  gfx::Point container_origin = container()->GetBoundsInScreen().origin();
  ui::MouseWheelEvent wheel_up(gfx::Vector2d(0, 1000), container_origin,
                               container_origin, base::TimeTicks(), 0, 0);

  ui::MouseWheelEvent wheel_down(gfx::Vector2d(0, -1000), container_origin,
                                 container_origin, base::TimeTicks(), 0, 0);

  int previous_page = pagination_model()->selected_page();

  // Mouse wheel down takes to next page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate mouse wheel down
    container()->OnMouseWheel(wheel_down);
    pagination_model()->FinishAnimation();

    int current_page = pagination_model()->selected_page();
    // Expect next page
    EXPECT_EQ(previous_page + 1, current_page);
    previous_page = current_page;
  }

  // Mouse wheel up takes to previous page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate mouse wheel up
    container()->OnMouseWheel(wheel_up);
    pagination_model()->FinishAnimation();

    int current_page = pagination_model()->selected_page();
    // Expect previous page
    EXPECT_EQ(previous_page - 1, current_page);
    previous_page = current_page;
  }
}

TEST_F(FeaturePodsContainerViewTest, NonTogglableButton) {
  // Add one togglable and one non-tobblable button.
  buttons_.push_back(new FeaturePodButton(this, /*is_togglable=*/false));
  AddButtons(1);

  // Non-togglable buttons should be labelled as a regular button for
  // accessibility and vice versa.
  ui::AXNodeData ax_node_data;
  buttons_[0]->icon_button()->GetAccessibleNodeData(&ax_node_data);
  EXPECT_EQ(ax_node_data.role, ax::mojom::Role::kButton);
  buttons_[1]->icon_button()->GetAccessibleNodeData(&ax_node_data);
  EXPECT_EQ(ax_node_data.role, ax::mojom::Role::kToggleButton);
}

}  // namespace ash
