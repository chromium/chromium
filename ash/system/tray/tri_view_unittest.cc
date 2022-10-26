// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/system/tray/tri_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/test_layout_manager.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// Returns a layout manager that will size views according to their preferred
// size.
std::unique_ptr<views::LayoutManager> CreatePreferredSizeLayoutManager() {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  return layout;
}

}  // namespace

class TriViewTest : public testing::Test {
 public:
  TriViewTest();

  TriViewTest(const TriViewTest&) = delete;
  TriViewTest& operator=(const TriViewTest&) = delete;

 protected:
  // Convenience function to get the minimum height of |container|.
  int GetMinHeight(TriView::Container container) const;

  // Returns the bounds of |child| in the coordinate space of
  // |tri_view_|.
  gfx::Rect GetBoundsInHost(const views::View* child) const;

  // Wrapper functions to access the internals of |tri_view_|.
  views::View* GetContainer(TriView::Container container) const;

  // The test target.
  std::unique_ptr<TriView> tri_view_;
};

TriViewTest::TriViewTest() : tri_view_(std::make_unique<TriView>()) {}

int TriViewTest::GetMinHeight(TriView::Container container) const {
  return tri_view_->GetMinSize(container).height();
}

gfx::Rect TriViewTest::GetBoundsInHost(const views::View* child) const {
  gfx::RectF rect_f(child->bounds());
  views::View::ConvertRectToTarget(child, tri_view_.get(), &rect_f);
  return ToNearestRect(rect_f);
}

views::View* TriViewTest::GetContainer(TriView::Container container) const {
  return tri_view_->GetContainer(container);
}

TEST_F(TriViewTest, PaddingBetweenContainers) {
  const int kPaddingBetweenContainers = 3;
  const int kViewWidth = 10;
  const int kViewHeight = 10;
  const gfx::Size kViewSize(kViewWidth, kViewHeight);
  const int kStartChildExpectedX = 0;
  const int kCenterChildExpectedX =
      kStartChildExpectedX + kViewWidth + kPaddingBetweenContainers;
  const int kEndChildExpectedX =
      kCenterChildExpectedX + kViewWidth + kPaddingBetweenContainers;

  tri_view_ = std::make_unique<TriView>(kPaddingBetweenContainers);
  tri_view_->SetBounds(0, 0, 100, 10);

  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(kStartChildExpectedX, GetBoundsInHost(start_child).x());
  EXPECT_EQ(kCenterChildExpectedX, GetBoundsInHost(center_child).x());
  EXPECT_EQ(kEndChildExpectedX, GetBoundsInHost(end_child).x());
}

TEST_F(TriViewTest, VerticalOrientation) {
  const int kViewWidth = 10;
  const int kViewHeight = 10;
  const gfx::Size kViewSize(kViewWidth, kViewHeight);

  tri_view_ = std::make_unique<TriView>(TriView::Orientation::VERTICAL);
  tri_view_->SetBounds(0, 0, 10, 100);

  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(0, GetBoundsInHost(start_child).y());
  EXPECT_EQ(kViewWidth, GetBoundsInHost(center_child).y());
  EXPECT_EQ(kViewWidth * 2, GetBoundsInHost(end_child).y());
}

TEST_F(TriViewTest, MainAxisMinSize) {
  tri_view_->SetBounds(0, 0, 100, 10);
  const gfx::Size kMinSize(15, 10);
  tri_view_->SetMinSize(TriView::Container::START, kMinSize);
  views::View* child = new views::StaticSizedView(gfx::Size(10, 10));
  tri_view_->AddView(TriView::Container::CENTER, child);

  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(kMinSize.width(), GetBoundsInHost(child).x());
}

TEST_F(TriViewTest, MainAxisMaxSize) {
  tri_view_->SetBounds(0, 0, 100, 10);
  const gfx::Size kMaxSize(10, 10);

  tri_view_->SetMaxSize(TriView::Container::START, kMaxSize);
  views::View* start_child = new views::StaticSizedView(gfx::Size(20, 20));
  tri_view_->AddView(TriView::Container::START, start_child);

  views::View* center_child = new views::StaticSizedView(gfx::Size(10, 10));
  tri_view_->AddView(TriView::Container::CENTER, center_child);

  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(kMaxSize.width(), GetBoundsInHost(center_child).x());
}

TEST_F(TriViewTest, ViewsAddedToCorrectContainers) {
  views::View* start_child = new views::StaticSizedView();
  views::View* center_child = new views::StaticSizedView();
  views::View* end_child = new views::StaticSizedView();

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  EXPECT_TRUE(GetContainer(TriView::Container::START)->Contains(start_child));
  EXPECT_EQ(1u, GetContainer(TriView::Container::START)->children().size());

  EXPECT_TRUE(GetContainer(TriView::Container::CENTER)->Contains(center_child));
  EXPECT_EQ(1u, GetContainer(TriView::Container::CENTER)->children().size());

  EXPECT_TRUE(GetContainer(TriView::Container::END)->Contains(end_child));
  EXPECT_EQ(1u, GetContainer(TriView::Container::END)->children().size());
}

