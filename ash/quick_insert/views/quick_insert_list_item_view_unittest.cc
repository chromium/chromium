// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_list_item_view.h"

#include <string>
#include <utility>

#include "ash/quick_insert/model/quick_insert_action_type.h"
#include "ash/quick_insert/views/quick_insert_badge_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"
#include "ash/quick_insert/views/quick_insert_shortcut_hint_view.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
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

base::OnceCallback<std::optional<base::File::Info>()> GetNulloptFileInfo() {
  return base::ReturnValueOnce<std::optional<base::File::Info>>(std::nullopt);
}

base::OnceCallback<std::optional<base::File::Info>()> GetFileInfoCallback(
    base::File::Info file_info) {
  return base::ReturnValueOnce<std::optional<base::File::Info>>(file_info);
}

class QuickInsertListItemViewTest : public views::ViewsTestBase {
 private:
  AshColorProvider provider_;
};

TEST_F(QuickInsertListItemViewTest, SetsPrimaryText) {
  QuickInsertListItemView item_view(base::DoNothing());

  const std::u16string kPrimaryText = u"Item";
  item_view.SetPrimaryText(kPrimaryText);

  ASSERT_THAT(item_view.primary_container_for_testing()->children(), SizeIs(1));
  const views::View* primary_label =
      item_view.primary_container_for_testing()->children()[0];
  ASSERT_TRUE(views::IsViewClass<views::Label>(primary_label));
  EXPECT_THAT(views::AsViewClass<views::Label>(primary_label),
              Property(&views::Label::GetText, kPrimaryText));
}

TEST_F(QuickInsertListItemViewTest, SetsPrimaryImage) {
  QuickInsertListItemView item_view(base::DoNothing());

  item_view.SetPrimaryImage(ui::ImageModel(), /*available_width=*/100);

  ASSERT_THAT(item_view.primary_container_for_testing()->children(), SizeIs(1));
  EXPECT_TRUE(views::IsViewClass<views::ImageView>(
      item_view.primary_container_for_testing()->children()[0]));
}

TEST_F(QuickInsertListItemViewTest, SetPrimaryImageScalesImage) {
  QuickInsertListItemView item_view(base::DoNothing());
  item_view.SetPrimaryImage(
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(1)),
      /*available_width=*/320);

  ASSERT_THAT(item_view.primary_container_for_testing()->children(), SizeIs(1));
  ASSERT_TRUE(views::IsViewClass<views::ImageView>(
      item_view.primary_container_for_testing()->children()[0]));
  EXPECT_EQ(views::AsViewClass<views::ImageView>(
                item_view.primary_container_for_testing()->children()[0])
                ->GetImageBounds()
                .size(),
            gfx::Size(252, 64));
}

TEST_F(QuickInsertListItemViewTest, SetsLeadingIcon) {
  QuickInsertListItemView item_view(base::DoNothing());

  item_view.SetLeadingIcon(ui::ImageModel::FromVectorIcon(
      kImeMenuEmoticonIcon, cros_tokens::kCrosSysOnSurface));

  EXPECT_TRUE(
      item_view.leading_icon_view_for_testing().GetImageModel().IsVectorIcon());
}

TEST_F(QuickInsertListItemViewTest, SetsShortcutHintView) {
  QuickInsertListItemView item_view(base::DoNothing());

  item_view.SetShortcutHintView(std::make_unique<PickerShortcutHintView>(
      QuickInsertCapsLockResult::Shortcut::kAltSearch));

  EXPECT_NE(item_view.shortcut_hint_view_for_testing(), nullptr);
}

TEST_F(QuickInsertListItemViewTest, SetsBadgeVisible) {
  QuickInsertListItemView item_view(base::DoNothing());

  item_view.SetBadgeVisible(true);

  EXPECT_TRUE(item_view.trailing_badge_for_testing().GetVisible());
}

