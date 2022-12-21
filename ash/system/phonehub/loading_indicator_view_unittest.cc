// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/loading_indicator_view.h"

#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

constexpr int kSizeInDip = 5;

}  // namespace

class LoadingIndicatorViewTest : public AshTestBase {
 public:
  LoadingIndicatorViewTest() = default;
  LoadingIndicatorViewTest(const LoadingIndicatorViewTest&) = delete;
  LoadingIndicatorViewTest& operator=(const LoadingIndicatorViewTest&) = delete;

  ~LoadingIndicatorViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    icon_ = std::make_unique<views::ImageView>();
    loading_indicator_view_ =
        std::make_unique<LoadingIndicatorView>(icon_.get());

    const gfx::Rect initial_bounds(0, 0, kSizeInDip, kSizeInDip);
    loading_indicator_view_->SetBoundsRect(initial_bounds);
  }

  void TearDown() override {
    loading_indicator_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  LoadingIndicatorView* loading_indicator_view() {
    return loading_indicator_view_.get();
  }

 private:
  std::unique_ptr<LoadingIndicatorView> loading_indicator_view_;
  std::unique_ptr<views::ImageView> icon_;
};

TEST_F(LoadingIndicatorViewTest, SetAnimating) {
  // The loading indicator default is visible and not animating.
  EXPECT_TRUE(loading_indicator_view()->GetVisible());
  EXPECT_FALSE(loading_indicator_view()->GetAnimating());

  loading_indicator_view()->SetVisible(false);
  EXPECT_FALSE(loading_indicator_view()->GetVisible());

  // The loading indicator should be invisible and not animating if we set
  // animating to false.
  loading_indicator_view()->SetAnimating(false);
  EXPECT_FALSE(loading_indicator_view()->GetVisible());
  EXPECT_FALSE(loading_indicator_view()->GetAnimating());

  // The loading indicator shows up and animates if we set animating to true.
  loading_indicator_view()->SetAnimating(true);
  EXPECT_TRUE(loading_indicator_view()->GetVisible());
  EXPECT_TRUE(loading_indicator_view()->GetAnimating());

  // Again, the loading indicator is invisible and not animating if we set it
  // back.
  loading_indicator_view()->SetAnimating(false);
  EXPECT_FALSE(loading_indicator_view()->GetVisible());
  EXPECT_FALSE(loading_indicator_view()->GetAnimating());
}

TEST_F(LoadingIndicatorViewTest, OnPaintAnimating) {
  gfx::Canvas canvas(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                     /*is_opaque=*/false);

  loading_indicator_view()->SetAnimating(true);
  loading_indicator_view()->OnPaint(&canvas);

  // Expect the center of animation should be the same as controls layer color.
  EXPECT_EQ(AshColorProvider::Get()->GetControlsLayerColor(
                AshColorProvider::ControlsLayerType::kFocusRingColor),
            canvas.GetBitmap().getColor(kSizeInDip / 2, kSizeInDip / 2));
}

TEST_F(LoadingIndicatorViewTest, OnPaintNotAnimating) {
  gfx::Canvas canvas(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                     /*is_opaque=*/false);

  loading_indicator_view()->OnPaint(&canvas);

  // No paint if not animating.
  EXPECT_EQ(0u, canvas.GetBitmap().getColor(kSizeInDip / 2, kSizeInDip / 2));
}

}  // namespace ash
