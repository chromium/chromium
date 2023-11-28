// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/toast_data.h"
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

// Creates a `ToastData` object with only the required elements.
ToastData CreateBaseToastData() {
  const std::string id = "id";
  const std::u16string text = u"text";
  auto catalog_name = ToastCatalogName::kTestCatalogName;
  return ToastData(id, catalog_name, text);
}

// Body text constants
const std::u16string button_text = u"Button";
const std::u16string long_body_text =
    u"Nudge body text should be clear, short and succint (80 characters "
    u"recommended)";

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
  // Set up base toast data, which has an id, a catalog name and a body text.
  auto toast_data = CreateBaseToastData();

  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/3, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, WithLeadingIcon) {
  // Set up base toast data and add a leading icon.
  auto toast_data = CreateBaseToastData();
  toast_data.leading_icon = &kSystemMenuBusinessIcon;

  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/3, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, WithButton) {
  // Set up base toast data and add a button.
  auto toast_data = CreateBaseToastData();
  toast_data.dismiss_text = button_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/3, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, WithLeadingIconAndButton) {
  // Set up base toast data and add a leading icon and a button.
  auto toast_data = CreateBaseToastData();
  toast_data.leading_icon = &kSystemMenuBusinessIcon;
  toast_data.dismiss_text = button_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/3, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, Multiline_TextOnly) {
  // Set up a multiline text toast.
  auto toast_data = CreateBaseToastData();
  toast_data.text = long_body_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/3, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, Multiline_WithLeadingIcon) {
  // Set up a multiline text toast.
  auto toast_data = CreateBaseToastData();
  toast_data.text = long_body_text;
  toast_data.leading_icon = &kSystemMenuBusinessIcon;

  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/3, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, Multiline_WithButton) {
  // Set up a multiline text toast and add a button.
  auto toast_data = CreateBaseToastData();
  toast_data.text = long_body_text;
  toast_data.dismiss_text = button_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/3, GetContentsView()));
}

TEST_F(SystemToastViewPixelTest, Multiline_WithLeadingIconAndButton) {
  // Set up a multiline text toast and add a leading icon and a button.
  auto toast_data = CreateBaseToastData();
  toast_data.text = long_body_text;
  toast_data.leading_icon = &kSystemMenuBusinessIcon;
  toast_data.dismiss_text = button_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/3, GetContentsView()));
}

}  // namespace ash
