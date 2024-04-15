// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper_factory.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"
#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

using ContextType = extensions::ExtensionBrowserTest::ContextType;
using extensions::AppWindow;
using extensions::AppWindowRegistry;
using extensions::Extension;
using extensions::ExtensionNotificationDisplayHelper;
using extensions::ExtensionNotificationDisplayHelperFactory;
using extensions::ResultCatcher;

namespace utils = extensions::api_test_utils;

namespace {

enum class WindowState {
  FULLSCREEN,
  NORMAL
};

class NotificationsApiTest : public extensions::ExtensionApiTest {
 public:
  explicit NotificationsApiTest(ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~NotificationsApiTest() override = default;
  NotificationsApiTest(const NotificationsApiTest&) = delete;
  NotificationsApiTest& operator=(const NotificationsApiTest&) = delete;

  const Extension* LoadExtensionAndWait(
      const std::string& test_name) {
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    extensions::ExtensionHostTestHelper host_helper(profile());
    host_helper.RestrictToType(
        extensions::mojom::ViewType::kExtensionBackgroundPage);
    const extensions::Extension* extension = LoadExtension(extdir);
    if (extension) {
      host_helper.WaitForDocumentElementAvailable();
    }
    return extension;
  }

  const Extension* LoadAppWithWindowState(
      const std::string& test_name, WindowState window_state) {
    const char* window_state_string = nullptr;
    switch (window_state) {
      case WindowState::FULLSCREEN:
        window_state_string = "fullscreen";
        break;
      case WindowState::NORMAL:
        window_state_string = "normal";
        break;
    }
    const std::string& create_window_options = base::StringPrintf(
        "{\"state\":\"%s\"}", window_state_string);
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const extensions::Extension* extension = LoadExtension(extdir);
    EXPECT_TRUE(extension);

    ExtensionTestMessageListener launched_listener("launched",
                                                   ReplyBehavior::kWillReply);
    LaunchPlatformApp(extension);
    EXPECT_TRUE(launched_listener.WaitUntilSatisfied());
    launched_listener.Reply(create_window_options);

    return extension;
  }

  AppWindow* GetFirstAppWindow(const std::string& app_id) {
    AppWindowRegistry::AppWindowList app_windows = AppWindowRegistry::Get(
        browser()->profile())->GetAppWindowsForApp(app_id);

    AppWindowRegistry::const_iterator iter = app_windows.begin();
    if (iter != app_windows.end())
      return *iter;

    return nullptr;
  }

  ExtensionNotificationDisplayHelper* GetDisplayHelper() {
    return ExtensionNotificationDisplayHelperFactory::GetForProfile(profile());
  }

  NotifierStateTracker* GetNotifierStateTracker() {
    return NotifierStateTrackerFactory::GetForProfile(profile());
  }

 protected:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    DCHECK(profile());
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

  void TearDownOnMainThread() override {
    display_service_tester_.reset();
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  // Returns the notification that's being displayed for |extension|, or nullptr
  // when the notification count is not equal to one. It's not safe to rely on
  // the Notification pointer after closing the notification, but a copy can be
  // made to continue to be able to access the underlying information.
  message_center::Notification* GetNotificationForExtension(
      const extensions::Extension* extension) {
    DCHECK(extension);

    std::set<std::string> notifications =
        GetDisplayHelper()->GetNotificationIdsForExtension(extension->url());
    if (notifications.size() != 1)
      return nullptr;

    return GetDisplayHelper()->GetByNotificationId(*notifications.begin());
  }

  std::string GetNotificationIdFromDelegateId(const std::string& delegate_id) {
    return GetDisplayHelper()->GetByNotificationId(delegate_id)->id();
  }

  void LaunchPlatformApp(const Extension* extension) {
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
            extension->id(), apps::LaunchContainer::kLaunchContainerNone,
            WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

// TODO(crbug.com/40170747): We should merge this class with the base
// class once the issues mentioned in the bug are resolved.
class NotificationsApiTestWithBackgroundType
    : public NotificationsApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  NotificationsApiTestWithBackgroundType() : NotificationsApiTest(GetParam()) {}
  ~NotificationsApiTestWithBackgroundType() override = default;
  NotificationsApiTestWithBackgroundType(
      const NotificationsApiTestWithBackgroundType&) = delete;
  NotificationsApiTestWithBackgroundType& operator=(
      const NotificationsApiTestWithBackgroundType&) = delete;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         NotificationsApiTestWithBackgroundType,
                         testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         NotificationsApiTestWithBackgroundType,
                         testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(NotificationsApiTestWithBackgroundType, TestBasicUsage) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/basic_usage")) << message_;
}

// Flaky on TSan, see crbug.com/1304777.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TestEvents DISABLED_TestEvents
#else
#define MAYBE_TestEvents TestEvents
#endif
IN_PROC_BROWSER_TEST_P(NotificationsApiTestWithBackgroundType,
                       MAYBE_TestEvents) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/events")) << message_;
}

IN_PROC_BROWSER_TEST_P(NotificationsApiTestWithBackgroundType, TestCSP) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/csp")) << message_;
}

// Native notifications don't support (or use) observers.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestByUser) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/by_user");
  ASSERT_TRUE(extension) << message_;

