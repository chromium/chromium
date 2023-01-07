// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/controls/rounded_scroll_bar.h"
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
class RoundedScrollBarTest : public views::ViewsTestBase {
 public:
  RoundedScrollBarTest()
      : ViewsTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~RoundedScrollBarTest() override = default;

  // testing::Test:
  void SetUp() override {
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
    scroll_bar_ = contents->AddChildView(
        std::make_unique<RoundedScrollBar>(/*horizontal=*/false));
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
  RoundedScrollBar* scroll_bar_ = nullptr;
  views::BaseScrollBarThumb* thumb_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

TEST_F(RoundedScrollBarTest, InvisibleByDefault) {
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), 0.f);
}

TEST_F(RoundedScrollBarTest, ShowOnThumbBoundsChanged) {
  // Programmatically scroll the view, which changes the thumb bounds.
  // By default this does not show the thumb.
  scroll_bar_->Update(kViewportHeight, kContentHeight,
                      /*contents_scroll_offset=*/100);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), 0.f);

  // With the setting enabled, changing the thumb bounds shows the thumb.
  scroll_bar_->SetShowOnThumbBoundsChanged(true);
  scroll_bar_->Update(kViewportHeight, kContentHeight,
                      /*contents_scroll_offset=*/200);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kDefaultOpacity);
}

TEST_F(RoundedScrollBarTest, ScrollingShowsDefaultOpacity) {
  scroll_bar_->ScrollByAmount(views::ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kDefaultOpacity);
}

TEST_F(RoundedScrollBarTest, FadesAfterScroll) {
  scroll_bar_->ScrollByAmount(views::ScrollBar::ScrollAmount::kNextLine);
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), 0.f);
}

TEST_F(RoundedScrollBarTest, MoveToThumbShowsActiveOpacity) {
  generator_->MoveMouseTo(thumb_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kActiveOpacity);
}

TEST_F(RoundedScrollBarTest, MoveToTrackOutsideThumbShowsDefaultOpacity) {
  gfx::Point thumb_bottom = thumb_->GetBoundsInScreen().bottom_center();
  generator_->MoveMouseTo(thumb_bottom.x(), thumb_bottom.y() + 1);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kDefaultOpacity);
}

TEST_F(RoundedScrollBarTest, MoveFromThumbToTrackShowsDefaultOpacity) {
  generator_->MoveMouseTo(thumb_->GetBoundsInScreen().CenterPoint());
  gfx::Point thumb_bottom = thumb_->GetBoundsInScreen().bottom_center();
  generator_->MoveMouseTo(thumb_bottom.x(), thumb_bottom.y() + 1);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kDefaultOpacity);
}

TEST_F(RoundedScrollBarTest, MoveFromTrackToThumbShowsActiveOpacity) {
  gfx::Point thumb_bottom = thumb_->GetBoundsInScreen().bottom_center();
  generator_->MoveMouseTo(thumb_bottom.x(), thumb_bottom.y() + 1);
  generator_->MoveMouseTo(thumb_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kActiveOpacity);
}

TEST_F(RoundedScrollBarTest, DragOutsideTrackShowsActiveOpacity) {
  gfx::Point thumb_center = thumb_->GetBoundsInScreen().CenterPoint();
  generator_->MoveMouseTo(thumb_center);
  generator_->PressLeftButton();
  gfx::Point outside_scroll_bar(thumb_center.x() + kScrollBarWidth,
                                thumb_center.y());
  generator_->MoveMouseTo(outside_scroll_bar);
  EXPECT_EQ(thumb_->layer()->GetTargetOpacity(), kActiveOpacity);
}

}  // namespace
}  // namespace ash
