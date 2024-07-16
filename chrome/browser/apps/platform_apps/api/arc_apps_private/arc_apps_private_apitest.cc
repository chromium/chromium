// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/path_service.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

class ArcAppsPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  ArcAppsPrivateApiTest() = default;
  ArcAppsPrivateApiTest(const ArcAppsPrivateApiTest&) = delete;
  ArcAppsPrivateApiTest& operator=(const ArcAppsPrivateApiTest&) = delete;
  ~ArcAppsPrivateApiTest() override = default;

 protected:
  // Helper function to create a fake app instance and wait for the instance to
  // be ready.
  void CreateAppInstance(ArcAppListPrefs* prefs) {
    app_instance_ = std::make_unique<arc::FakeAppInstance>(prefs);
    arc::ArcServiceManager::Get()->arc_bridge_service()->app()->SetInstance(
        app_instance());
    WaitForInstanceReady(
        arc::ArcServiceManager::Get()->arc_bridge_service()->app());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
    // Allowlist the test platform app.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "fgkfegllpjfhppblcabhjjipnfelohej");
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
    // SessionManagerClient will be destroyed in ChromeBrowserMain.
    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::FakeSessionManagerClient::Get()->set_arc_available(true);
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    arc::SetArcPlayStoreEnabledForProfile(profile(), true);
  }

  void TearDownOnMainThread() override {
    extensions::ExtensionApiTest::TearDownOnMainThread();
    if (app_instance_) {
      arc::ArcServiceManager::Get()->arc_bridge_service()->app()->CloseInstance(
          app_instance());
    }
    app_instance_.reset();
  }

  arc::FakeAppInstance* app_instance() { return app_instance_.get(); }

 private:
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

IN_PROC_BROWSER_TEST_F(ArcAppsPrivateApiTest, GetPackageNameAndLaunchApp) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  CreateAppInstance(prefs);
  // Add one launchable app and one non-launchable app.
  std::vector<arc::mojom::AppInfoPtr> one_app;
  one_app.emplace_back(
      arc::mojom::AppInfo::New("App_0", "Package_0", "Dummy_activity_0"));
  app_instance()->SendRefreshAppList(one_app);
  static_cast<arc::mojom::AppHost*>(prefs)->OnTaskCreated(
      0 /* task_id */, "Package_1", "Dummy_activity_1", "App_1",
      std::nullopt /* intent */, 0 /* session_id */);

  // Stopping the service makes the app non-ready.
  arc::ArcServiceManager::Get()->arc_bridge_service()->app()->CloseInstance(
      app_instance());
  EXPECT_EQ(0u, app_instance()->launch_requests().size());
  // Verify |chrome.arcAppsPrivate.getLaunchableApps| returns the package name
  // of the launchable app only. The JS test will attempt to launch the app.
  EXPECT_TRUE(RunExtensionTest(
      "arc_app_launcher/launch_app",
      {.custom_arg = "Package_0", .launch_as_platform_app = true}))
      << message_;

  // Verify the app is not launched because it's not ready.
  EXPECT_EQ(0u, app_instance()->launch_requests().size());
  // Restarting the service makes the app ready. Verify the app is launched
  // successfully.
  CreateAppInstance(prefs);
  app_instance()->SendRefreshAppList(one_app);
  EXPECT_EQ(0u, app_instance()->launch_requests().size());
  ASSERT_EQ(1u, app_instance()->launch_intents().size());
  EXPECT_NE(app_instance()->launch_intents()[0].find(
                "component=Package_0/Dummy_activity_0;"),
            std::string::npos);
}

IN_PROC_BROWSER_TEST_F(ArcAppsPrivateApiTest, OnInstalled) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  CreateAppInstance(prefs);

  std::vector<arc::mojom::AppInfoPtr> launchable_apps;
  launchable_apps.emplace_back(
      arc::mojom::AppInfo::New("App_0", "Package_0", "Dummy_activity_0"));

  // The JS test will observe the onInstalled event and attempt to launch the
  // newly installed app.
  SetCustomArg("Package_0");
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener ready_listener("ready");

  base::FilePath path =
      test_data_dir_.AppendASCII("arc_app_launcher/install_event");
  const extensions::Extension* app = LoadExtension(path);
  ASSERT_TRUE(app);
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  EXPECT_EQ(0u, app_instance()->launch_requests().size());
  // Add one launchable app and one non-launchable app.
  app_instance()->SendRefreshAppList(launchable_apps);
  static_cast<arc::mojom::AppHost*>(prefs)->OnTaskCreated(
      0 /* task_id */, "Package_1", "Dummy_activity_1", "App_1",
      std::nullopt /* intent */, 0 /* session_id */);
  // Verify the JS test receives the onInstalled event for the launchable app
  // only, and the app is launched successfully.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_EQ(1u, app_instance()->launch_requests().size());
  EXPECT_TRUE(
      app_instance()->launch_requests()[0]->IsForApp(*launchable_apps[0]));
}