  {
    ResultCatcher catcher;
    const std::string notification_id =
        GetNotificationIdFromDelegateId(extension->id() + "-FOO");
    display_service_tester_->RemoveNotification(
        NotificationHandler::Type::EXTENSION, notification_id,
        false /* by_user */);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  {
    ResultCatcher catcher;
    const std::string notification_id =
        GetNotificationIdFromDelegateId(extension->id() + "-BAR");
    display_service_tester_->RemoveNotification(
        NotificationHandler::Type::EXTENSION, notification_id,
        true /* by_user */);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  {
    ResultCatcher catcher;
    display_service_tester_->RemoveAllNotifications(
        NotificationHandler::Type::EXTENSION, false /* by_user */);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
  {
    ResultCatcher catcher;
    display_service_tester_->RemoveAllNotifications(
        NotificationHandler::Type::EXTENSION, true /* by_user */);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}
#endif  // !BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_P(NotificationsApiTestWithBackgroundType,
                       TestPartialUpdate) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/partial_update")) << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  const char16_t kNewTitle[] = u"Changed!";
  const char16_t kNewMessage[] = u"Too late! The show ended yesterday";
  int kNewPriority = 2;
  const char16_t kButtonTitle[] = u"NewButton";

  message_center::Notification* notification =
      GetNotificationForExtension(extension);
  ASSERT_TRUE(notification);

  EXPECT_EQ(kNewTitle, notification->title());
  EXPECT_EQ(kNewMessage, notification->message());
  EXPECT_EQ(kNewPriority, notification->priority());
  EXPECT_TRUE(notification->silent());
  EXPECT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(kButtonTitle, notification->buttons()[0].title);
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestGetPermissionLevel) {
  scoped_refptr<const Extension> empty_extension(
      extensions::ExtensionBuilder("Test").Build());

  // Get permission level for the extension whose notifications are enabled.
  {
    scoped_refptr<extensions::NotificationsGetPermissionLevelFunction>
        notification_function(
            new extensions::NotificationsGetPermissionLevelFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        notification_function.get(), "[]", profile(),
        extensions::api_test_utils::FunctionMode::kNone);

    EXPECT_EQ(base::Value::Type::STRING, result->type());
    EXPECT_TRUE(result->is_string());
    EXPECT_EQ("granted", result->GetString());
  }

  // Get permission level for the extension whose notifications are disabled.
  {
    scoped_refptr<extensions::NotificationsGetPermissionLevelFunction>
        notification_function(
            new extensions::NotificationsGetPermissionLevelFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    message_center::NotifierId notifier_id(
        message_center::NotifierType::APPLICATION, empty_extension->id());
    GetNotifierStateTracker()->SetNotifierEnabled(notifier_id, false);

    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        notification_function.get(), "[]", profile(),
        extensions::api_test_utils::FunctionMode::kNone);

    EXPECT_EQ(base::Value::Type::STRING, result->type());
    EXPECT_TRUE(result->is_string());
    EXPECT_EQ("denied", result->GetString());
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestOnPermissionLevelChanged) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/permission");
  ASSERT_TRUE(extension) << message_;

  // Test permission level changing from granted to denied.
  {
    ResultCatcher catcher;

    message_center::NotifierId notifier_id(
        message_center::NotifierType::APPLICATION, extension->id());
    GetNotifierStateTracker()->SetNotifierEnabled(notifier_id, false);

    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  // Test permission level changing from denied to granted.
  {
    ResultCatcher catcher;

    message_center::NotifierId notifier_id(
        message_center::NotifierType::APPLICATION, extension->id());
    GetNotifierStateTracker()->SetNotifierEnabled(notifier_id, true);

    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

// Native notifications don't support (nor use) observers.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestUserGesture) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/user_gesture");
  ASSERT_TRUE(extension) << message_;

  message_center::Notification* notification =
      GetNotificationForExtension(extension);
  ASSERT_TRUE(notification);

  {
    ExtensionTestMessageListener listener;
    // Action button event.
    display_service_tester_->SimulateClick(
        NotificationHandler::Type::EXTENSION, notification->id(),
        0 /* action_index */, std::nullopt /* reply */);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(listener.had_user_gesture());
  }

  {
    ExtensionTestMessageListener listener;
    // Click event.
    display_service_tester_->SimulateClick(
        NotificationHandler::Type::EXTENSION, notification->id(),
        std::nullopt /* action_index */, std::nullopt /* reply */);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(listener.had_user_gesture());
  }

  {
    ExtensionTestMessageListener listener;
    // Close event.
    display_service_tester_->RemoveNotification(
        NotificationHandler::Type::EXTENSION, notification->id(),
        true /* by_user */, false /* silent */);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(listener.had_user_gesture());
    // Note that |notification| no longer points to valid memory.
  }

  ASSERT_FALSE(GetNotificationForExtension(extension));
}
#endif  // !BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestRequireInteraction) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/require_interaction");
  ASSERT_TRUE(extension) << message_;

  message_center::Notification* notification =
      GetNotificationForExtension(extension);
  ASSERT_TRUE(notification);

  EXPECT_TRUE(notification->never_timeout());
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestShouldDisplayNormal) {
  ExtensionTestMessageListener notification_created_listener("created");
  const Extension* extension = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::NORMAL);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension->id())->GetNativeWindow()));

