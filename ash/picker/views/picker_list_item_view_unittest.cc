// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_list_item_view.h"

#include <string>
#include <utility>

#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/views/picker_badge_view.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using ::testing::Property;
using ::testing::SizeIs;

class PickerListItemViewTest : public views::ViewsTestBase {
 private:
  AshColorProvider provider_;
};

TEST_F(PickerListItemViewTest, SetsPrimaryText) {
  PickerListItemView item_view(base::DoNothing());

  const std::u16string kPrimaryText = u"Item";
  item_view.SetPrimaryText(kPrimaryText);

  ASSERT_THAT(item_view.primary_container_for_testing()->children(), SizeIs(1));
  const views::View* primary_label =
      item_view.primary_container_for_testing()->children()[0];
  ASSERT_TRUE(views::IsViewClass<views::Label>(primary_label));
  EXPECT_THAT(views::AsViewClass<views::Label>(primary_label),
              Property(&views::Label::GetText, kPrimaryText));
}

TEST_F(PickerListItemViewTest, SetsPrimaryImage) {
  PickerListItemView item_view(base::DoNothing());

  item_view.SetPrimaryImage(std::make_unique<views::ImageView>());

  ASSERT_THAT(item_view.primary_container_for_testing()->children(), SizeIs(1));
  EXPECT_TRUE(views::IsViewClass<views::ImageView>(
      item_view.primary_container_for_testing()->children()[0]));
}

TEST_F(PickerListItemViewTest, SetsLeadingIcon) {
  PickerListItemView item_view(base::DoNothing());

  item_view.SetLeadingIcon(ui::ImageModel::FromVectorIcon(
      kImeMenuEmoticonIcon, cros_tokens::kCrosSysOnSurface));

  EXPECT_TRUE(
      item_view.leading_icon_view_for_testing().GetImageModel().IsVectorIcon());
}

TEST_F(PickerListItemViewTest, SetsBadgeVisible) {
  PickerListItemView item_view(base::DoNothing());

  item_view.SetBadgeVisible(true);

  EXPECT_TRUE(item_view.trailing_badge_for_testing().GetVisible());
}

TEST_F(PickerListItemViewTest, SetsBadgeNotVisible) {
  PickerListItemView item_view(base::DoNothing());

  item_view.SetBadgeVisible(false);

  EXPECT_FALSE(item_view.trailing_badge_for_testing().GetVisible());
}

TEST_F(PickerListItemViewTest, SetBadgeActionDoHasNoLabelText) {
  PickerListItemView item_view(base::DoNothing());

  item_view.SetBadgeAction(PickerActionType::kDo);

  EXPECT_EQ(item_view.trailing_badge_for_testing().GetText(), u"");
}

TEST_F(PickerListItemViewTest, SetBadgeActionHasLabelText) {
  PickerListItemView item_view(base::DoNothing());

  item_view.SetBadgeAction(PickerActionType::kInsert);
  EXPECT_NE(item_view.trailing_badge_for_testing().GetText(), u"");

  item_view.SetBadgeAction(PickerActionType::kOpen);
  EXPECT_NE(item_view.trailing_badge_for_testing().GetText(), u"");

  item_view.SetBadgeAction(PickerActionType::kCreate);
  EXPECT_NE(item_view.trailing_badge_for_testing().GetText(), u"");
}

TEST_F(PickerListItemViewTest, SetPreviewUpdatesIconWithPlaceholder) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  widget->Show();

  item_view->SetPreview(&preview_controller, base::FilePath(),
                        base::DoNothing(), /*update_icon=*/true);

  EXPECT_EQ(item_view->leading_icon_view_for_testing()
                .GetImageModel()
                .GetImage()
                .AsBitmap()
                .getColor(1, 1),
            SK_ColorTRANSPARENT);
}

TEST_F(PickerListItemViewTest, SetPreviewUpdatesIconOncePreviewIconResolves) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  widget->Show();
  base::RunLoop run_loop;
  SkBitmap bitmap = gfx::test::CreateBitmap(100, SK_ColorBLUE);

  item_view->SetPrimaryText(u"abc");
  item_view->SetPreview(
      &preview_controller, base::FilePath(),
      base::BindLambdaForTesting(
          [&](const base::FilePath& file_path, const gfx::Size& size,
              HoldingSpaceImage::BitmapCallback callback) {
            std::move(callback).Run(&bitmap, base::File::Error::FILE_OK);
            run_loop.Quit();
          }),
      /*update_icon=*/true);

  run_loop.Run();
  EXPECT_EQ(item_view->leading_icon_view_for_testing()
                .GetImageModel()
                .GetImage()
                .AsBitmap()
                .getColor(1, 1),
            SK_ColorBLUE);
}

}  // namespace
}  // namespace ash
