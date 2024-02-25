// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/controls/scroll_view_gradient_helper.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

constexpr int kWidgetHeight = 100;
constexpr int kWidgetWidth = 100;
constexpr int kGradientSize = 16;

// Uses ViewsTestBase because we may want to move this helper into //ui/views
// in the future.
class ScrollViewGradientHelperTest : public views::ViewsTestBase {
 public:
  ScrollViewGradientHelperTest() = default;
  ~ScrollViewGradientHelperTest() override = default;

  // testing::Test:
  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a small widget.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(10, 10, kWidgetWidth, kWidgetHeight);
    widget_->Init(std::move(params));
    widget_->Show();

    // Add a scroll view and gradient helper.
    auto* contents = widget_->SetContentsView(std::make_unique<views::View>());
    scroll_view_ =
        contents->AddChildView(std::make_unique<views::ScrollView>());
    scroll_view_->SetBounds(0, 0, kWidgetWidth, kWidgetHeight);
    scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    gradient_helper_ =
        std::make_unique<ScrollViewGradientHelper>(scroll_view_, kGradientSize);
  }

  void TearDown() override {
    gradient_helper_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void AddScrollViewContentsWithHeight(int height) {
    auto contents = std::make_unique<views::View>();
    contents->SetSize({kWidgetWidth, height});
    scroll_view_->SetContents(std::move(contents));
    views::test::RunScheduledLayout(scroll_view_);
  }

  bool HasGradientAtTop() {
    const auto& gradient_mask = gradient_helper_->gradient_mask_for_test();
    EXPECT_FALSE(gradient_mask.IsEmpty());
    return cc::MathUtil::IsWithinEpsilon(gradient_mask.steps()[0].fraction,
                                         0.f);
  }

  bool HasGradientAtBottom() {
    const auto& gradient_mask = gradient_helper_->gradient_mask_for_test();
    EXPECT_FALSE(gradient_mask.IsEmpty());
    return cc::MathUtil::IsWithinEpsilon(
        gradient_mask.steps()[gradient_mask.step_count() - 1].fraction, 1.f);
  }

  bool HasGradientMask(const ui::Layer* layer) {
    return !layer->gradient_mask().IsEmpty();
  }

  views::UniqueWidgetPtr widget_;
  raw_ptr<views::ScrollView, DanglingUntriaged> scroll_view_ = nullptr;
  std::unique_ptr<ScrollViewGradientHelper> gradient_helper_;
};

TEST_F(ScrollViewGradientHelperTest, NoGradientForViewThatDoesNotScroll) {
  // Create a short contents view, so the scroll view won't scroll.
  AddScrollViewContentsWithHeight(10);
  gradient_helper_->UpdateGradientMask();

  EXPECT_FALSE(HasGradientMask(scroll_view_->layer()));
  EXPECT_TRUE(gradient_helper_->gradient_mask_for_test().IsEmpty());
}

TEST_F(ScrollViewGradientHelperTest, HasGradientForViewThatScrolls) {
  // Create a tall contents view.
  AddScrollViewContentsWithHeight(500);
  gradient_helper_->UpdateGradientMask();

  // Gradient is shown.
  EXPECT_TRUE(HasGradientMask(scroll_view_->layer()));
  EXPECT_FALSE(gradient_helper_->gradient_mask_for_test().IsEmpty());

  // Shrink the contents view.
  scroll_view_->contents()->SetSize({kWidgetWidth, 10});
  views::test::RunScheduledLayout(scroll_view_);
  gradient_helper_->UpdateGradientMask();

  // Gradient is removed.
  EXPECT_FALSE(HasGradientMask(scroll_view_->layer()));
  EXPECT_TRUE(gradient_helper_->gradient_mask_for_test().IsEmpty());
}

TEST_F(ScrollViewGradientHelperTest, ShowsGradientsBasedOnScrollPosition) {
  // Create a tall contents view.
  AddScrollViewContentsWithHeight(500);
  ASSERT_TRUE(scroll_view_->vertical_scroll_bar());
  gradient_helper_->UpdateGradientMask();

  // Because the scroll position is at the top, only the bottom gradient shows.
  EXPECT_FALSE(HasGradientAtTop());
  EXPECT_TRUE(HasGradientAtBottom());

  // Scroll down. Now both top and bottom should have gradients.
  scroll_view_->vertical_scroll_bar()->ScrollByAmount(
      views::ScrollBar::ScrollAmount::kNextLine);
  EXPECT_TRUE(HasGradientAtTop());
  EXPECT_TRUE(HasGradientAtBottom());

  // Scroll to end. Now only the top should have a gradient.
  scroll_view_->vertical_scroll_bar()->ScrollByAmount(
      views::ScrollBar::ScrollAmount::kEnd);
  EXPECT_TRUE(HasGradientAtTop());
  EXPECT_FALSE(HasGradientAtBottom());
}

TEST_F(ScrollViewGradientHelperTest, DeletingHelperRemovesMaskLayer) {
  // Create a tall contents view.
  AddScrollViewContentsWithHeight(500);
  gradient_helper_->UpdateGradientMask();

  // Precondition: Mask exists.
  ASSERT_TRUE(HasGradientMask(scroll_view_->layer()));

  gradient_helper_.reset();
  ASSERT_FALSE(HasGradientMask(scroll_view_->layer()));
}

}  // namespace
}  // namespace ash
