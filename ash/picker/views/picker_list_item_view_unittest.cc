// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_list_item_view.h"

#include <string>
#include <utility>

#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/views/picker_badge_view.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
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

  item_view.SetPrimaryImage(ui::ImageModel());

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

TEST_F(PickerListItemViewTest, ClosesSubmenuOnEnter) {
  auto anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->SetContentsView(std::make_unique<views::View>());
  anchor_widget->Show();
  PickerSubmenuController submenu_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  item_view->SetPrimaryText(u"abc");
  item_view->SetSubmenuController(&submenu_controller);
  widget->Show();
  submenu_controller.Show(anchor_widget->GetContentsView(), {});

  item_view->OnMouseEntered(ui::MouseEvent(
      ui::EventType::kMouseMoved, gfx::PointF(), gfx::PointF(),
      /*time_stamp=*/{}, ui::EF_IS_SYNTHESIZED, ui::EF_LEFT_MOUSE_BUTTON));

  views::test::WidgetDestroyedWaiter(submenu_controller.widget_for_testing())
      .Wait();
}

TEST_F(PickerListItemViewTest, AccessibleNameUsesPrimaryText) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");

  EXPECT_EQ(view.GetAccessibleName(), u"primary");
}

TEST_F(PickerListItemViewTest, AccessibleNameUsesPrimaryAndSecondaryText) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");

  EXPECT_EQ(view.GetAccessibleName(), u"primary, secondary");
}

TEST_F(PickerListItemViewTest, AccessibleNameUsesPrimaryTextAndBadgeActionDo) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetBadgeAction(PickerActionType::kDo);

  EXPECT_EQ(view.GetAccessibleName(), u"primary");
}

TEST_F(PickerListItemViewTest,
       AccessibleNameUsesPrimaryTextAndBadgeActionInsert) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetBadgeAction(PickerActionType::kInsert);

  EXPECT_EQ(view.GetAccessibleName(), u"Insert primary");
}

TEST_F(PickerListItemViewTest,
       AccessibleNameUsesPrimaryTextAndBadgeActionOpen) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetBadgeAction(PickerActionType::kOpen);

  EXPECT_EQ(view.GetAccessibleName(), u"Open primary");
}

TEST_F(PickerListItemViewTest,
       AccessibleNameUsesPrimaryTextAndBadgeActionCreate) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetBadgeAction(PickerActionType::kCreate);

  EXPECT_EQ(view.GetAccessibleName(), u"primary");
}

TEST_F(PickerListItemViewTest,
       AccessibleNameUsesPrimaryAndSecondaryTextAndBadgeActionDo) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");
  view.SetBadgeAction(PickerActionType::kDo);

  EXPECT_EQ(view.GetAccessibleName(), u"primary, secondary");
}

TEST_F(PickerListItemViewTest,
       AccessibleNameUsesPrimaryAndSecondaryTextAndBadgeActionInsert) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");
  view.SetBadgeAction(PickerActionType::kInsert);

  EXPECT_EQ(view.GetAccessibleName(), u"Insert primary, secondary");
}

TEST_F(PickerListItemViewTest,
       AccessibleNameUsesPrimaryAndSecondaryTextAndBadgeActionOpen) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");
  view.SetBadgeAction(PickerActionType::kOpen);

  EXPECT_EQ(view.GetAccessibleName(), u"Open primary, secondary");
}

TEST_F(PickerListItemViewTest,
       AccessibleNameUsesPrimaryAndSecondaryTextAndBadgeActionCreate) {
  PickerListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");
  view.SetBadgeAction(PickerActionType::kCreate);

  EXPECT_EQ(view.GetAccessibleName(), u"primary, secondary");
}

}  // namespace
}  // namespace ash