TEST_F(QuickInsertListItemViewTest, SetsBadgeNotVisible) {
  QuickInsertListItemView item_view(base::DoNothing());

  item_view.SetBadgeVisible(false);

  EXPECT_FALSE(item_view.trailing_badge_for_testing().GetVisible());
}

TEST_F(QuickInsertListItemViewTest, SetsBadgeVisibleWithPrimaryText) {
  QuickInsertListItemView item_view(base::DoNothing());
  item_view.SetPrimaryText(u"a");

  item_view.SetBadgeVisible(true);

  EXPECT_TRUE(item_view.trailing_badge_for_testing().GetVisible());
}

TEST_F(QuickInsertListItemViewTest, DoesNotSetBadgeVisibleWithPrimaryImage) {
  QuickInsertListItemView item_view(base::DoNothing());
  item_view.SetPrimaryImage(ui::ImageModel(), /*available_width=*/100);

  item_view.SetBadgeVisible(true);

  EXPECT_FALSE(item_view.trailing_badge_for_testing().GetVisible());
}

TEST_F(QuickInsertListItemViewTest, SetBadgeActionDoHasNoLabelText) {
  QuickInsertListItemView item_view(base::DoNothing());

  item_view.SetBadgeAction(QuickInsertActionType::kDo);

  EXPECT_EQ(item_view.trailing_badge_for_testing().GetText(), u"");
}

TEST_F(QuickInsertListItemViewTest, SetBadgeActionHasLabelText) {
  QuickInsertListItemView item_view(base::DoNothing());

  item_view.SetBadgeAction(QuickInsertActionType::kInsert);
  EXPECT_NE(item_view.trailing_badge_for_testing().GetText(), u"");

  item_view.SetBadgeAction(QuickInsertActionType::kOpen);
  EXPECT_NE(item_view.trailing_badge_for_testing().GetText(), u"");

  item_view.SetBadgeAction(QuickInsertActionType::kCreate);
  EXPECT_NE(item_view.trailing_badge_for_testing().GetText(), u"");
}

TEST_F(QuickInsertListItemViewTest, SetPreviewUpdatesIconWithPlaceholder) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  widget->Show();

  item_view->SetPreview(&preview_controller, base::NullCallback(),
                        base::FilePath(), base::DoNothing(),
                        /*update_icon=*/true);

  EXPECT_EQ(item_view->leading_icon_view_for_testing()
                .GetImageModel()
                .GetImage()
                .AsBitmap()
                .getColor(1, 1),
            SK_ColorTRANSPARENT);
}

TEST_F(QuickInsertListItemViewTest,
       SetPreviewUpdatesIconOncePreviewIconResolves) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  widget->Show();
  base::RunLoop run_loop;
  SkBitmap bitmap = gfx::test::CreateBitmap(100, SK_ColorBLUE);

  item_view->SetPrimaryText(u"abc");
  item_view->SetPreview(
      &preview_controller, base::NullCallback(), base::FilePath(),
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

TEST_F(QuickInsertListItemViewTest, SetPreviewResolvesFileInfo) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  widget->Show();
  base::test::TestFuture<void> file_info_future;

  item_view->SetPreview(
      &preview_controller,
      file_info_future.GetSequenceBoundCallback().Then(GetNulloptFileInfo()),
      base::FilePath(), base::DoNothing(), /*update_icon=*/true);

  EXPECT_TRUE(file_info_future.Wait());
}

TEST_F(QuickInsertListItemViewTest,
       PseudofocusHidesLabelsBeforeFileInfoResolves) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  item_view->SetPrimaryText(u"abc");
  widget->Show();
  base::test::TestFuture<void> file_info_future;

  item_view->SetPreview(
      &preview_controller,
      file_info_future.GetSequenceBoundCallback().Then(GetNulloptFileInfo()),
      base::FilePath(), base::DoNothing(), /*update_icon=*/true);
  item_view->SetItemState(QuickInsertItemView::ItemState::kPseudoFocused);

  PickerPreviewBubbleView* bubble_view =
      preview_controller.bubble_view_for_testing();
  ASSERT_FALSE(file_info_future.IsReady());
  ViewDrawnWaiter().Wait(bubble_view);
  EXPECT_FALSE(bubble_view->GetLabelVisibleForTesting());
}

