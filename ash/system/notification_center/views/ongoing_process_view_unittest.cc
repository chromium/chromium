// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/system/notification_center/views/ongoing_process_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

views::ImageView* GetIcon(views::View* notification_view) {
  return views::AsViewClass<views::ImageView>(
      notification_view->GetViewByID(VIEW_ID_ONGOING_PROCESS_ICON));
}

views::Label* GetTitleLabel(views::View* notification_view) {
  return views::AsViewClass<views::Label>(
      notification_view->GetViewByID(VIEW_ID_ONGOING_PROCESS_TITLE_LABEL));
}

views::Label* GetSubtitleLabel(views::View* notification_view) {
  return views::AsViewClass<views::Label>(notification_view->GetViewByID(
      VIEW_ID_ONGOING_PROCESS_SUBTITLE_LABEL));
}

PillButton* GetPillButton(views::View* notification_view) {
  return views::AsViewClass<PillButton>(
      notification_view->GetViewByID(VIEW_ID_ONGOING_PROCESS_PILL_BUTTON));
}

IconButton* GetPrimaryIconButton(views::View* notification_view) {
  return views::AsViewClass<IconButton>(notification_view->GetViewByID(
      VIEW_ID_ONGOING_PROCESS_PRIMARY_ICON_BUTTON));
}

IconButton* GetSecondaryIconButton(views::View* notification_view) {
  return views::AsViewClass<IconButton>(notification_view->GetViewByID(
      VIEW_ID_ONGOING_PROCESS_SECONDARY_ICON_BUTTON));
}

// Sample constants to use in the created test views.
const std::u16string sample_text = u"sample";
raw_ptr<const gfx::VectorIcon> sample_icon = &kPinnedIcon;

// Histogram names.
constexpr char kOngoingProcessShownWithoutIconCount[] =
    "Ash.NotifierFramework.PinnedSystemNotification.ShownWithoutIcon";
constexpr char kOngoingProcessShownWithoutTitleCount[] =
    "Ash.NotifierFramework.PinnedSystemNotification.ShownWithoutTitle";

}  // namespace

// Unit test to verify that `OngoingProcessView` elements are created by
// providing different parameters. Visit go/ongoing-processes-variations to
// access screenshots of the different view configurations.
using OngoingProcessViewTest = AshTestBase;

