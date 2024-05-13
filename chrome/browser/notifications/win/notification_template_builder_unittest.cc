// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_template_builder.h"

#include <memory>
#include <string>

#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/win/fake_notification_image_retainer.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::Notification;
using message_center::NotifierId;
using message_center::RichNotificationData;

namespace {

const char kContextMenuLabel[] = "settings";
const char kEncodedId[] =
    "0|0|Default|aumi|0|https://example.com/|notification_id";
const char kNotificationId[] = "notification_id";
const char16_t kNotificationTitle[] = u"My Title";
const char16_t kNotificationMessage[] = u"My Message";
const char kNotificationOrigin[] = "https://example.com";

base::Time FixedTime() {
  static constexpr base::Time::Exploded kTime = {.year = 1998,
                                                 .month = 9,
                                                 .day_of_month = 4,
                                                 .hour = 1,
                                                 .minute = 2,
                                                 .second = 3};
  base::Time time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kTime, &time));
  return time;
}

}  // namespace

class NotificationTemplateBuilderTest : public ::testing::Test {
 public:
  NotificationTemplateBuilderTest() = default;
  NotificationTemplateBuilderTest(const NotificationTemplateBuilderTest&) =
      delete;
  NotificationTemplateBuilderTest& operator=(
      const NotificationTemplateBuilderTest&) = delete;
  ~NotificationTemplateBuilderTest() override = default;

  void SetUp() override { SetContextMenuLabelForTesting(kContextMenuLabel); }

  void TearDown() override { SetContextMenuLabelForTesting(nullptr); }

 protected:
  // Builds a notification object and initializes it to default values.
  message_center::Notification BuildNotification() {
    GURL origin_url(kNotificationOrigin);
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
        kNotificationTitle, kNotificationMessage, ui::ImageModel() /* icon */,
        std::u16string() /* display_source */, origin_url,
        NotifierId(origin_url), RichNotificationData(), nullptr /* delegate */);
    // Set a fixed timestamp, to avoid having to test against current timestamp.
    notification.set_timestamp(FixedTime());
    notification.set_settings_button_handler(
        message_center::SettingsButtonHandler::DELEGATE);
    return notification;
  }

  // Converts the notification data to XML and verifies it is as expected. Calls
  // must be wrapped in ASSERT_NO_FATAL_FAILURE().
  void VerifyXml(const message_center::Notification& notification,
                 const std::wstring& expected_xml_template) {
    FakeNotificationImageRetainer image_retainer;
    NotificationLaunchId launch_id(kEncodedId);
    std::wstring xml_template =
        BuildNotificationTemplate(&image_retainer, launch_id, notification);
    EXPECT_EQ(xml_template, expected_xml_template);
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(NotificationTemplateBuilderTest, SimpleToast) {
  message_center::Notification notification = BuildNotification();

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, Buttons) {
  message_center::Notification notification = BuildNotification();

  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(u"Button1");
  buttons.emplace_back(u"Button2");
  notification.set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action activationType="foreground" content="Button2" arguments="1|1|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineReplies) {
  message_center::Notification notification = BuildNotification();

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo button1(u"Button1");
  button1.placeholder = u"Reply here";
  buttons.emplace_back(button1);
  buttons.emplace_back(u"Button2");
  notification.set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action activationType="foreground" content="Button2" arguments="1|1|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineRepliesDoubleInput) {
  message_center::Notification notification = BuildNotification();

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo button1(u"Button1");
  button1.placeholder = u"Reply here";
  buttons.emplace_back(button1);
  message_center::ButtonInfo button2(u"Button2");
  button2.placeholder = u"Should not appear";
  buttons.emplace_back(button2);
  notification.set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action activationType="foreground" content="Button2" arguments="1|1|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineRepliesTextTypeNotFirst) {
  message_center::Notification notification = BuildNotification();

  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(u"Button1");
  message_center::ButtonInfo button2(u"Button2");
  button2.placeholder = u"Reply here";
  buttons.emplace_back(button2);
  notification.set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action activationType="foreground" content="Button2" arguments="1|1|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, Silent) {
  message_center::Notification notification = BuildNotification();
  notification.set_silent(true);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
 <audio silent="true"/>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, RequireInteraction) {
  message_center::Notification notification = BuildNotification();

  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(u"Button1");
  notification.set_buttons(buttons);
  notification.set_never_timeout(true);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" scenario="reminder" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, RequireInteractionSuppressed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNotificationDurationLongForRequireInteraction);

  message_center::Notification notification = BuildNotification();
  notification.set_never_timeout(true);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" duration="long" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, NullTimestamp) {
  message_center::Notification notification = BuildNotification();
  notification.set_timestamp(base::Time());

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, LocalizedContextMenu) {
  message_center::Notification notification = BuildNotification();

  // Disable overriding context menu label.
  SetContextMenuLabelForTesting(nullptr);

  std::wstring expected_xml = base::StrCat(
      {LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content=")",
       l10n_util::GetWideString(
           IDS_WIN_NOTIFICATION_SETTINGS_CONTEXT_MENU_ITEM_NAME),
       LR"(" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)"});

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, expected_xml));
}

