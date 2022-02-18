// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/controls/gradient_layer_delegate.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

constexpr int kWidgetHeight = 100;
constexpr int kWidgetWidth = 100;

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
    gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(scroll_view_);
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
    scroll_view_->Layout();
  }

  bool HasGradientAtTop() {
    auto* gradient_layer = gradient_helper_->gradient_layer_for_test();
    return !gradient_layer->start_fade_zone_bounds().IsEmpty();
  }

  bool HasGradientAtBottom() {
    auto* gradient_layer = gradient_helper_->gradient_layer_for_test();
    return !gradient_layer->end_fade_zone_bounds().IsEmpty();
  }

  views::UniqueWidgetPtr widget_;
  views::ScrollView* scroll_view_ = nullptr;
  std::unique_ptr<ScrollViewGradientHelper> gradient_helper_;
};

TEST_F(ScrollViewGradientHelperTest, NoGradientForViewThatDoesNotScroll) {
  // Create a short contents view, so the scroll view won't scroll.
  AddScrollViewContentsWithHeight(10);
  gradient_helper_->UpdateGradientZone();

  EXPECT_FALSE(scroll_view_->layer()->layer_mask_layer());
  EXPECT_FALSE(gradient_helper_->gradient_layer_for_test());
}

TEST_F(ScrollViewGradientHelperTest, HasGradientForViewThatScrolls) {
  // Create a tall contents view.
  AddScrollViewContentsWithHeight(500);
  gradient_helper_->UpdateGradientZone();

  // Gradient is shown.
  EXPECT_TRUE(scroll_view_->layer()->layer_mask_layer());
  EXPECT_TRUE(gradient_helper_->gradient_layer_for_test());

  // Shrink the contents view.
  scroll_view_->contents()->SetSize({kWidgetWidth, 10});
  scroll_view_->Layout();
  gradient_helper_->UpdateGradientZone();

  // Gradient is removed.
  EXPECT_FALSE(scroll_view_->layer()->layer_mask_layer());
  EXPECT_FALSE(gradient_helper_->gradient_layer_for_test());
}

TEST_F(ScrollViewGradientHelperTest, ShowsGradientsBasedOnScrollPosition) {
  // Create a tall contents view.
  AddScrollViewContentsWithHeight(500);
  ASSERT_TRUE(scroll_view_->vertical_scroll_bar());
  gradient_helper_->UpdateGradientZone();

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
  gradient_helper_->UpdateGradientZone();

  // Precondition: Mask layer exists.
  ASSERT_TRUE(scroll_view_->layer()->layer_mask_layer());

  // Deleting the helper removes the mask layer.
  gradient_helper_.reset();
  EXPECT_FALSE(scroll_view_->layer()->layer_mask_layer());
}

}  // namespace
}  // namespace ash
