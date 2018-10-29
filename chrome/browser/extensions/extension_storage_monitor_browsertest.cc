// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_storage_monitor.h"
#include "chrome/browser/extensions/extension_storage_monitor_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/message_center/public/cpp/notification.h"

namespace extensions {

namespace {

const int kInitialUsageThreshold = 500;

const char kWriteDataApp[] = "storage_monitor/write_data";

std::unique_ptr<KeyedService> CreateExtensionStorageMonitorInstance(
    content::BrowserContext* context) {
  return std::make_unique<ExtensionStorageMonitor>(
      Profile::FromBrowserContext(context));
}

class NotificationObserver {
 public:
  NotificationObserver(NotificationDisplayServiceTester* display_service,
                       const std::string& target_notification)
      : display_service_(display_service),
        target_notification_id_(target_notification) {
    // Don't count old notifications.
    display_service_->RemoveAllNotifications(
        NotificationHandler::Type::TRANSIENT, false);
  }

  ~NotificationObserver() {
    display_service_->SetNotificationAddedClosure(base::RepeatingClosure());
  }

  bool HasReceivedNotification() const {
    return !!display_service_->GetNotification(target_notification_id_);
  }

  // Runs the message loop and returns true if a notification is received.
  // Immediately returns true if a notification has already been received.
  bool WaitForNotification() {
    if (HasReceivedNotification())
      return true;

    waiting_ = true;
    display_service_->SetNotificationAddedClosure(base::BindRepeating(
        &NotificationObserver::OnNotificationAdded, base::Unretained(this)));
    content::RunMessageLoop();
    waiting_ = false;
    return HasReceivedNotification();
  }

 private:
  void OnNotificationAdded() {
    if (waiting_ && HasReceivedNotification())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  NotificationDisplayServiceTester* display_service_;
  std::string target_notification_id_;
  bool waiting_ = false;
};

}  // namespace

class ExtensionStorageMonitorTest : public ExtensionBrowserTest {
 public:
  ExtensionStorageMonitorTest() = default;

 protected:
  // ExtensionBrowserTest overrides:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());

    host_resolver()->AddRule("*", "127.0.0.1");

    InitStorageMonitor();
  }

  ExtensionStorageMonitor* monitor() {
    CHECK(storage_monitor_);
    return storage_monitor_.get();
  }

  int64_t GetInitialExtensionThreshold() {
    CHECK(storage_monitor_);
    return storage_monitor_->initial_extension_threshold_;
  }

  void DisableForInstalledExtensions() {
    CHECK(storage_monitor_);
    storage_monitor_->enable_for_all_extensions_ = false;
  }

  const Extension* InitWriteDataApp() {
    base::FilePath path = test_data_dir_.AppendASCII(kWriteDataApp);
    const Extension* extension = InstallExtension(path, 1);
    EXPECT_TRUE(extension);
    return extension;
  }

  const Extension* CreateHostedApp(const std::string& name,
                                   GURL app_url,
                                   std::vector<std::string> permissions) {
    auto dir = std::make_unique<TestExtensionDir>();

    url::Replacements<char> clear_port;
    clear_port.ClearPort();

    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1.0")
        .Set("manifest_version", 2)
        .Set(
            "app",
            DictionaryBuilder()
                .Set("urls",
                     ListBuilder()
                         .Append(app_url.ReplaceComponents(clear_port).spec())
                         .Build())
                .Set("launch",
                     DictionaryBuilder().Set("web_url", app_url.spec()).Build())
                .Build());
    ListBuilder permissions_builder;
    for (const std::string& permission : permissions)
      permissions_builder.Append(permission);
    manifest.Set("permissions", permissions_builder.Build());
    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  std::string GetNotificationId(const std::string& extension_id) {
    return monitor()->GetNotificationId(extension_id);
  }

  bool IsStorageNotificationEnabled(const std::string& extension_id) {
    return monitor()->IsStorageNotificationEnabled(extension_id);
  }

  int64_t GetNextStorageThreshold(const std::string& extension_id) {
    return monitor()->GetNextStorageThreshold(extension_id);
  }

  void WriteBytesExpectingNotification(const Extension* extension,
                                       int num_bytes,
                                       const char* filesystem = "PERSISTENT") {
    int64_t previous_threshold = GetNextStorageThreshold(extension->id());
    WriteBytes(extension, num_bytes, filesystem, true);
    ASSERT_GT(GetNextStorageThreshold(extension->id()), previous_threshold);
  }

  void WriteBytesNotExpectingNotification(
      const Extension* extension,
      int num_bytes,
      const char* filesystem = "PERSISTENT") {
    WriteBytes(extension, num_bytes, filesystem, false);
  }

  void SimulateProfileShutdown() {
    // Setting a testing factory function deletes the current
    // ExtensionStorageMonitor; see KeyedServiceFactory::SetTestingFactory().
    ExtensionStorageMonitorFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(&CreateExtensionStorageMonitorInstance));
    InitStorageMonitor();
  }