// Tests that the mandatory fields, which are title and icon, are created.
TEST_F(OngoingProcessViewTest, Default) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  auto notification =
      ash::SystemNotificationBuilder()
          .SetId("id")
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetSmallImage(*sample_icon)
          .SetTitle(sample_text)
          .Build(
              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that appropriate notification elements were created.
  EXPECT_TRUE(GetIcon(ongoing_process_view));
  ASSERT_TRUE(GetTitleLabel(ongoing_process_view));
  EXPECT_TRUE(GetTitleLabel(ongoing_process_view)->GetVisible());
  ASSERT_TRUE(GetSubtitleLabel(ongoing_process_view));
  EXPECT_FALSE(GetSubtitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_FALSE(GetPillButton(ongoing_process_view));
  EXPECT_FALSE(GetPrimaryIconButton(ongoing_process_view));
  EXPECT_FALSE(GetSecondaryIconButton(ongoing_process_view));
}

// Tests that the mandatory fields and a subtitle are created.
TEST_F(OngoingProcessViewTest, Subtitle) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  auto notification =
      ash::SystemNotificationBuilder()
          .SetId("id")
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetSmallImage(*sample_icon)
          .SetTitle(sample_text)
          .SetMessage(sample_text)
          .Build(
              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that appropriate notification elements were created.
  EXPECT_TRUE(GetIcon(ongoing_process_view));
  ASSERT_TRUE(GetTitleLabel(ongoing_process_view));
  EXPECT_TRUE(GetTitleLabel(ongoing_process_view)->GetVisible());
  ASSERT_TRUE(GetSubtitleLabel(ongoing_process_view));
  EXPECT_TRUE(GetSubtitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_FALSE(GetPillButton(ongoing_process_view));
  EXPECT_FALSE(GetPrimaryIconButton(ongoing_process_view));
  EXPECT_FALSE(GetSecondaryIconButton(ongoing_process_view));
}

// Tests that the mandatory fields and a pill button are created.
TEST_F(OngoingProcessViewTest, PillButton) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  message_center::RichNotificationData data;
  data.buttons.emplace_back(message_center::ButtonInfo(/*title=*/sample_text));

  auto notification =
      ash::SystemNotificationBuilder()
          .SetId("id")
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetSmallImage(*sample_icon)
          .SetTitle(sample_text)
          .SetOptionalFields(data)
          .Build(
              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that appropriate notification elements were created.
  EXPECT_TRUE(GetIcon(ongoing_process_view));
  EXPECT_TRUE(GetTitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_FALSE(GetSubtitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_TRUE(GetPillButton(ongoing_process_view));
  EXPECT_FALSE(GetPrimaryIconButton(ongoing_process_view));
  EXPECT_FALSE(GetSecondaryIconButton(ongoing_process_view));
}

// Tests that the mandatory fields and an icon button are created.
TEST_F(OngoingProcessViewTest, OneIconButton) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  message_center::RichNotificationData data;
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/sample_icon, /*accessible_name=*/sample_text));

  auto notification =
      ash::SystemNotificationBuilder()
          .SetId("id")
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetSmallImage(*sample_icon)
          .SetTitle(sample_text)
          .SetOptionalFields(data)
          .Build(
              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that appropriate notification elements were created.
  EXPECT_TRUE(GetIcon(ongoing_process_view));
  EXPECT_TRUE(GetTitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_FALSE(GetSubtitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_FALSE(GetPillButton(ongoing_process_view));
  EXPECT_TRUE(GetPrimaryIconButton(ongoing_process_view));
  EXPECT_FALSE(GetSecondaryIconButton(ongoing_process_view));
}

// Tests that the mandatory fields and two icon buttons are created.
TEST_F(OngoingProcessViewTest, TwoIconButtons) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  message_center::RichNotificationData data;
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/sample_icon, /*accessible_name=*/sample_text));
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/sample_icon, /*accessible_name=*/sample_text));

  auto notification =
      ash::SystemNotificationBuilder()
          .SetId("id")
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetSmallImage(*sample_icon)
          .SetTitle(sample_text)
          .SetOptionalFields(data)
          .Build(
              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that appropriate notification elements were created.
  EXPECT_TRUE(GetIcon(ongoing_process_view));
  EXPECT_TRUE(GetTitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_FALSE(GetSubtitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_FALSE(GetPillButton(ongoing_process_view));
  EXPECT_TRUE(GetPrimaryIconButton(ongoing_process_view));
  EXPECT_TRUE(GetSecondaryIconButton(ongoing_process_view));
}

// Tests the invalid case where two `ButtonInfo` objects are sent but one has a
// `title` to create a `PillButton` and the other only has a `vector_icon` and
// `accessible_name` to create an `IconButton`. In these cases, only the first
// button will be created and the second `ButtonInfo` object will be ignored.
TEST_F(OngoingProcessViewTest, InvalidButtonInfo) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  ash::SystemNotificationBuilder notification_builder;
  message_center::RichNotificationData data;

  // Provide the data to create an `IconButton` and then a `PillButton`.
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/sample_icon, /*accessible_name=*/sample_text));
  data.buttons.emplace_back(message_center::ButtonInfo(/*title=*/sample_text));

  auto notification =
      notification_builder.SetId("id")
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetSmallImage(*sample_icon)
          .SetTitle(sample_text)
          .SetOptionalFields(data)
          .Build(
              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that only a single `IconButton` was created.
  EXPECT_FALSE(GetPillButton(ongoing_process_view));
  EXPECT_TRUE(GetPrimaryIconButton(ongoing_process_view));
  EXPECT_FALSE(GetSecondaryIconButton(ongoing_process_view));

  // Provide the data to create a `PillButton` and then an `IconButton`.
  data.buttons.clear();
  data.buttons.emplace_back(message_center::ButtonInfo(/*title=*/sample_text));
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/sample_icon, /*accessible_name=*/sample_text));

  notification = notification_builder.SetId("id")
                     .SetCatalogName(NotificationCatalogName::kTestCatalogName)
                     .SetSmallImage(*sample_icon)
                     .SetTitle(sample_text)
                     .SetOptionalFields(data)
                     .Build(
                         /*keep_timestamp=*/false);

  ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that only a single `PillButton` was created.
  EXPECT_TRUE(GetPillButton(ongoing_process_view));
  EXPECT_FALSE(GetPrimaryIconButton(ongoing_process_view));
  EXPECT_FALSE(GetSecondaryIconButton(ongoing_process_view));
}

// Tests that the `Ash.NotifierFramework.PinnedSystemNotification`
// `ShownWithoutIcon` and `ShownWithoutTitle` metrics properly record when a
// pinned notification view is created without an icon or title.
TEST_F(OngoingProcessViewTest, ShownWithoutIconOrTitleMetrics) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  auto catalog_name = NotificationCatalogName::kTestCatalogName;

  auto notification = ash::SystemNotificationBuilder()
                          .SetId("id")
                          .SetCatalogName(catalog_name)
                          .Build(
                              /*keep_timestamp=*/false);

  widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that the appropriate metrics were recorded.
  histogram_tester.ExpectBucketCount(kOngoingProcessShownWithoutIconCount,
                                     catalog_name, 1);
  histogram_tester.ExpectBucketCount(kOngoingProcessShownWithoutTitleCount,
                                     catalog_name, 1);
}

