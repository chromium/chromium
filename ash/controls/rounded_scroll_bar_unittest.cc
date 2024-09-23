// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/controls/rounded_scroll_bar.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

// Scroll bar configuration.
constexpr int kScrollBarWidth = 10;
constexpr int kViewportHeight = 200;
constexpr int kContentHeight = 1000;

// Scroll bar thumb thickness.
constexpr int kThumbThickness = 8;
constexpr int kThumbHoverInset = 2;

// Thumb opacity values.
constexpr float kDefaultOpacity = 0.38f;
constexpr float kActiveOpacity = 1.0f;

// A no-op controller.
class TestScrollBarController : public views::ScrollBarController {
 public:
  // views::ScrollBarController:
  void ScrollToPosition(views::ScrollBar* source, int position) override {}
  int GetScrollIncrement(views::ScrollBar* source,
                         bool is_page,
                         bool is_positive) override {
    return 0;
  }
};

// Uses ViewsTestBase because we may want to move this control into //ui/views
// in the future.
class RoundedScrollBarTest : public views::ViewsTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  RoundedScrollBarTest()
      : ViewsTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~RoundedScrollBarTest() override = default;

  // testing::Test:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {chromeos::features::kJelly, chromeos::features::kJellyroll}, {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {chromeos::features::kJelly, chromeos::features::kJellyroll});
    }

    ViewsTestBase::SetUp();

    // Create a small widget.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(10, 10, 100, 200);
    widget_->Init(std::move(params));
    widget_->Show();

    // Add a vertical scrollbar along the right edge.
    auto* contents = widget_->SetContentsView(std::make_unique<views::View>());
    scroll_bar_ = contents->AddChildView(std::make_unique<RoundedScrollBar>(
        views::ScrollBar::Orientation::kVertical));
    scroll_bar_->set_controller(&controller_);
    scroll_bar_->SetBounds(90, 0, kScrollBarWidth, kViewportHeight);
    scroll_bar_->Update(kViewportHeight, kContentHeight,
                        /*contents_scroll_offset=*/0);
    thumb_ = scroll_bar_->GetThumbForTest();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  views::UniqueWidgetPtr widget_;
  TestScrollBarController controller_;
  raw_ptr<RoundedScrollBar, DanglingUntriaged> scroll_bar_ = nullptr;
  raw_ptr<views::BaseScrollBarThumb, DanglingUntriaged> thumb_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, RoundedScrollBarTest, testing::Bool());

TEST_P(RoundedScrollBarTest, InvisibleByDefault) {
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), 0.f);
}

TEST_P(RoundedScrollBarTest, ShowOnThumbBoundsChanged) {
  // Programmatically scroll the view, which changes the thumb bounds.
  // By default this does not show the thumb.
  scroll_bar_->Update(kViewportHeight, kContentHeight,
                      /*contents_scroll_offset=*/100);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), 0.f);

  // With the setting enabled, changing the thumb bounds shows the thumb.
  scroll_bar_->SetShowOnThumbBoundsChanged(true);
  scroll_bar_->Update(kViewportHeight, kContentHeight,
                      /*contents_scroll_offset=*/200);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(),
            GetParam() ? kActiveOpacity : kDefaultOpacity);
}

TEST_P(RoundedScrollBarTest, ShowOnScrolling) {
  scroll_bar_->ScrollByAmount(views::ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(),
            GetParam() ? kActiveOpacity : kDefaultOpacity);
}

TEST_P(RoundedScrollBarTest, FadesAfterScroll) {
  scroll_bar_->ScrollByAmount(views::ScrollBar::ScrollAmount::kNextLine);
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), 0.f);
}

TEST_P(RoundedScrollBarTest, AlwaysShowThumbIsTrue) {
  scroll_bar_->SetAlwaysShowThumb(true);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(),
            GetParam() ? kActiveOpacity : kDefaultOpacity);
  scroll_bar_->ScrollByAmount(views::ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(),
            GetParam() ? kActiveOpacity : kDefaultOpacity);
  generator_->MoveMouseTo(thumb_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kActiveOpacity);
}

TEST_P(RoundedScrollBarTest, MoveToThumbShowsActiveOpacity) {
  generator_->MoveMouseTo(thumb_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kActiveOpacity);
}

TEST_P(RoundedScrollBarTest, MoveToTrackOutsideThumbShowsInactiveThumb) {
  gfx::Point thumb_bottom = thumb_->GetBoundsInScreen().bottom_center();
  generator_->MoveMouseTo(thumb_bottom.x(), thumb_bottom.y() + 1);

  if (GetParam()) {
    EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), 1.0f);
    EXPECT_EQ(scroll_bar_->GetThickness(), kThumbThickness - kThumbHoverInset);
  } else {
    EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kDefaultOpacity);
    EXPECT_EQ(scroll_bar_->GetThickness(), kThumbThickness);
  }
}

TEST_P(RoundedScrollBarTest, MoveFromThumbToTrackShowsInactiveThumb) {
  generator_->MoveMouseTo(thumb_->GetBoundsInScreen().CenterPoint());
  gfx::Point thumb_bottom = thumb_->GetBoundsInScreen().bottom_center();
  generator_->MoveMouseTo(thumb_bottom.x(), thumb_bottom.y() + 1);
  if (GetParam()) {
    EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), 1.0f);
    EXPECT_EQ(scroll_bar_->GetThickness(), kThumbThickness - kThumbHoverInset);
  } else {
    EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kDefaultOpacity);
    EXPECT_EQ(scroll_bar_->GetThickness(), kThumbThickness);
  }
}

TEST_P(RoundedScrollBarTest, MoveFromTrackToThumbShowsActiveThumb) {
  gfx::Point thumb_bottom = thumb_->GetBoundsInScreen().bottom_center();
  generator_->MoveMouseTo(thumb_bottom.x(), thumb_bottom.y() + 1);
  generator_->MoveMouseTo(thumb_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kActiveOpacity);
  EXPECT_EQ(scroll_bar_->GetThickness(), kThumbThickness);
}

TEST_P(RoundedScrollBarTest, DragOutsideTrackShowsActiveThumb) {
  gfx::Point thumb_center = thumb_->GetBoundsInScreen().CenterPoint();
  generator_->MoveMouseTo(thumb_center);
  generator_->PressLeftButton();
  gfx::Point outside_scroll_bar(thumb_center.x() + kScrollBarWidth,
                                thumb_center.y());
  generator_->MoveMouseTo(outside_scroll_bar);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kActiveOpacity);
  EXPECT_EQ(scroll_bar_->GetThickness(), kThumbThickness);
}

}  // namespace
}  // namespace ash