  void InitStorageMonitor() {
    EXPECT_FALSE(storage_monitor_);
    storage_monitor_ =
        ExtensionStorageMonitor::Get(profile())->weak_ptr_factory_.GetWeakPtr();
    ASSERT_TRUE(storage_monitor_);

    // Override thresholds so that we don't have to write a huge amount of data
    // to trigger notifications in these tests.
    storage_monitor_->enable_for_all_extensions_ = true;
    storage_monitor_->initial_extension_threshold_ = kInitialUsageThreshold;

    // To ensure storage events are dispatched from QuotaManager immediately.
    storage_monitor_->observer_rate_ = base::TimeDelta();
  }

  // Write bytes for a hosted app page that's loaded the script:
  //     //chrome/test/data/extensions/storage_monitor/hosted_apps/common.js
  void WriteBytesForHostedApp(const Extension* extension,
                              int num_bytes,
                              const std::string& filesystem) {
    content::WebContents* web_contents = OpenApplication(AppLaunchParams(
        profile(), extension, LAUNCH_CONTAINER_TAB,
        WindowOpenDisposition::SINGLETON_TAB, extensions::SOURCE_TEST));

    ASSERT_TRUE(WaitForLoadStop(web_contents));
    std::string result;
    const char* script = R"(
        HostedAppWriteData(%s, %d)
           .then(() => domAutomationController.send('write_done'))
           .catch(e => domAutomationController.send('write_error: ' + e));
    )";
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        web_contents, base::StringPrintf(script, filesystem.c_str(), num_bytes),
        &result));
    ASSERT_EQ("write_done", result);
  }

  // Write bytes for the extension loaded from:
  //     //chrome/test/data/extensions/storage_monitor/write_data
  void WriteBytesForExtension(const Extension* extension, int num_bytes) {
    ExtensionTestMessageListener launched_listener("launched", true);
    ExtensionTestMessageListener write_complete_listener("write_complete",
                                                         false);

    OpenApplication(AppLaunchParams(profile(), extension, LAUNCH_CONTAINER_NONE,
                                    WindowOpenDisposition::NEW_WINDOW,
                                    extensions::SOURCE_TEST));

    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

    // Instruct the app to write |num_bytes| of data.
    launched_listener.Reply(base::IntToString(num_bytes));
    ASSERT_TRUE(write_complete_listener.WaitUntilSatisfied());
  }

  // Write a number of bytes to persistent storage.
  void WriteBytes(const Extension* extension,
                  int num_bytes,
                  const std::string& filesystem,
                  bool expected_notification) {
    NotificationObserver notification_observer(
        display_service_.get(), GetNotificationId(extension->id()));

    if (extension->is_hosted_app()) {
      WriteBytesForHostedApp(extension, num_bytes, filesystem);
    } else {
      ASSERT_EQ("PERSISTENT", filesystem) << "Not implemented in the js code.";
      WriteBytesForExtension(extension, num_bytes);
    }

    if (expected_notification) {
      ASSERT_TRUE(notification_observer.WaitForNotification());
    } else {
      base::RunLoop().RunUntilIdle();
      ASSERT_FALSE(notification_observer.HasReceivedNotification());
    }
  }

  base::WeakPtr<ExtensionStorageMonitor> storage_monitor_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::vector<std::unique_ptr<TestExtensionDir>> temp_dirs_;
};

// Control - No notifications should be shown if usage remains under the
// threshold.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, UnderThreshold) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesNotExpectingNotification(extension, 1);
}

// Ensure a notification is shown when usage reaches the first threshold.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, ExceedInitialThreshold) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension, GetInitialExtensionThreshold());
}

// Ensure a notification is shown when usage immediately exceeds double the
// first threshold.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, DoubleInitialThreshold) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension,
                                  GetInitialExtensionThreshold() * 2);
}

// Ensure that notifications are not fired if the next threshold has not been
// reached.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, ThrottleNotifications) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);

  // Exceed the first threshold.
  WriteBytesExpectingNotification(extension, GetInitialExtensionThreshold());

  // Stay within the next threshold.
  WriteBytesNotExpectingNotification(extension, 1);
}