TEST_F(NotificationTemplateBuilderTest, Images) {
  message_center::Notification notification = BuildNotification();

  SkBitmap icon;
  icon.allocN32Pixels(64, 64);
  icon.eraseARGB(255, 100, 150, 200);

  notification.set_icon(
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(icon)));
  notification.SetImage(gfx::Image::CreateFrom1xBitmap(icon));

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo button(u"Button1");
  button.placeholder = u"Reply here";
  button.icon = gfx::Image::CreateFrom1xBitmap(icon);
  buttons.emplace_back(button);
  notification.set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
   <image placement="appLogoOverride" src="c:\temp\img0.tmp" hint-crop="none"/>
   <image placement="hero" src="c:\temp\img1.tmp"/>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|aumi|0|https://example.com/|notification_id" imageUri="c:\temp\img2.tmp"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ContextMessage) {
  message_center::Notification notification = BuildNotification();

  notification.set_context_message(u"context_message");

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">context_message</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ExtensionNoContextMessage) {
  message_center::Notification notification = BuildNotification();

  // Explicitly not setting context message to ensure attribution is not added.
  // Explicitly set origin url to something non http/https to ensure that origin
  // is not used as attribution.
  notification.set_origin_url(
      GURL("chrome-extension://bfojpkhoiegeigfifhdnbeobmhlahdle/"));

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ProgressBar) {
  message_center::Notification notification = BuildNotification();

  notification.set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  notification.set_progress(30);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
   <progress status="" value="0.30"/>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ProgressBar_Indeterminate) {
  message_center::Notification notification = BuildNotification();

  notification.set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  // Setting the progress outside the [0-100] range should result in an
  // indeterminate progress notification.
  notification.set_progress(-1);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
   <progress status="" value="indeterminate"/>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ListEntries) {
  message_center::Notification notification = BuildNotification();

  notification.set_type(message_center::NOTIFICATION_TYPE_MULTIPLE);
  std::vector<message_center::NotificationItem> items;
  items.push_back({u"title1", u"message1"});
  items.push_back({u"title2", u"message2"});
  items.push_back({u"title3", u"message3"});
  items.push_back({u"title4", u"message4"});
  items.push_back({u"title5", u"message5"});  // Will be truncated.
  notification.set_items(items);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>title1 - message1
title2 - message2
title3 - message3
title4 - message4
</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, NoSettings) {
  message_center::Notification notification = BuildNotification();

  // Disable overriding context menu label.
  SetContextMenuLabelForTesting(nullptr);

  notification.set_settings_button_handler(
      message_center::SettingsButtonHandler::NONE);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions/>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, IncomingCallFromWebApp) {
  message_center::Notification notification = BuildNotification();
  notification.set_scenario(
      message_center::NotificationScenario::INCOMING_CALL);

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo acknowledge_button(u"Acknowledge");
  acknowledge_button.type = message_center::ButtonType::ACKNOWLEDGE;
  buttons.push_back(acknowledge_button);
  message_center::ButtonInfo dismiss_button(u"Close");
  dismiss_button.type = message_center::ButtonType::DISMISS;
  buttons.push_back(dismiss_button);
  notification.set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" scenario="incomingCall" useButtonStyle="true" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action activationType="foreground" hint-buttonStyle="Success" content="Acknowledge" arguments="1|0|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action activationType="background" hint-buttonStyle="Critical" content="Close" arguments="3|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, IncomingCallFromNonInstalledOrigin) {
  message_center::Notification notification = BuildNotification();

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo acknowledge_button(u"Acknowledge");
  acknowledge_button.type = message_center::ButtonType::ACKNOWLEDGE;
  buttons.push_back(acknowledge_button);
  message_center::ButtonInfo dismiss_button(u"Close");
  dismiss_button.type = message_center::ButtonType::DISMISS;
  buttons.push_back(dismiss_button);
  notification.set_buttons(buttons);

  // In this case, the toast wont' have the "scenario" and "useButtonStyle"
  // arguments being set. Thus, even if the action buttons have the
  // "hint-buttonStyle" argument set, it should not take effect.
  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action activationType="foreground" hint-buttonStyle="Success" content="Acknowledge" arguments="1|0|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action activationType="background" hint-buttonStyle="Critical" content="Close" arguments="3|0|Default|aumi|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|aumi|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(notification, kExpectedXml));
}
