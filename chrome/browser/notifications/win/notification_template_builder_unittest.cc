// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_template_builder.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/win/mock_notification_image_retainer.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/grit/chromium_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::Notification;
using message_center::NotifierId;
using message_center::RichNotificationData;

namespace {

const char kContextMenuLabel[] = "settings";
const char kEncodedId[] = "0|0|Default|0|https://example.com/|notification_id";
const char kNotificationId[] = "notification_id";
const char kNotificationTitle[] = "My Title";
const char kNotificationMessage[] = "My Message";
const char kNotificationOrigin[] = "https://example.com";

bool FixedTime(base::Time* time) {
  base::Time::Exploded exploded = {0};
  exploded.year = 1998;
  exploded.month = 9;
  exploded.day_of_month = 4;
  exploded.hour = 1;
  exploded.minute = 2;
  exploded.second = 3;
  return base::Time::FromUTCExploded(exploded, time);
}

}  // namespace

class NotificationTemplateBuilderTest : public ::testing::Test {
 public:
  NotificationTemplateBuilderTest() = default;
  ~NotificationTemplateBuilderTest() override = default;

  void SetUp() override {
    NotificationTemplateBuilder::OverrideContextMenuLabelForTesting(
        kContextMenuLabel);
  }

  void TearDown() override {
    NotificationTemplateBuilder::OverrideContextMenuLabelForTesting(nullptr);
  }

 protected:
  // Builds a notification object and initializes it to default values.
  std::unique_ptr<message_center::Notification> InitializeBasicNotification() {
    GURL origin_url(kNotificationOrigin);
    auto notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
        base::UTF8ToUTF16(kNotificationTitle),
        base::UTF8ToUTF16(kNotificationMessage), gfx::Image() /* icon */,
        base::string16() /* display_source */, origin_url,
        NotifierId(origin_url), RichNotificationData(), nullptr /* delegate */);
    // Set a fixed timestamp, to avoid having to test against current timestamp.
    base::Time timestamp;
    if (!FixedTime(&timestamp))
      return nullptr;
    notification->set_timestamp(timestamp);
    return notification;
  }

  // Converts the notification data to XML and verifies it is as expected. Calls
  // must be wrapped in ASSERT_NO_FATAL_FAILURE().
  void VerifyXml(const message_center::Notification& notification,
                 const base::string16& xml_template) {
    MockNotificationImageRetainer image_retainer;
    NotificationLaunchId launch_id(kEncodedId);
    template_ = NotificationTemplateBuilder::Build(&image_retainer, launch_id,
                                                   notification);

    ASSERT_TRUE(template_);

    EXPECT_EQ(template_->GetNotificationTemplate(), xml_template);
  }

  std::unique_ptr<NotificationTemplateBuilder> template_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationTemplateBuilderTest);
};

TEST_F(NotificationTemplateBuilderTest, SimpleToast) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, Buttons) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(base::ASCIIToUTF16("Button1"));
  buttons.emplace_back(base::ASCIIToUTF16("Button2"));
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|0|https://example.com/|notification_id"/>
  <action activationType="foreground" content="Button2" arguments="1|1|0|Default|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineReplies) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo button1(base::ASCIIToUTF16("Button1"));
  button1.placeholder = base::ASCIIToUTF16("Reply here");
  buttons.emplace_back(button1);
  buttons.emplace_back(base::ASCIIToUTF16("Button2"));
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|0|https://example.com/|notification_id"/>
  <action activationType="foreground" content="Button2" arguments="1|1|0|Default|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineRepliesDoubleInput) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo button1(base::ASCIIToUTF16("Button1"));
  button1.placeholder = base::ASCIIToUTF16("Reply here");
  buttons.emplace_back(button1);
  message_center::ButtonInfo button2(base::ASCIIToUTF16("Button2"));
  button2.placeholder = base::ASCIIToUTF16("Should not appear");
  buttons.emplace_back(button2);
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|0|https://example.com/|notification_id"/>
  <action activationType="foreground" content="Button2" arguments="1|1|0|Default|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineRepliesTextTypeNotFirst) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(base::ASCIIToUTF16("Button1"));
  message_center::ButtonInfo button2(base::ASCIIToUTF16("Button2"));
  button2.placeholder = base::ASCIIToUTF16("Reply here");
  buttons.emplace_back(button2);
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|0|https://example.com/|notification_id"/>
  <action activationType="foreground" content="Button2" arguments="1|1|0|Default|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, Silent) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();
  notification->set_silent(true);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
 <audio silent="true"/>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, RequireInteraction) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(base::ASCIIToUTF16("Button1"));
  notification->set_buttons(buttons);
  notification->set_never_timeout(true);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" scenario="reminder" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|0|https://example.com/|notification_id"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, NullTimestamp) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();
  base::Time timestamp;
  notification->set_timestamp(timestamp);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, LocalizedContextMenu) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();
  // Disable overriding context menu label.
  NotificationTemplateBuilder::OverrideContextMenuLabelForTesting(nullptr);

  const wchar_t kExpectedXmlTemplate[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action content="%ls" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  base::string16 settings_msg = l10n_util::GetStringUTF16(
      IDS_WIN_NOTIFICATION_SETTINGS_CONTEXT_MENU_ITEM_NAME);
  base::string16 expected_xml =
      base::StringPrintf(kExpectedXmlTemplate, settings_msg.c_str());

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, expected_xml));
}

TEST_F(NotificationTemplateBuilderTest, Images) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  SkBitmap icon;
  icon.allocN32Pixels(64, 64);
  icon.eraseARGB(255, 100, 150, 200);

  notification->set_icon(gfx::Image::CreateFrom1xBitmap(icon));
  notification->set_image(gfx::Image::CreateFrom1xBitmap(icon));

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo button(base::ASCIIToUTF16("Button1"));
  button.placeholder = base::ASCIIToUTF16("Reply here");
  button.icon = gfx::Image::CreateFrom1xBitmap(icon);
  buttons.emplace_back(button);
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
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
  <action activationType="foreground" content="Button1" arguments="1|0|0|Default|0|https://example.com/|notification_id" imageUri="c:\temp\img2.tmp"/>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ContextMessage) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  notification->set_context_message(L"context_message");

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">context_message</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ExtensionNoContextMessage) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  // Explicitly not setting context message to ensure attribution is not added.
  // Explicitly set origin url to something non http/https to ensure that origin
  // is not used as attribution.
  notification->set_origin_url(
      GURL("chrome-extension://bfojpkhoiegeigfifhdnbeobmhlahdle/"));

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ProgressBar) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  notification->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  notification->set_progress(30);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
   <progress status="" value="0.30"/>
  </binding>
 </visual>
 <actions>
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, ListEntries) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  notification->set_type(message_center::NOTIFICATION_TYPE_MULTIPLE);
  std::vector<message_center::NotificationItem> items;
  items.push_back({L"title1", L"message1"});
  items.push_back({L"title2", L"message2"});
  items.push_back({L"title3", L"message3"});
  items.push_back({L"title4", L"message4"});
  items.push_back({L"title5", L"message5"});  // Will be truncated.
  notification->set_items(items);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id" displayTimestamp="1998-09-04T01:02:03Z">
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
  <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}
