// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/toast/system_toast_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Test constants
const std::u16string kTestText = u"text";
const std::u16string kTestLongText =
    u"Nudge body text should be clear, short and succint (80 characters "
    u"recommended)";
const std::u16string kTestButtonText = u"Button";
const gfx::VectorIcon* kTestIcon = &kSystemMenuBusinessIcon;

}  // namespace

class SystemToastViewPixelTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    test_widget_ = CreateFramelessTestWidget();
    // Set a size larger than the toast max dimensions.
    test_widget_->SetBounds(gfx::Rect(700, 70));
    test_widget_->SetContentsView(
        views::Builder<views::FlexLayoutView>()
            .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
            .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
            .SetBackground(views::CreateThemedSolidBackground(
                cros_tokens::kCrosSysSystemBaseElevated))
            .Build());
  }

  views::View* GetContentsView() { return test_widget_->GetContentsView(); }

  void TearDown() override {
    test_widget_.reset();
    AshTestBase::TearDown();
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  std::unique_ptr<views::Widget> test_widget_;
};

TEST_F(SystemToastViewPixelTest, TextOnly) {
  GetContentsView()->AddChildView(std::make_unique<SystemToastView>(kTestText));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/4, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, WithLeadingIcon) {
  GetContentsView()->AddChildView(std::make_unique<SystemToastView>(
      /*text=*/kTestText, /*dismiss_text=*/std::u16string(),
      /*dismiss_callback=*/base::DoNothing(), /*leading_icon=*/kTestIcon));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/5, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, WithButton) {
  GetContentsView()->AddChildView(std::make_unique<SystemToastView>(
      /*text=*/kTestText, /*dismiss_text=*/kTestButtonText));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/6, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, WithLeadingIconAndButton) {
  GetContentsView()->AddChildView(std::make_unique<SystemToastView>(
      /*text=*/kTestText, /*dismiss_text=*/kTestButtonText,
      /*dismiss_callback=*/base::DoNothing(), /*leading_icon=*/kTestIcon));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/5, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, Multiline_TextOnly) {
  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(kTestLongText));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/4, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, Multiline_WithLeadingIcon) {
  GetContentsView()->AddChildView(std::make_unique<SystemToastView>(
      /*text=*/kTestLongText, /*dismiss_text=*/std::u16string(),
      /*dismiss_callback=*/base::DoNothing(), /*leading_icon=*/kTestIcon));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/4, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, Multiline_WithButton) {
  GetContentsView()->AddChildView(std::make_unique<SystemToastView>(
      /*text=*/kTestLongText, /*dismiss_text=*/kTestButtonText));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/4, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, Multiline_WithLeadingIconAndButton) {
  GetContentsView()->AddChildView(std::make_unique<SystemToastView>(
      /*text=*/kTestLongText, /*dismiss_text=*/kTestButtonText,
      /*dismiss_callback=*/base::DoNothing(), /*leading_icon=*/kTestIcon));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/4, GetContentsView()));
}

}  // namespace ash
