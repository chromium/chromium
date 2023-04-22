// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/display_detailed_view.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class DisplayDetailedViewTest : public AshTestBase {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures({features::kQsRevamp}, {});
    AshTestBase::SetUp();

    // Create a widget so tests can click on views.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    delegate_ = std::make_unique<FakeDetailedViewDelegate>();
    // Passes in a fake delegate and a nullptr as `tray_controller` since we
    // don't need to test the actual functionality of controllers.
    detailed_view_ =
        widget_->SetContentsView(std::make_unique<DisplayDetailedView>(
            delegate_.get(), /*tray_controller=*/nullptr));
  }

  void TearDown() override {
    widget_.reset();
    detailed_view_ = nullptr;
    delegate_.reset();
    AshTestBase::TearDown();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<DetailedViewDelegate> delegate_;
  raw_ptr<DisplayDetailedView, ExperimentalAsh> detailed_view_ = nullptr;
};

TEST_F(DisplayDetailedViewTest, ScrollContentChildren) {
  // The scroll content has two children, one feature tile container and one
  // `UnifiedBrightnessView`.
  views::View* scroll_content =
      detailed_view_->GetViewByID(VIEW_ID_QS_DISPLAY_SCROLL_CONTENT);
  ASSERT_TRUE(scroll_content);
  ASSERT_EQ(scroll_content->children().size(), 2u);

  // The first child of scroll content is the `tile_container`, which has two
  // children (night light and dark mode feature tiles).
  views::View* tile_container =
      scroll_content->GetViewByID(VIEW_ID_QS_DISPLAY_TILE_CONTAINER);
  ASSERT_TRUE(tile_container);
  ASSERT_EQ(tile_container->children().size(), 2u);
  EXPECT_STREQ(tile_container->children()[0]->GetClassName(), "FeatureTile");
  EXPECT_STREQ(tile_container->children()[1]->GetClassName(), "FeatureTile");

  // The second children of scroll content is the `UnifiedBrightnessView`.
  views::View* unified_brightness_view =
      scroll_content->GetViewByID(VIEW_ID_QS_DISPLAY_BRIGHTNESS_SLIDER);
  EXPECT_STREQ(unified_brightness_view->GetClassName(),
               "UnifiedBrightnessView");
}

}  // namespace
}  // namespace ash
