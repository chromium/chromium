// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

class BackgroundContentsServiceNotificationTest : public InProcessBrowserTest {
 public:
  BackgroundContentsServiceNotificationTest() = default;

  BackgroundContentsServiceNotificationTest(
      const BackgroundContentsServiceNotificationTest&) = delete;
  BackgroundContentsServiceNotificationTest& operator=(
      const BackgroundContentsServiceNotificationTest&) = delete;

  ~BackgroundContentsServiceNotificationTest() override = default;

  // Overridden from InProcessBrowserTest
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
    background_service_ =
        BackgroundContentsServiceFactory::GetForProfile(browser()->profile());
  }

  void TearDownOnMainThread() override {
    display_service_.reset();
    background_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Creates crash notification for the specified extension and returns
  // the created one.
  const message_center::Notification CreateCrashNotification(
      scoped_refptr<extensions::Extension> extension) {
    std::string notification_id = BackgroundContentsService::
        GetNotificationDelegateIdForExtensionForTesting(extension->id());
    background_service_->ShowBalloonForTesting(extension.get());
    base::RunLoop run_loop;
    display_service_->SetNotificationAddedClosure(run_loop.QuitClosure());
    run_loop.Run();
    display_service_->SetNotificationAddedClosure(base::RepeatingClosure());
    return *display_service_->GetNotification(notification_id);
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  raw_ptr<BackgroundContentsService> background_service_ = nullptr;

  bool HasIcons(scoped_refptr<extensions::Extension> extension) {
    return !extensions::IconsInfo::GetIcons(extension.get()).empty();
  }
};

IN_PROC_BROWSER_TEST_F(BackgroundContentsServiceNotificationTest,
                       TestShowBalloon) {
  scoped_refptr<extensions::Extension> extension;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    extension =
        extension_test_util::LoadManifest("image_loading_tracker", "app.json");
  }
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(HasIcons(extension));

  const message_center::Notification notification =
      CreateCrashNotification(extension);
  EXPECT_FALSE(notification.icon().IsEmpty());
}

// Verify if a test notification can show the default extension icon for
// a crash notification for an extension without icon.
IN_PROC_BROWSER_TEST_F(BackgroundContentsServiceNotificationTest,
                       TestShowBalloonNoIcon) {
  // Extension manifest file with no 'icon' field.
  scoped_refptr<extensions::Extension> extension;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    extension = extension_test_util::LoadManifest("app", "manifest.json");
  }
  ASSERT_TRUE(extension.get());
  ASSERT_FALSE(HasIcons(extension));

  const message_center::Notification notification =
      CreateCrashNotification(extension);
  EXPECT_FALSE(notification.icon().IsEmpty());
}

IN_PROC_BROWSER_TEST_F(BackgroundContentsServiceNotificationTest,
                       TestShowTwoBalloons) {
  scoped_refptr<extensions::Extension> extension;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    extension = extension_test_util::LoadManifest("app", "manifest.json");
  }
  ASSERT_TRUE(extension.get());
  CreateCrashNotification(extension);
  CreateCrashNotification(extension);

  ASSERT_EQ(1u, display_service_
                    ->GetDisplayedNotificationsForType(
                        NotificationHandler::Type::TRANSIENT)
                    .size());
}
