// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_interactive_uitest_support.h"
#include "chrome/browser/notifications/notification_ui_manager_impl.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "content/public/test/browser_test.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using message_center::Notification;

class NotificationUIManagerBrowserTest : public InProcessBrowserTest {
 public:
  NotificationUIManagerBrowserTest() {
    feature_list_.InitWithFeatures(
        {}, {features::kNativeNotifications, features::kSystemNotifications});
  }

  NotificationUIManagerImpl* manager() {
    return static_cast<NotificationUIManagerImpl*>(
        g_browser_process->notification_ui_manager());
  }

  message_center::MessageCenter* message_center() {
    return message_center::MessageCenter::Get();
  }

  Profile* profile() { return browser()->profile(); }

  class TestDelegate : public message_center::NotificationDelegate {
   public:
    TestDelegate() = default;
    TestDelegate(const TestDelegate&) = delete;
    TestDelegate& operator=(const TestDelegate&) = delete;
    void Close(bool by_user) override {
      log_ += "Close_";
      log_ += (by_user ? "by_user_" : "programmatically_");
    }
    void Click(const std::optional<int>& button_index,
               const std::optional<std::u16string>& reply) override {
      if (button_index) {
        log_ += "ButtonClick_";
        log_ += base::NumberToString(*button_index) + "_";
      } else {
        log_ += "Click_";
      }
    }
    const std::string& log() { return log_; }

   private:
    ~TestDelegate() override {}
    std::string log_;
  };

  Notification CreateTestNotification(const std::string& id,
                                      TestDelegate** delegate = NULL) {
    TestDelegate* new_delegate = new TestDelegate();
    if (delegate) {
      *delegate = new_delegate;
      new_delegate->AddRef();
    }

    return Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, u"title", u"message",
        ui::ImageModel(), u"chrome-test://testing/",
        GURL("chrome-test://testing/"),
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   "extension_id"),
        message_center::RichNotificationData(), new_delegate);
  }

  Notification CreateRichTestNotification(const std::string& id,
                                          TestDelegate** delegate = NULL) {
    TestDelegate* new_delegate = new TestDelegate();
    if (delegate) {
      *delegate = new_delegate;
      new_delegate->AddRef();
    }

    message_center::RichNotificationData data;

    return Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, u"title", u"message",
        ui::ImageModel(), u"chrome-test://testing/",
        GURL("chrome-test://testing/"),
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   "extension_id"),
        data, new_delegate);
  }

  void RunLoopUntilIdle() {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NotificationUIManagerBrowserTest, RetrieveBaseParts) {
  EXPECT_TRUE(manager());
  EXPECT_TRUE(message_center());
}

IN_PROC_BROWSER_TEST_F(NotificationUIManagerBrowserTest, BasicNullProfile) {
  TestMessageCenterObserver observer;
  message_center()->AddObserver(&observer);
  manager()->CancelAll();
  manager()->Add(CreateTestNotification("hey"), nullptr);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  EXPECT_NE("", observer.last_displayed_id());
  manager()->CancelById("hey", ProfileNotification::GetProfileID(nullptr));
  EXPECT_EQ(0u, message_center()->NotificationCount());
  message_center()->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(NotificationUIManagerBrowserTest, BasicAddCancel) {
  // Someone may create system notifications like "you're in multi-profile
  // mode..." or something which may change the expectation.
  // TODO(mukai): move this to SetUpOnMainThread() after fixing the side-effect
  // of canceling animation which prevents some Displayed() event.
  TestMessageCenterObserver observer;
  message_center()->AddObserver(&observer);
  manager()->CancelAll();
  manager()->Add(CreateTestNotification("hey"), profile());
  EXPECT_EQ(1u, message_center()->NotificationCount());
  EXPECT_NE("", observer.last_displayed_id());
  manager()->CancelById("hey", ProfileNotification::GetProfileID(profile()));
  EXPECT_EQ(0u, message_center()->NotificationCount());
  message_center()->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(NotificationUIManagerBrowserTest, BasicDelegate) {
  TestDelegate* delegate;
  manager()->Add(CreateTestNotification("hey", &delegate), profile());
  manager()->CancelById("hey", ProfileNotification::GetProfileID(profile()));
  // Verify that delegate accumulated correct log of events.
  EXPECT_EQ("Close_programmatically_", delegate->log());
  delegate->Release();
}

IN_PROC_BROWSER_TEST_F(NotificationUIManagerBrowserTest,
                       ButtonClickedDelegate) {
  TestDelegate* delegate;
  manager()->Add(CreateTestNotification("n", &delegate), profile());
  const std::string notification_id =
      manager()->GetMessageCenterNotificationIdForTest("n", profile());
  message_center()->ClickOnNotificationButton(notification_id, 1);
  // Verify that delegate accumulated correct log of events.
  EXPECT_EQ("ButtonClick_1_", delegate->log());
  delegate->Release();
}

IN_PROC_BROWSER_TEST_F(NotificationUIManagerBrowserTest,
                       UpdateExistingNotification) {
  TestDelegate* delegate;
  manager()->Add(CreateTestNotification("n", &delegate), profile());
  TestDelegate* delegate2;
  manager()->Add(CreateRichTestNotification("n", &delegate2), profile());

  manager()->CancelById("n", ProfileNotification::GetProfileID(profile()));
  EXPECT_EQ("Close_programmatically_", delegate2->log());

  delegate->Release();
  delegate2->Release();
}

// On Lacros, notifications don't keep the browser alive. Don't run this test on
// Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(NotificationUIManagerBrowserTest, VerifyKeepAlives) {
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NOTIFICATION));

  TestDelegate* delegate;
  manager()->Add(CreateTestNotification("a", &delegate), profile());
  RunLoopUntilIdle();
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NOTIFICATION));

  TestDelegate* delegate2;
  manager()->Add(CreateRichTestNotification("b", &delegate2), profile());
  RunLoopUntilIdle();
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NOTIFICATION));

  manager()->CancelById("a", ProfileNotification::GetProfileID(profile()));
  RunLoopUntilIdle();
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NOTIFICATION));

  manager()->CancelById("b", ProfileNotification::GetProfileID(profile()));
  RunLoopUntilIdle();
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NOTIFICATION));

  delegate->Release();
  delegate2->Release();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