TEST_F(QuickInsertListItemViewTest,
       PseudofocusHidesPreviewLabelsAfterFileInfoResolvesWithNullFileInfo) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  item_view->SetPrimaryText(u"abc");
  widget->Show();
  base::test::TestFuture<void> file_info_future;

  item_view->SetPreview(
      &preview_controller,
      file_info_future.GetSequenceBoundCallback().Then(
          base::ReturnValueOnce<std::optional<base::File::Info>>(
              base::File::Info())),
      base::FilePath(), base::DoNothing(), /*update_icon=*/true);
  item_view->SetItemState(QuickInsertItemView::ItemState::kPseudoFocused);

  PickerPreviewBubbleView* bubble_view =
      preview_controller.bubble_view_for_testing();
  ASSERT_TRUE(file_info_future.Wait()) << "File info was never resolved";
  // `GetSequenceBoundCallback` allows this sequence to know when the callback
  // is called by the task runner... but it doesn't allow this sequence to know
  // when the *reply* is run (in this sequence). This `RunUntilIdle` is required
  // to ensure that the reply is run.
  base::RunLoop().RunUntilIdle();
  ViewDrawnWaiter().Wait(bubble_view);
  EXPECT_FALSE(bubble_view->GetLabelVisibleForTesting());
}

TEST_F(QuickInsertListItemViewTest,
       PseudofocusShowsPreviewLabelsWithValidFileInfo) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  item_view->SetPrimaryText(u"abc");
  widget->Show();
  base::File::Info only_modified;
  EXPECT_TRUE(base::Time::FromString("23 Dec 2021 09:01:00",
                                     &only_modified.last_modified));
  base::test::TestFuture<void> file_info_future;

  item_view->SetPreview(&preview_controller,
                        file_info_future.GetSequenceBoundCallback().Then(
                            GetFileInfoCallback(only_modified)),
                        base::FilePath(), base::DoNothing(),
                        /*update_icon=*/true);
  item_view->SetItemState(QuickInsertItemView::ItemState::kPseudoFocused);

  PickerPreviewBubbleView* bubble_view =
      preview_controller.bubble_view_for_testing();
  ASSERT_TRUE(file_info_future.Wait()) << "File info was never resolved";
  base::RunLoop().RunUntilIdle();
  ViewDrawnWaiter().Wait(bubble_view);
  EXPECT_TRUE(bubble_view->GetLabelVisibleForTesting());
  EXPECT_EQ(bubble_view->GetMainTextForTesting(), u"Edited · Dec 23");
}

TEST_F(QuickInsertListItemViewTest,
       PseudofocusShowsPreviewUsingCachedFileInfo) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  item_view->SetPrimaryText(u"abc");
  widget->Show();
  base::File::Info only_modified;
  EXPECT_TRUE(base::Time::FromString("23 Dec 2021 09:01:00",
                                     &only_modified.last_modified));
  base::test::TestFuture<void> file_info_future;
  item_view->SetPreview(&preview_controller,
                        file_info_future.GetSequenceBoundCallback().Then(
                            GetFileInfoCallback(only_modified)),
                        base::FilePath(), base::DoNothing(),
                        /*update_icon=*/true);
  item_view->SetItemState(QuickInsertItemView::ItemState::kPseudoFocused);
  PickerPreviewBubbleView* bubble_view =
      preview_controller.bubble_view_for_testing();
  ASSERT_TRUE(file_info_future.Wait()) << "File info was never resolved";
  base::RunLoop().RunUntilIdle();
  ViewDrawnWaiter().Wait(bubble_view);
  item_view->SetItemState(QuickInsertItemView::ItemState::kNormal);

  item_view->SetItemState(QuickInsertItemView::ItemState::kPseudoFocused);

  EXPECT_TRUE(bubble_view->GetLabelVisibleForTesting());
  EXPECT_EQ(bubble_view->GetMainTextForTesting(), u"Edited · Dec 23");
}

