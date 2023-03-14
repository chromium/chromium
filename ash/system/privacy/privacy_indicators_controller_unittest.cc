// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_controller.h"

#include <string>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/notification_center/notification_list_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

namespace {

class TestDelegate : public PrivacyIndicatorsNotificationDelegate {
 public:
  explicit TestDelegate(bool has_launch_app_callback = true,
                        bool has_launch_settings_callback = true) {
    if (has_launch_app_callback) {
      SetLaunchAppCallback(base::BindRepeating(
          &TestDelegate::LaunchApp, weak_pointer_factory_.GetWeakPtr()));
    }
    if (has_launch_settings_callback) {
      SetLaunchSettingsCallback(
          base::BindRepeating(&TestDelegate::LaunchAppSettings,
                              weak_pointer_factory_.GetWeakPtr()));
    }
  }

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  void LaunchApp() { launch_app_called_ = true; }
  void LaunchAppSettings() { launch_settings_called_ = true; }

  bool launch_app_called() { return launch_app_called_; }
  bool launch_settings_called() { return launch_settings_called_; }

 private:
  ~TestDelegate() override = default;

  bool launch_app_called_ = false;
  bool launch_settings_called_ = false;

  base::WeakPtrFactory<TestDelegate> weak_pointer_factory_{this};
};

}  // namespace

class PrivacyIndicatorsControllerTest : public AshTestBase {
 public:
  PrivacyIndicatorsControllerTest() = default;
  PrivacyIndicatorsControllerTest(const PrivacyIndicatorsControllerTest&) =
      delete;
  PrivacyIndicatorsControllerTest& operator=(
      const PrivacyIndicatorsControllerTest&) = delete;
  ~PrivacyIndicatorsControllerTest() override = default;

  // Get the notification view from message center associated with `id`.
  message_center::NotificationViewBase* GetNotificationViewFromMessageCenter(
      const std::string& id) {
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    auto* view = GetPrimaryUnifiedSystemTray()
                     ->message_center_bubble()
                     ->notification_center_view()
                     ->notification_list_view()
                     ->GetMessageViewForNotificationId(id);
    auto* notification_view =
        static_cast<message_center::NotificationViewBase*>(view);
    EXPECT_TRUE(notification_view);
    return notification_view;
  }

  // Get the popup notification view associated with `id`.
  views::View* GetPopupNotificationView(const std::string& id) {
    return GetPrimaryUnifiedSystemTray()
        ->GetMessagePopupCollection()
        ->GetMessageViewForNotificationId(id);
  }

  void ClickView(message_center::NotificationViewBase* view, int button_index) {
    auto* action_buttons = view->GetViewByID(
        message_center::NotificationViewBase::kActionButtonsRow);

    auto* button_view = action_buttons->children()[button_index];

    ui::test::EventGenerator generator(GetRootWindow(button_view->GetWidget()));
    gfx::Point cursor_location = button_view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }
};

TEST_F(PrivacyIndicatorsControllerTest, NotificationMetadata) {
  std::string app_id = "test_app_id";
  std::u16string app_name = u"test_app_name";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  ash::ModifyPrivacyIndicatorsNotification(
      app_id, app_name, /*is_camera_used=*/true, /*is_microphone_used=*/true,
      delegate);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);

  // Notification message should contains app name.
  EXPECT_NE(std::string::npos, notification->message().find(app_name));

  // Privacy indicators notification should not be a popup. It is silently added
  // to the tray.
  EXPECT_FALSE(GetPopupNotificationView(notification_id));
}

TEST_F(PrivacyIndicatorsControllerTest, NotificationWithNoButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
      /*has_launch_app_callback=*/false,
      /*has_launch_settings_callback=*/false);
  ash::ModifyPrivacyIndicatorsNotification(
      app_id, u"test_app_name", /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);

  // With the delegate don't provide any callbacks, the notification
  // should have one button for launching the app.
  EXPECT_EQ(0u, notification->buttons().size());
}

TEST_F(PrivacyIndicatorsControllerTest, NotificationClickWithLaunchAppButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
      /*has_launch_app_callback=*/true, /*has_launch_settings_callback=*/false);
  ash::ModifyPrivacyIndicatorsNotification(
      app_id, u"test_app_name", /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification_id);

  // With the delegate provides only launch app callbacks, the notification
  // should have one button for launching the app.
  auto buttons = notification->buttons();
  ASSERT_EQ(1u, buttons.size());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_LAUNCH),
      buttons[0].title);

  // Clicking that button will trigger launching the app.
  EXPECT_FALSE(delegate->launch_app_called());
  ClickView(notification_view, 0);
  EXPECT_TRUE(delegate->launch_app_called());
}

TEST_F(PrivacyIndicatorsControllerTest,
       NotificationClickWithLaunchSettingsButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>(
      /*has_launch_app_callback=*/false, /*has_launch_settings_callback=*/true);
  ash::ModifyPrivacyIndicatorsNotification(
      app_id, u"test_app_name", /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification_id);

  // With the delegate provides only launch settings callbacks, the notification
  // should have one button for launching the app settings.
  auto buttons = notification->buttons();
  ASSERT_EQ(1u, buttons.size());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_SETTINGS),
      buttons[0].title);

  // Clicking that button will trigger launching the app.
  EXPECT_FALSE(delegate->launch_settings_called());
  ClickView(notification_view, 0);
  EXPECT_TRUE(delegate->launch_settings_called());
}

TEST_F(PrivacyIndicatorsControllerTest, NotificationClickWithTwoButtons) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  ash::ModifyPrivacyIndicatorsNotification(
      app_id, u"test_app_name", /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification_id);

  // With the delegate provides both launch app and launch settings callbacks,
  // the notification should have 2 buttons. The first one is the launch app and
  // the second one is the launch button.
  auto buttons = notification->buttons();
  ASSERT_EQ(2u, buttons.size());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_LAUNCH),
      buttons[0].title);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_BUTTON_APP_SETTINGS),
      buttons[1].title);

  // Clicking the first button will trigger launching the app.
  EXPECT_FALSE(delegate->launch_app_called());
  ClickView(notification_view, 0);
  EXPECT_TRUE(delegate->launch_app_called());

  // Clicking the second button will trigger launching the app settings.
  EXPECT_FALSE(delegate->launch_settings_called());
  ClickView(notification_view, 1);
  EXPECT_TRUE(delegate->launch_settings_called());
}

// Tests that a basic privacy indicator notification is disabled when the video
// conference feature is enabled.
TEST_F(PrivacyIndicatorsControllerTest,
       DoNotShowNotificationWithVideoConferenceEnabled) {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kVideoConference};
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kCameraEffectsSupportedByHardware);
  // Try to show a notification.
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  ash::ModifyPrivacyIndicatorsNotification(
      app_id, u"test_app_name", /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate);

  // The notification should not exist.
  EXPECT_FALSE(message_center::MessageCenter::Get()->FindNotificationById(
      notification_id));
}

}  // namespace ash
