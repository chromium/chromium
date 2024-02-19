// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/message_popup_animation_waiter.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/notification_control_buttons_view.h"

namespace ash {

namespace {

constexpr char kShortTitleString[] = "Short Title";
constexpr char kMediumTitleString[] = "Test Notification's Multiline Title";
constexpr char kLongTitleString[] =
    "Test Notification's Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Long Multiline Title";
constexpr char kLongMessageString[] =
    "Test Notification's Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Long Message";

constexpr char kShortTitleScreenshot[] = "ash_notification_short_title";
constexpr char kMediumTitleScreenshot[] =
    "ash_notification_multiline_medium_title";
constexpr char kLongTitleScreenshot[] = "ash_notification_multiline_long_title";

const ui::ImageModel test_green_icon = ui::ImageModel::FromImageSkia(
    CreateSolidColorTestImage(gfx::Size(/*width=*/48, /*height=*/48),
                              SK_ColorGREEN));

}  // namespace

// Pixel tests for Chrome OS Notification views.
class AshNotificationViewPixelTest : public AshTestBase {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    test_api_ = std::make_unique<NotificationCenterTestApi>();
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

 private:
  std::unique_ptr<NotificationCenterTestApi> test_api_;
};

// Tests that a notification's close button is visible when it is focused.
TEST_F(AshNotificationViewPixelTest, CloseButtonFocused) {
  // Create a notification and open the notification center bubble to view it.
  const auto id = test_api()->AddNotification();
  test_api()->ToggleBubble();

  // Verify that the close button is neither focused nor visible. Note that the
  // close button, as a `views::ImageButton`, will actually be visible in the
  // sense of `views::View::GetVisible()`, but its parent's `ui::Layer` will
  // have an opacity of zero, making it visually invisible.
  auto* notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(id));
  auto* control_buttons_layer =
      notification_view->GetControlButtonsView()->layer();
  auto* close_button =
      notification_view->GetControlButtonsView()->close_button();
  EXPECT_EQ(control_buttons_layer->opacity(), 0);
  EXPECT_FALSE(close_button->HasFocus());

  // Move focus to the close button.
  close_button->GetWidget()->widget_delegate()->SetCanActivate(true);
  close_button->RequestFocus();

  // Verify, with both an assertion and a pixel test, that the close button has
  // focus and is visible.
  EXPECT_TRUE(close_button->HasFocus());
  EXPECT_EQ(control_buttons_layer->opacity(), 1);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "close_button_focused", /*revision_number=*/5, notification_view));
}

// Regression test for http://b/267195370. Tests that a notification with no
// message has its title vertically centered in the collapsed state.
TEST_F(AshNotificationViewPixelTest, CollapsedNoMessage) {
  // Create a notification with no message, and open the notification center
  // bubble to view it.
  const std::string id = test_api()->AddCustomNotification(
      u"Notification title", u"", test_green_icon);
  test_api()->ToggleBubble();

  // Make sure the notification is collapsed.
  auto* notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(id));
  notification_view->SetExpanded(false);
  ASSERT_FALSE(notification_view->IsExpanded());

  // Verify with a pixel test that the notification's title is vertically
  // centered.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "collapsed_no_message", /*revision_number=*/7, notification_view));
}

// Tests that a progress notification does not have its title vertically
// centered in the collapsed state.
TEST_F(AshNotificationViewPixelTest, ProgressCollapsed) {
  // Create a progress notification and open the notification center bubble to
  // view it. Also add a second notification so that the progress notification
  // is automatically in its collapsed state when the bubble is toggled.
  const std::string id = test_api()->AddProgressNotification();
  test_api()->AddNotification();
  test_api()->ToggleBubble();

  // Verify that the notification is collapsed.
  auto* notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(id));
  ASSERT_FALSE(notification_view->IsExpanded());

  // Verify with a pixel test that the notification's title is not vertically
  // centered.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "progress_collapsed", /*revision_number=*/5, notification_view));
}

// Tests the control buttons UI for the case of a notification with just the
// close button.
TEST_F(AshNotificationViewPixelTest, CloseControlButton) {
  // Generate a notification that should show just the close control button.
  // Also toggle the notification bubble so that the notification doesn't
  // disappear during the test.
  const std::string id = test_api()->AddNotification();
  test_api()->ToggleBubble();

  // Hover the mouse over the notification so that the close control button is
  // visible when taking a screenshot.
  auto* notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(id));
  GetEventGenerator()->MoveMouseTo(
      notification_view->GetBoundsInScreen().CenterPoint(), /*count=*/10);

  // Verify with a pixel test that the close control button is visible and has
  // the proper placement.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "close_control_button", /*revision_number=*/4, notification_view));
}

// Tests the control buttons UI for the case of a notification with both the
// settings and close buttons.
TEST_F(AshNotificationViewPixelTest, SettingsAndCloseControlButtons) {
  // Generate a notification that should show both the settings and close
  // control buttons. Also toggle the notification bubble so that the
  // notification doesn't disappear during the test.
  const std::string id = test_api()->AddNotificationWithSettingsButton();
  test_api()->ToggleBubble();

  // Hover the mouse over the notification so that the control buttons are
  // visible when taking a screenshot.
  auto* notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(id));
  GetEventGenerator()->MoveMouseTo(
      notification_view->GetBoundsInScreen().CenterPoint(), /*count=*/10);

  // Verify with a pixel test that the control buttons are visible and have
  // proper spacing between them.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "settings_and_close_control_buttons", /*revision_number=*/4,
      notification_view));
}