// Verify that notifications are disabled when the user clicks the action button
// in the notification.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, UserDisabledNotifications) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension, GetInitialExtensionThreshold());

  EXPECT_TRUE(IsStorageNotificationEnabled(extension->id()));

  // Fake clicking the notification button to disable notifications.
  display_service_->GetNotification(GetNotificationId(extension->id()))
      ->delegate()
      ->Click(ExtensionStorageMonitor::BUTTON_DISABLE_NOTIFICATION,
              base::nullopt);

  EXPECT_FALSE(IsStorageNotificationEnabled(extension->id()));

  // Expect to receive no further notifications when usage continues to
  // increase.
  int64_t next_threshold = GetNextStorageThreshold(extension->id());
  int64_t next_data_size = next_threshold - GetInitialExtensionThreshold();
  ASSERT_GT(next_data_size, 0);

  WriteBytesNotExpectingNotification(extension, next_data_size);
}

// Ensure that monitoring is disabled for installed extensions if
// |enable_for_all_extensions_| is false. This test can be removed if monitoring
// is eventually enabled for all extensions.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest,
                       DisableForInstalledExtensions) {
  DisableForInstalledExtensions();

  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesNotExpectingNotification(extension, GetInitialExtensionThreshold());
}

// Regression test for https://crbug.com/716426
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest,
                       HostedAppTemporaryFilesystem) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL(
      "chromium.org", "/extensions/storage_monitor/hosted_apps/one/index.html");
  const Extension* app =
      CreateHostedApp("Hosted App", url, {"unlimitedStorage"});

  EXPECT_NO_FATAL_FAILURE(WriteBytesExpectingNotification(
      app, GetInitialExtensionThreshold(), "TEMPORARY"));
  EXPECT_NO_FATAL_FAILURE(WriteBytesNotExpectingNotification(
      app, GetInitialExtensionThreshold(), "PERSISTENT"));
  EXPECT_NO_FATAL_FAILURE(WriteBytesExpectingNotification(
      app, GetInitialExtensionThreshold(), "TEMPORARY"));

  // Bug 716426 was a shutdown crash due to not removing a
  // storage::StorageObserver registration before deleting the observer. To
  // recreate that scenario, first disable the app (which leaks the observer
  // registration), then simulate the step of profile exit where we delete the
  // StorageObserver.
  DisableExtension(app->id());
  SimulateProfileShutdown();

  // Now generate more storage activity for the hosted app's temporary
  // filesystem. Note that it's not a hosted app anymore -- it's just a webpage.
  // Bug 716426 caused this to crash the browser.
  EXPECT_NO_FATAL_FAILURE(WriteBytesNotExpectingNotification(
      app, GetInitialExtensionThreshold(), "TEMPORARY"));
}

// Exercises the case where two hosted apps are same-origin but have non-
// overlapping extents. Disabling one should not suppress storage monitoring for
// the other.
// Disabled for flakiness. crbug.com/799022
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest,
                       DISABLED_TwoHostedAppsInSameOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL(
      "chromium.org", "/extensions/storage_monitor/hosted_apps/one/index.html");
  const Extension* app1 = CreateHostedApp("App 1", url1, {"unlimitedStorage"});

  GURL url2 = embedded_test_server()->GetURL(
      "chromium.org", "/extensions/storage_monitor/hosted_apps/two/index.html");
  const Extension* app2 = CreateHostedApp("App 2", url2, {"unlimitedStorage"});

  EXPECT_EQ(url1.GetOrigin(), url2.GetOrigin());

  EXPECT_NO_FATAL_FAILURE(
      WriteBytesExpectingNotification(app1, GetInitialExtensionThreshold()));
  EXPECT_NO_FATAL_FAILURE(WriteBytesExpectingNotification(
      app2, GetInitialExtensionThreshold() * 2));

  // Disable app2. We should still be monitoring the origin on behalf of app1.
  DisableExtension(app2->id());

  // Writing a bunch of data in app1 should trigger the warning.
  EXPECT_NO_FATAL_FAILURE(WriteBytesExpectingNotification(
      app1, GetInitialExtensionThreshold() * 4));
}

// Verify that notifications are disabled when the user clicks the action button
// in the notification.
// Flaky: https://crbug.com/617801
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest,
                       DISABLED_UninstallExtension) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension, GetInitialExtensionThreshold());

  // Fake clicking the notification button to uninstall and accepting the
  // uninstall.
  ScopedTestDialogAutoConfirm scoped_autoconfirm(
      ScopedTestDialogAutoConfirm::ACCEPT);
  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()),
                                         extension->id());
  display_service_->GetNotification(GetNotificationId(extension->id()))
      ->delegate()
      ->Click(ExtensionStorageMonitor::BUTTON_UNINSTALL, base::nullopt);
  observer.WaitForExtensionUninstalled();
}

}  // namespace extensions