// Tests that the notification `title` and `subtitle` fields can be updated.
TEST_F(OngoingProcessViewTest, UpdateWithNotification) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  auto catalog_name = NotificationCatalogName::kTestCatalogName;

  auto notification = ash::SystemNotificationBuilder()
                          .SetId("id")
                          .SetCatalogName(catalog_name)
                          .SetSmallImage(*sample_icon)
                          .SetTitle(sample_text)
                          .SetMessage(sample_text)
                          .Build(
                              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that appropriate notification elements were created.
  EXPECT_TRUE(GetTitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_TRUE(GetSubtitleLabel(ongoing_process_view)->GetVisible());

  // Set an empty `title` and `subtitle` and update the notification view.
  notification.set_title(std::u16string());
  notification.set_message(std::u16string());
  ongoing_process_view->UpdateWithNotification(notification);

  // Ensure that the notification elements's visibility was properly updated.
  EXPECT_TRUE(GetTitleLabel(ongoing_process_view)->GetVisible());
  EXPECT_FALSE(GetSubtitleLabel(ongoing_process_view)->GetVisible());

  // Ensure the `ShownWithoutTitle` metric was recorded after updating the
  // notification with an empty `title`.
  histogram_tester.ExpectBucketCount(kOngoingProcessShownWithoutTitleCount,
                                     catalog_name, 1);
}

// Tests that the notification pill button can be updated with a new text.
TEST_F(OngoingProcessViewTest, UpdateWithNotification_PillButton) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Create a notification with a pill button.
  message_center::RichNotificationData data;
  data.buttons.emplace_back(message_center::ButtonInfo(/*title=*/sample_text));

  auto notification =
      ash::SystemNotificationBuilder()
          .SetId("id")
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetSmallImage(*sample_icon)
          .SetTitle(sample_text)
          .SetMessage(sample_text)
          .SetOptionalFields(data)
          .Build(
              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  // Test that the pill buttons was created with the proper text.
  auto* pill_button = GetPillButton(ongoing_process_view);
  EXPECT_TRUE(pill_button->GetVisible());
  EXPECT_EQ(pill_button->GetText(), sample_text);

  // Set a new text for the pill button.
  std::u16string updated_text = u"updated";
  data.buttons.clear();
  data.buttons.emplace_back(message_center::ButtonInfo(/*title=*/updated_text));
  notification.set_buttons(data.buttons);
  ongoing_process_view->UpdateWithNotification(notification);

  // Ensure that the icon buttons is still visible and with the updated text.
  EXPECT_TRUE(pill_button->GetVisible());
  EXPECT_EQ(pill_button->GetText(), updated_text);

  // Remove the text for the pill buttons.
  data.buttons.clear();
  notification.set_buttons(data.buttons);
  ongoing_process_view->UpdateWithNotification(notification);

  // The pill button should still be visible, as the buttons cannot be removed.
  EXPECT_TRUE(pill_button->GetVisible());
  EXPECT_EQ(pill_button->GetText(), updated_text);
}

// Tests that the notification buttons can be updated with new icons.
TEST_F(OngoingProcessViewTest, UpdateWithNotification_IconButtons) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Create a notification with two icons.
  message_center::RichNotificationData data;
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/sample_icon, /*accessible_name=*/sample_text));
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/sample_icon, /*accessible_name=*/sample_text));

  auto notification =
      ash::SystemNotificationBuilder()
          .SetId("id")
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetSmallImage(*sample_icon)
          .SetTitle(sample_text)
          .SetMessage(sample_text)
          .SetOptionalFields(data)
          .Build(
              /*keep_timestamp=*/false);

  OngoingProcessView* ongoing_process_view = widget->SetContentsView(
      std::make_unique<OngoingProcessView>(notification));

  auto* primary_button = GetPrimaryIconButton(ongoing_process_view);
  auto* secondary_button = GetSecondaryIconButton(ongoing_process_view);

  // Test that the icon buttons were created.
  EXPECT_TRUE(primary_button);
  EXPECT_TRUE(secondary_button);

  // Set new icons for the icon buttons.
  raw_ptr<const gfx::VectorIcon> updated_icon = &kPlaceholderAppIcon;
  data.buttons.clear();
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/updated_icon, /*accessible_name=*/sample_text));
  data.buttons.emplace_back(message_center::ButtonInfo(
      /*vector_icon=*/updated_icon, /*accessible_name=*/sample_text));
  notification.set_buttons(data.buttons);
  ongoing_process_view->UpdateWithNotification(notification);

  // Ensure that the icon buttons are still visible.
  EXPECT_TRUE(primary_button);
  EXPECT_TRUE(secondary_button);

  // Remove the icons for the icon buttons.
  data.buttons.clear();
  notification.set_buttons(data.buttons);
  ongoing_process_view->UpdateWithNotification(notification);

  // The icon buttons should still be visible, as the buttons cannot be removed.
  EXPECT_TRUE(primary_button);
  EXPECT_TRUE(secondary_button);
}

}  // namespace ash