  message_center::Notification* notification =
      GetNotificationForExtension(extension);
  ASSERT_TRUE(notification);

  // If the app hasn't created a fullscreen window, then its notifications
  // shouldn't be displayed when a window is fullscreen.
  EXPECT_EQ(message_center::FullscreenVisibility::NONE,
            notification->fullscreen_visibility());
}

// Full screen related tests don't run on Mac as native notifications full
// screen decisions are done by the OS directly.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestShouldDisplayFullscreen) {
  ExtensionTestMessageListener notification_created_listener("created");
  const Extension* extension = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension->id())->GetNativeWindow()));

  ASSERT_TRUE(GetFirstAppWindow(extension->id())->IsFullscreen())
      << "Not Fullscreen";
  ASSERT_TRUE(GetFirstAppWindow(extension->id())->GetBaseWindow()->IsActive())
      << "Not Active";

  message_center::Notification* notification =
      GetNotificationForExtension(extension);
  ASSERT_TRUE(notification);

  // If the app has created a fullscreen window, then its notifications should
  // be displayed when a window is fullscreen.
  EXPECT_EQ(message_center::FullscreenVisibility::OVER_USER,
            notification->fullscreen_visibility());
}

// The Fake OSX fullscreen window doesn't like drawing a second fullscreen
// window when another is visible.
IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestShouldDisplayMultiFullscreen) {
  // Start a fullscreen app, and then start another fullscreen app on top of the
  // first. Notifications from the first should not be displayed because it is
  // not the app actually displaying on the screen.
  ExtensionTestMessageListener notification_created_listener("created");
  const Extension* extension1 = LoadAppWithWindowState(
      "notifications/api/notification_on_blur", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension1) << message_;

  ExtensionTestMessageListener window_visible_listener("visible");
  const Extension* extension2 = LoadAppWithWindowState(
      "notifications/api/other_app", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension2) << message_;

  ASSERT_TRUE(window_visible_listener.WaitUntilSatisfied());
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension2->id())->GetNativeWindow()));

  message_center::Notification* notification =
      GetNotificationForExtension(extension1);
  ASSERT_TRUE(notification);

  // The first app window is superseded by the second window, so its
  // notification shouldn't be displayed.
  EXPECT_EQ(message_center::FullscreenVisibility::NONE,
            notification->fullscreen_visibility());
}

// Verify that a notification is actually displayed when the app window that
// creates it is fullscreen.
IN_PROC_BROWSER_TEST_F(NotificationsApiTest,
                       TestShouldDisplayPopupNotification) {
  ExtensionTestMessageListener notification_created_listener("created");
  const Extension* extension = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension->id())->GetNativeWindow()));

  ASSERT_TRUE(GetFirstAppWindow(extension->id())->IsFullscreen())
      << "Not Fullscreen";
  ASSERT_TRUE(GetFirstAppWindow(extension->id())->GetBaseWindow()->IsActive())
      << "Not Active";

  message_center::Notification* notification =
      GetNotificationForExtension(extension);
  ASSERT_TRUE(notification);

  // The extension's window is being shown and focused, so its expected that
  // the notification displays on top of it.
  EXPECT_EQ(message_center::FullscreenVisibility::OVER_USER,
            notification->fullscreen_visibility());
}
#endif  // !BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestSmallImage) {
  ExtensionTestMessageListener notification_created_listener("created");
  const Extension* extension = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::NORMAL);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  message_center::Notification* notification =
      GetNotificationForExtension(extension);
  ASSERT_TRUE(notification);

  EXPECT_FALSE(notification->small_image().IsEmpty());
  EXPECT_TRUE(notification->small_image_needs_additional_masking());
}