TEST_F(QuickInsertListItemViewTest, ClosesPreviewBubbleAfterLosingPseudoFocus) {
  PickerPreviewBubbleController preview_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  item_view->SetPrimaryText(u"abc");
  widget->Show();
  base::test::TestFuture<void> file_info_future;
  item_view->SetPreview(&preview_controller, base::NullCallback(),
                        base::FilePath(), base::DoNothing(),
                        /*update_icon=*/true);
  item_view->SetItemState(QuickInsertItemView::ItemState::kPseudoFocused);

  item_view->SetItemState(QuickInsertItemView::ItemState::kNormal);

  EXPECT_EQ(preview_controller.bubble_view_for_testing(), nullptr);
}

TEST_F(QuickInsertListItemViewTest, ClosesSubmenuOnEnter) {
  auto anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->SetContentsView(std::make_unique<views::View>());
  anchor_widget->Show();
  PickerSubmenuController submenu_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
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

TEST_F(QuickInsertListItemViewTest, AccessibleNameUsesPrimaryText) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");

  EXPECT_EQ(view.GetAccessibleName(), u"primary");
}

TEST_F(QuickInsertListItemViewTest, AccessibleNameUsesPrimaryAndSecondaryText) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");

  EXPECT_EQ(view.GetAccessibleName(), u"primary, secondary");
}

TEST_F(QuickInsertListItemViewTest,
       AccessibleNameUsesPrimaryTextAndBadgeActionDo) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetBadgeAction(QuickInsertActionType::kDo);

  EXPECT_EQ(view.GetAccessibleName(), u"primary");
}

TEST_F(QuickInsertListItemViewTest,
       AccessibleNameUsesPrimaryTextAndBadgeActionInsert) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetBadgeAction(QuickInsertActionType::kInsert);

  EXPECT_EQ(view.GetAccessibleName(), u"Insert primary");
}

TEST_F(QuickInsertListItemViewTest,
       AccessibleNameUsesPrimaryTextAndBadgeActionOpen) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetBadgeAction(QuickInsertActionType::kOpen);

  EXPECT_EQ(view.GetAccessibleName(), u"Open primary");
}

TEST_F(QuickInsertListItemViewTest,
       AccessibleNameUsesPrimaryTextAndBadgeActionCreate) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetBadgeAction(QuickInsertActionType::kCreate);

  EXPECT_EQ(view.GetAccessibleName(), u"primary");
}

TEST_F(QuickInsertListItemViewTest,
       AccessibleNameUsesPrimaryAndSecondaryTextAndBadgeActionDo) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");
  view.SetBadgeAction(QuickInsertActionType::kDo);

  EXPECT_EQ(view.GetAccessibleName(), u"primary, secondary");
}

TEST_F(QuickInsertListItemViewTest,
       AccessibleNameUsesPrimaryAndSecondaryTextAndBadgeActionInsert) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");
  view.SetBadgeAction(QuickInsertActionType::kInsert);

  EXPECT_EQ(view.GetAccessibleName(), u"Insert primary, secondary");
}

TEST_F(QuickInsertListItemViewTest,
       AccessibleNameUsesPrimaryAndSecondaryTextAndBadgeActionOpen) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");
  view.SetBadgeAction(QuickInsertActionType::kOpen);

  EXPECT_EQ(view.GetAccessibleName(), u"Open primary, secondary");
}

TEST_F(QuickInsertListItemViewTest,
       AccessibleNameUsesPrimaryAndSecondaryTextAndBadgeActionCreate) {
  QuickInsertListItemView view(base::DoNothing());
  view.SetPrimaryText(u"primary");
  view.SetSecondaryText(u"secondary");
  view.SetBadgeAction(QuickInsertActionType::kCreate);

  EXPECT_EQ(view.GetAccessibleName(), u"primary, secondary");
}

}  // namespace
}  // namespace ash