TEST_F(TriViewTest, AddViewViaUniquePtr) {
  auto child_ptr = std::make_unique<views::StaticSizedView>();

  views::View* child =
      tri_view_->AddView(TriView::Container::START, std::move(child_ptr));

  EXPECT_TRUE(GetContainer(TriView::Container::START)->Contains(child));
}

TEST_F(TriViewTest, AddViewAtViaUniquePtr) {
  auto child1_ptr = std::make_unique<views::StaticSizedView>();
  auto child2_ptr = std::make_unique<views::StaticSizedView>();

  views::View* child1 =
      tri_view_->AddViewAt(TriView::Container::START, std::move(child1_ptr), 0);
  // Add the second view in front of the first view.
  views::View* child2 =
      tri_view_->AddViewAt(TriView::Container::START, std::move(child2_ptr), 0);

  // The children are in reverse order.
  EXPECT_EQ(child2, GetContainer(TriView::Container::START)->children()[0]);
  EXPECT_EQ(child1, GetContainer(TriView::Container::START)->children()[1]);
}

TEST_F(TriViewTest, MultipleViewsAddedToTheSameContainer) {
  views::View* child1 = new views::StaticSizedView();
  views::View* child2 = new views::StaticSizedView();

  tri_view_->AddView(TriView::Container::START, child1);
  tri_view_->AddView(TriView::Container::START, child2);

  EXPECT_TRUE(GetContainer(TriView::Container::START)->Contains(child1));
  EXPECT_TRUE(GetContainer(TriView::Container::START)->Contains(child2));
}

TEST_F(TriViewTest, Insets) {
  const int kInset = 3;
  const int kViewHeight = 10;
  const int kExpectedViewHeight = kViewHeight - 2 * kInset;
  const gfx::Size kStartViewSize(10, kViewHeight);
  const gfx::Size kCenterViewSize(100, kViewHeight);
  const gfx::Size kEndViewSize(10, kViewHeight);
  const int kHostWidth = 100;

  tri_view_->SetBounds(0, 0, kHostWidth, kViewHeight);
  tri_view_->SetInsets(gfx::Insets(kInset));

  views::View* start_child = new views::StaticSizedView(kStartViewSize);
  views::View* center_child = new views::StaticSizedView(kCenterViewSize);
  views::View* end_child = new views::StaticSizedView(kEndViewSize);

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  tri_view_->SetFlexForContainer(TriView::Container::CENTER, 1.f);
  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(
      gfx::Rect(kInset, kInset, kStartViewSize.width(), kExpectedViewHeight),
      GetBoundsInHost(start_child));
  EXPECT_EQ(gfx::Rect(kInset + kStartViewSize.width(), kInset,
                      kHostWidth - kStartViewSize.width() -
                          kEndViewSize.width() - 2 * kInset,
                      kExpectedViewHeight),
            GetBoundsInHost(center_child));
  EXPECT_EQ(gfx::Rect(kHostWidth - kEndViewSize.width() - kInset, kInset,
                      kEndViewSize.width(), kExpectedViewHeight),
            GetBoundsInHost(end_child));
}

TEST_F(TriViewTest, InvisibleContainerDoesntTakeUpSpace) {
  const int kViewWidth = 10;
  const int kViewHeight = 10;
  const gfx::Size kViewSize(kViewWidth, kViewHeight);

  tri_view_->SetBounds(0, 0, 30, 10);

  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  tri_view_->SetContainerVisible(TriView::Container::START, false);
  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), GetBoundsInHost(start_child));
  EXPECT_EQ(0, GetBoundsInHost(center_child).x());
  EXPECT_EQ(kViewWidth, GetBoundsInHost(end_child).x());

  tri_view_->SetContainerVisible(TriView::Container::START, true);
  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(0, GetBoundsInHost(start_child).x());
  EXPECT_EQ(kViewWidth, GetBoundsInHost(center_child).x());
  EXPECT_EQ(kViewWidth * 2, GetBoundsInHost(end_child).x());
}

TEST_F(TriViewTest, NonZeroFlex) {
  const int kHostWidth = 100;
  const gfx::Size kDefaultViewSize(10, 10);
  const gfx::Size kCenterViewSize(100, 10);
  const gfx::Size kExpectedCenterViewSize(
      kHostWidth - 2 * kDefaultViewSize.width(), 10);

  tri_view_->SetBounds(0, 0, kHostWidth, 10);

  views::View* start_child = new views::StaticSizedView(kDefaultViewSize);
  views::View* center_child = new views::StaticSizedView(kCenterViewSize);
  views::View* end_child = new views::StaticSizedView(kDefaultViewSize);

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  tri_view_->SetFlexForContainer(TriView::Container::CENTER, 1.f);
  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(kDefaultViewSize, GetBoundsInHost(start_child).size());
  EXPECT_EQ(kExpectedCenterViewSize, GetBoundsInHost(center_child).size());
  EXPECT_EQ(kDefaultViewSize, GetBoundsInHost(end_child).size());
}

