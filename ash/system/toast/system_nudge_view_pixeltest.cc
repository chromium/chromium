// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/system/toast/system_nudge_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Creates an `AnchoredNudgeData` object with only the required elements.
// This will create a nudge shown on its default location.
AnchoredNudgeData CreateBaseNudgeData() {
  // Set up nudge data contents.
  const std::string id = "id";
  const std::u16string body_text = u"text";
  auto catalog_name = NudgeCatalogName::kTestCatalogName;

  return AnchoredNudgeData(id, catalog_name, body_text);
}

// Nudge constants
const std::u16string button_text = u"Button";
const std::u16string title_text = u"Title text";
const std::u16string long_body_text =
    u"Nudge body text should be clear, short and succinct (80 characters "
    u"recommended)";

}  // namespace

class SystemNudgeViewPixelTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    test_widget_ = CreateFramelessTestWidget();
    // Set a size larger than the nudge max dimensions.
    test_widget_->SetBounds(gfx::Rect(800, 600));
    test_widget_->SetContentsView(
        views::Builder<views::FlexLayoutView>()
            .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
            .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
            .SetBackground(views::CreateThemedSolidBackground(
                cros_tokens::kCrosSysSystemBase))
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

TEST_F(SystemNudgeViewPixelTest, TextOnly) {
  // Set up base nudge data, which has an id, a catalog name and a body text.
  auto nudge_data = CreateBaseNudgeData();

  GetContentsView()->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/0, GetContentsView()));
}

TEST_F(SystemNudgeViewPixelTest, TextOnly_LongText) {
  // Set up base nudge data and set a long text.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.body_text = long_body_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/0, GetContentsView()));
}

TEST_F(SystemNudgeViewPixelTest, WithButtons) {
  // Set up base nudge data, set a long text and add buttons.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.body_text = long_body_text;
  nudge_data.primary_button_text = button_text;
  nudge_data.secondary_button_text = button_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/0, GetContentsView()));
}

TEST_F(SystemNudgeViewPixelTest, TitleAndLeadingImage) {
  // Set up base nudge data, set a long text, a title and a leading image.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kDogfoodIcon, cros_tokens::kCrosSysOnSurface,
      /*icon_size=*/60);
  nudge_data.title_text = title_text;
  nudge_data.body_text = long_body_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/0, GetContentsView()));
}

TEST_F(SystemNudgeViewPixelTest, TitleAndLeadingImageWithButtons) {
  // Set up base nudge data, set a long text, title, leading image and buttons.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kDogfoodIcon, cros_tokens::kCrosSysOnSurface,
      /*icon_size=*/60);
  nudge_data.title_text = title_text;
  nudge_data.body_text = long_body_text;
  nudge_data.primary_button_text = button_text;
  nudge_data.secondary_button_text = button_text;

  GetContentsView()->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/0, GetContentsView()));
}

TEST_F(SystemNudgeViewPixelTest, AnchoredNudgeWithPointyCorner) {
  // Set up base nudge data and add an anchor view.
  auto nudge_data = CreateBaseNudgeData();
  auto* anchor_view =
      GetContentsView()->AddChildView(std::make_unique<views::View>());
  nudge_data.SetAnchorView(anchor_view);
  // Set a corner-anchored arrow that will create a pointy corner.
  nudge_data.arrow = views::BubbleBorder::Arrow::BOTTOM_RIGHT;

  GetContentsView()->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "screenshot", /*revision_number=*/0, GetContentsView()));
}

}  // namespace ash
