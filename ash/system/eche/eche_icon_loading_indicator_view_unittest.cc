// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_icon_loading_indicator_view.h"

#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

constexpr int kSizeInDip = 5;

}  // namespace

class EcheIconLoadingIndicatorViewTest : public AshTestBase {
 public:
  EcheIconLoadingIndicatorViewTest() = default;

  EcheIconLoadingIndicatorViewTest(const EcheIconLoadingIndicatorViewTest&) =
      delete;
  EcheIconLoadingIndicatorViewTest& operator=(
      const EcheIconLoadingIndicatorViewTest&) = delete;

  ~EcheIconLoadingIndicatorViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    icon_ = std::make_unique<views::ImageView>();
    eche_icon_loading_indicatior_view_ =
        std::make_unique<EcheIconLoadingIndicatorView>(icon_.get());

    const gfx::Rect initial_bounds(0, 0, kSizeInDip, kSizeInDip);
    eche_icon_loading_indicatior_view_->SetBoundsRect(initial_bounds);
  }

  void TearDown() override {
    eche_icon_loading_indicatior_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  EcheIconLoadingIndicatorView* eche_icon_loading_indicatior_view() {
    return eche_icon_loading_indicatior_view_.get();
  }

 private:
  std::unique_ptr<EcheIconLoadingIndicatorView>
      eche_icon_loading_indicatior_view_;
  std::unique_ptr<views::ImageView> icon_;
};

TEST_F(EcheIconLoadingIndicatorViewTest, SetAnimating) {
  // The loading indicator default is visible and not animating.
  EXPECT_TRUE(eche_icon_loading_indicatior_view()->GetVisible());
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetAnimating());

  eche_icon_loading_indicatior_view()->SetVisible(false);
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetVisible());

  // The loading indicator should be invisible and not animating if we set
  // animating to false.
  eche_icon_loading_indicatior_view()->SetAnimating(false);
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetVisible());
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetAnimating());

  // The loading indicator shows up and animates if we set animating to true.
  eche_icon_loading_indicatior_view()->SetAnimating(true);
  EXPECT_TRUE(eche_icon_loading_indicatior_view()->GetVisible());
  EXPECT_TRUE(eche_icon_loading_indicatior_view()->GetAnimating());

  // Again, the loading indicator is invisible and not animating if we set it
  // back.
  eche_icon_loading_indicatior_view()->SetAnimating(false);
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetVisible());
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetAnimating());
}

TEST_F(EcheIconLoadingIndicatorViewTest, OnPaintAnimating) {
  gfx::Canvas canvas(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                     /*is_opaque=*/false);

  eche_icon_loading_indicatior_view()->SetAnimating(true);
  eche_icon_loading_indicatior_view()->OnPaint(&canvas);

  // Expect the center of animation should be the same as controls layer color.
  EXPECT_EQ(AshColorProvider::Get()->GetControlsLayerColor(
                AshColorProvider::ControlsLayerType::kFocusRingColor),
            canvas.GetBitmap().getColor(kSizeInDip / 2, kSizeInDip / 2));
}

TEST_F(EcheIconLoadingIndicatorViewTest, OnPaintNotAnimating) {
  gfx::Canvas canvas(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                     /*is_opaque=*/false);

  eche_icon_loading_indicatior_view()->OnPaint(&canvas);

  // No paint if not animating.
  EXPECT_EQ(0u, canvas.GetBitmap().getColor(kSizeInDip / 2, kSizeInDip / 2));
}

}  // namespace ash