TEST_F(TriViewTest, NonZeroFlexTakesPrecedenceOverMinSize) {
  const int kHostWidth = 25;
  const gfx::Size kViewSize(10, 10);
  const gfx::Size kMinCenterSize = kViewSize;
  const gfx::Size kExpectedCenterSize(kHostWidth - 2 * kViewSize.width(), 10);

  tri_view_->SetBounds(0, 0, kHostWidth, 10);

  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  tri_view_->SetFlexForContainer(TriView::Container::CENTER, 1.f);
  tri_view_->SetMinSize(TriView::Container::CENTER, kMinCenterSize);
  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(kViewSize, GetBoundsInHost(start_child).size());
  EXPECT_EQ(kExpectedCenterSize,
            GetBoundsInHost(GetContainer(TriView::Container::CENTER)).size());
  EXPECT_EQ(kViewSize, GetBoundsInHost(end_child).size());
}

TEST_F(TriViewTest, NonZeroFlexTakesPrecedenceOverMaxSize) {
  const int kHostWidth = 100;
  const gfx::Size kViewSize(10, 10);
  const gfx::Size kMaxCenterSize(20, 10);
  const gfx::Size kExpectedCenterSize(kHostWidth - 2 * kViewSize.width(), 10);

  tri_view_->SetBounds(0, 0, kHostWidth, 10);

  views::View* start_child = new views::StaticSizedView(kViewSize);
  views::View* center_child = new views::StaticSizedView(kViewSize);
  views::View* end_child = new views::StaticSizedView(kViewSize);

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  tri_view_->SetFlexForContainer(TriView::Container::CENTER, 1.f);
  tri_view_->SetMaxSize(TriView::Container::CENTER, kMaxCenterSize);
  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(kViewSize, GetBoundsInHost(start_child).size());
  EXPECT_EQ(kExpectedCenterSize,
            GetBoundsInHost(GetContainer(TriView::Container::CENTER)).size());
  EXPECT_EQ(kViewSize, GetBoundsInHost(end_child).size());
}

TEST_F(TriViewTest, ChildViewsPreferredSizeChanged) {
  const int kHostWidth = 500;
  const gfx::Size kMinStartSize(100, 10);

  tri_view_->SetBounds(0, 0, kHostWidth, 10);
  tri_view_->SetMinSize(TriView::Container::START, kMinStartSize);
  tri_view_->SetContainerLayout(TriView::Container::START,
                                CreatePreferredSizeLayoutManager());
  tri_view_->SetFlexForContainer(TriView::Container::CENTER, 1.f);

  views::ProportionallySizedView* child_view =
      new views::ProportionallySizedView(1);
  tri_view_->AddView(TriView::Container::START, child_view);
  // Adding a child view invalidates the layout. Run scheduled layouts.
  views::test::RunScheduledLayout(tri_view_.get());

  child_view->SetPreferredWidth(1);
  views::test::RunScheduledLayout(child_view);
  EXPECT_EQ(child_view->GetPreferredSize(), child_view->size());

  child_view->SetPreferredWidth(2);
  views::test::RunScheduledLayout(child_view);
  EXPECT_EQ(child_view->GetPreferredSize(), child_view->size());
}

TEST_F(TriViewTest, SetMinHeight) {
  const int kMinHeight = 10;

  EXPECT_NE(kMinHeight, GetMinHeight(TriView::Container::START));
  EXPECT_NE(kMinHeight, GetMinHeight(TriView::Container::CENTER));
  EXPECT_NE(kMinHeight, GetMinHeight(TriView::Container::END));

  tri_view_->SetMinHeight(kMinHeight);

  EXPECT_EQ(kMinHeight, GetMinHeight(TriView::Container::START));
  EXPECT_EQ(kMinHeight, GetMinHeight(TriView::Container::CENTER));
  EXPECT_EQ(kMinHeight, GetMinHeight(TriView::Container::END));
}

TEST_F(TriViewTest, ChangingContainersVisibilityPerformsLayout) {
  const int kViewWidth = 10;
  const int kViewHeight = 10;
  const gfx::Size kEndViewSize(kViewWidth, kViewHeight);

  tri_view_->SetBounds(0, 0, 3 * kViewWidth, kViewHeight);
  tri_view_->SetFlexForContainer(TriView::Container::CENTER, 1.f);

  views::View* start_child = new views::View();
  start_child->SetPreferredSize(kEndViewSize);

  views::View* center_child = new views::View();
  center_child->SetPreferredSize(gfx::Size(2 * kViewWidth, kViewHeight));

  views::View* end_child = new views::View();
  end_child->SetPreferredSize(kEndViewSize);

  tri_view_->AddView(TriView::Container::START, start_child);
  tri_view_->AddView(TriView::Container::CENTER, center_child);
  tri_view_->AddView(TriView::Container::END, end_child);

  views::test::RunScheduledLayout(tri_view_.get());

  EXPECT_EQ(gfx::Size(kViewWidth, kViewHeight), center_child->size());

  tri_view_->SetContainerVisible(TriView::Container::END, false);

  EXPECT_EQ(gfx::Size(2 * kViewWidth, kViewHeight), center_child->size());
}

}  // namespace ash