// Tests that a notification's icon is sized and positioned correctly at
// different sizes.
class AshNotificationViewIconPixelTest
    : public AshNotificationViewPixelTest,
      public testing::WithParamInterface<int> {
 public:
  int GetIconSize() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(IconTest,
                         AshNotificationViewIconPixelTest,
                         testing::ValuesIn({
                             16,
                             32,
                             128,
                             512,
                         }));

TEST_P(AshNotificationViewIconPixelTest, NotificationIcon) {
  int size = GetIconSize();
  // Create a notification with an icon with the given `size`.
  const std::string id = test_api()->AddCustomNotification(
      u"Notification title", u"Notification message",
      ui::ImageModel::FromImageSkia(CreateSolidColorTestImage(
          gfx::Size(/*width=*/size, /*height=*/size), SK_ColorGREEN)));

  test_api()->ToggleBubble();

  // Make sure the notification is expanded.
  auto* notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(id));
  ASSERT_TRUE(notification_view->IsExpanded());

  // Verify with a pixel test that the notification's title is vertically
  // centered.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      base::StringPrintf("expanded_icon_size_%u", size),
      /*revision_number=*/1, notification_view));

  notification_view->ToggleExpand();
  ASSERT_FALSE(notification_view->IsExpanded());

  notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(id));
  // Verify with a pixel test that the notification's title is vertically
  // centered.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      base::StringPrintf("collapsed_icon_size_%u", size),
      /*revision_number=*/1, notification_view));
}

class AshNotificationViewTitlePixelTest
    : public AshNotificationViewPixelTest,
      public testing::WithParamInterface<
          std::pair<const char* /*notification title string*/,
                    const char* /*screenshot name*/>> {
 public:
  const std::string GetTitle() { return GetParam().first; }

  const std::string GetScreenshotName() { return GetParam().second; }
};

INSTANTIATE_TEST_SUITE_P(
    TitleTest,
    AshNotificationViewTitlePixelTest,
    testing::ValuesIn({
        std::make_pair(kShortTitleString, kShortTitleScreenshot),
        std::make_pair(kMediumTitleString, kMediumTitleScreenshot),
        std::make_pair(kLongTitleString, kLongTitleScreenshot),
    }));

// Regression test for b/251686063. Tests that a notification with a medium
// length multiline title and an icon is correctly displayed. This string would
// not be displayed properly without the workaround implemented for b/251686063.
TEST_P(AshNotificationViewTitlePixelTest, NotificationTitleTest) {
  // Create a notification with a multiline title and an icon.
  const std::string title = GetTitle();

  const std::string id = test_api()->AddCustomNotification(
      base::UTF8ToUTF16(title), u"Notification Content", test_green_icon);

  test_api()->ToggleBubble();

  // Make sure the notification view exists and is visible.
  message_center::MessageView* notification_view =
      test_api()->GetNotificationViewForId(id);
  ASSERT_TRUE(notification_view);
  EXPECT_TRUE(notification_view->GetVisible());

  // Compare pixels.
  const std::string screenshot_name = GetScreenshotName();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      screenshot_name, /*revision_number=*/10, notification_view));
}

class AshNotificationViewCollapsedLongTextPixelTest
    : public AshNotificationViewPixelTest,
      public testing::WithParamInterface<
          std::tuple<bool /*whether there is an icon*/,
                     bool /*whether there is a settings control button*/>> {
 public:
  bool HasIcon() { return std::get<0>(GetParam()); }
  bool HasSettingsControlButton() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AshNotificationViewCollapsedLongTextPixelTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests the spacing between long, elided title/message text content and the
// next element of the notification (either icon or expand/collapse button).
// Also parameterized by the presence/absence of the settings control button.
TEST_P(AshNotificationViewCollapsedLongTextPixelTest, ElidedTextSpacing) {
  // Generate a notification with a long title and message, and view it in the
  // notification center. Also add a second notification so that the main
  // notification is automatically in its collapsed state when the bubble is
  // toggled.
  message_center::RichNotificationData optional_fields;
  if (HasSettingsControlButton()) {
    optional_fields.settings_button_handler =
        message_center::SettingsButtonHandler::DELEGATE;
  }
  const std::string id = test_api()->AddCustomNotification(
      base::UTF8ToUTF16(std::string(kLongTitleString)),
      base::UTF8ToUTF16(std::string(kLongMessageString)),
      /*icon=*/HasIcon() ? test_green_icon : ui::ImageModel(),
      /*display_source=*/u"", /*url=*/GURL(),
      /*notifier_id=*/message_center::NotifierId(), optional_fields);
  test_api()->AddNotification();
  test_api()->ToggleBubble();

  // Verify that the notification is collapsed.
  auto* notification_view = static_cast<AshNotificationView*>(
      test_api()->GetNotificationViewForId(id));
  ASSERT_FALSE(notification_view->IsExpanded());

  // Hover the mouse over the notification so that the control buttons are
  // visible when taking a screenshot.
  GetEventGenerator()->MoveMouseTo(
      notification_view->GetBoundsInScreen().CenterPoint(), /*count=*/10);

  // Verify the spacing with a pixel test.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "elided_text_spacing", /*revision_number=*/3, notification_view));
}

}  // namespace ash
