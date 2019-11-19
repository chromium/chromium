// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

class ArcAppsPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  ArcAppsPrivateApiTest() = default;
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
    // Whitelist the test platform app.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "fgkfegllpjfhppblcabhjjipnfelohej");
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
    // SessionManagerClient will be destroyed in ChromeBrowserMain.
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::FakeSessionManagerClient::Get()->set_arc_available(true);
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

  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(ArcAppsPrivateApiTest, GetPackageNameAndLaunchApp) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  CreateAppInstance(prefs);
  // Add one launchable app and one non-launchable app.
  arc::mojom::AppInfo launchable_app("App_0", "Package_0", "Dummy_activity_0");
  app_instance()->SendRefreshAppList({launchable_app});
  static_cast<arc::mojom::AppHost*>(prefs)->OnTaskCreated(
      0 /* task_id */, "Package_1", "Dummy_activity_1", "App_1",
      base::nullopt /* intent */);

  // Stopping the service makes the app non-ready.
  arc::ArcServiceManager::Get()->arc_bridge_service()->app()->CloseInstance(
      app_instance());
  EXPECT_EQ(0u, app_instance()->launch_requests().size());
  // Verify |chrome.arcAppsPrivate.getLaunchableApps| returns the package name
  // of the launchable app only. The JS test will attempt to launch the app.
  EXPECT_TRUE(
      RunPlatformAppTestWithArg("arc_app_launcher/launch_app", "Package_0"))
      << message_;

  // Verify the app is not launched because it's not ready.
  EXPECT_EQ(0u, app_instance()->launch_requests().size());
  // Restarting the service makes the app ready. Verify the app is launched
  // successfully.
  CreateAppInstance(prefs);
  app_instance()->SendRefreshAppList({launchable_app});
  ASSERT_EQ(1u, app_instance()->launch_requests().size());
  EXPECT_TRUE(app_instance()->launch_requests()[0]->IsForApp(launchable_app));
}

IN_PROC_BROWSER_TEST_F(ArcAppsPrivateApiTest, OnInstalled) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  CreateAppInstance(prefs);

  arc::mojom::AppInfo launchable_app("App_0", "Package_0", "Dummy_activity_0");

  // The JS test will observe the onInstalled event and attempt to launch the
  // newly installed app.
  SetCustomArg("Package_0");
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener ready_listener("ready", false);

  base::FilePath path =
      test_data_dir_.AppendASCII("arc_app_launcher/install_event");
  const extensions::Extension* app = LoadExtension(path);
  ASSERT_TRUE(app);
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  EXPECT_EQ(0u, app_instance()->launch_requests().size());
  // Add one launchable app and one non-launchable app.
  app_instance()->SendRefreshAppList({launchable_app});
  static_cast<arc::mojom::AppHost*>(prefs)->OnTaskCreated(
      0 /* task_id */, "Package_1", "Dummy_activity_1", "App_1",
      base::nullopt /* intent */);
  // Verify the JS test receives the onInstalled event for the launchable app
  // only, and the app is launched successfully.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_EQ(1u, app_instance()->launch_requests().size());
  EXPECT_TRUE(app_instance()->launch_requests()[0]->IsForApp(launchable_app));
}

IN_PROC_BROWSER_TEST_F(ArcAppsPrivateApiTest,
                       NoDemoModeAppLaunchSourceReported) {
  // Not in Demo mode
  EXPECT_FALSE(chromeos::DemoSession::IsDeviceInDemoMode());

  base::HistogramTester histogram_tester;

  // Should see 0 apps launched from the Launcher in the histogram at first.
  histogram_tester.ExpectTotalCount("DemoMode.AppLaunchSource", 0);

  // Launch an arc app as done in the tests above.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  CreateAppInstance(prefs);
  arc::mojom::AppInfo launchable_app("App_0", "Package_0", "Dummy_activity_0");
  app_instance()->SendRefreshAppList({launchable_app});
  EXPECT_TRUE(
      RunPlatformAppTestWithArg("arc_app_launcher/launch_app", "Package_0"))
      << message_;

  // Should still see no apps launched in the histogram.
  histogram_tester.ExpectTotalCount("DemoMode.AppLaunchSource", 0);
}

IN_PROC_BROWSER_TEST_F(ArcAppsPrivateApiTest, DemoModeAppLaunchSourceReported) {
  // Set Demo mode
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  EXPECT_TRUE(chromeos::DemoSession::IsDeviceInDemoMode());

  base::HistogramTester histogram_tester;

  // Should see 0 apps launched from the Launcher in the histogram at first.
  histogram_tester.ExpectTotalCount("DemoMode.AppLaunchSource", 0);

  // Launch an arc app as done in the tests above.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  CreateAppInstance(prefs);
  arc::mojom::AppInfo launchable_app("App_0", "Package_0", "Dummy_activity_0");
  app_instance()->SendRefreshAppList({launchable_app});
  EXPECT_TRUE(
      RunPlatformAppTestWithArg("arc_app_launcher/launch_app", "Package_0"))
      << message_;

  // Should see 1 app launched from the highlights app in the histogram.
  histogram_tester.ExpectUniqueSample(
      "DemoMode.AppLaunchSource",
      chromeos::DemoSession::AppLaunchSource::kExtensionApi, 1);
}
