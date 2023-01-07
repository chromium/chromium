// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace {

class LacrosExtensionAppsControllerTest
    : public extensions::ExtensionBrowserTest {
 public:
  void InstallApp() {
    DCHECK(app_id_.empty());
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
    app_id_ = lacros_extensions_util::MuxId(profile(), extension);
  }

  const std::string& app_id() const { return app_id_; }

 private:
  // extensions::ExtensionBrowserTest:
  void TearDownOnMainThread() override {
    CloseAllAppWindows();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void CloseAllAppWindows() {
    for (extensions::AppWindow* app_window :
         extensions::AppWindowRegistry::Get(profile())->app_windows()) {
      app_window->GetBaseWindow()->Close();
    }

    // Wait for item to stop existing in shelf.
    if (!app_id_.empty()) {
      ASSERT_TRUE(
          browser_test_util::WaitForShelfItem(app_id_, /*exists=*/false));
    }
  }

  std::string app_id_;
};

// Test that launching an app causing it to appear in the shelf. Closing the app
// removes it from the shelf.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, ShowsInShelf) {
  // If ash is does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kDoesItemExistInShelfMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Create the controller and publisher.
  std::unique_ptr<LacrosExtensionAppsPublisher> publisher =
      LacrosExtensionAppsPublisher::MakeForChromeApps();
  publisher->Initialize();
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->Initialize(publisher->publisher());

  // No item should exist in the shelf before the window is launched.
  InstallApp();
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/false));

  // There should be no app windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());

  // Launch the app via LacrosExtensionAppsController.
  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id();
  launch_params->launch_source = apps::LaunchSource::kFromTest;
  controller->Launch(std::move(launch_params), base::DoNothing());

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/true));
}

// Test that clicking a pinned chrome app in the shelf launches it.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, LaunchPinnedApp) {
  // If ash does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kSelectContextMenuForShelfItemMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Create the controller and publisher.
  std::unique_ptr<LacrosExtensionAppsPublisher> publisher =
      LacrosExtensionAppsPublisher::MakeForChromeApps();
  publisher->Initialize();
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->Initialize(publisher->publisher());

  // No item should exist in the shelf before the window is launched.
  InstallApp();
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/false));

  // Launch the app via LacrosExtensionAppsController.
  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id();
  launch_params->launch_source = apps::LaunchSource::kFromTest;
  controller->Launch(std::move(launch_params), base::DoNothing());

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/true));

  // Pin the shelf item.
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
  bool success = false;
  waiter.PinOrUnpinItemInShelf(app_id(), /*pin=*/true, &success);
  ASSERT_TRUE(success);

  // Close all app windows.
  extensions::AppWindowRegistry::AppWindowList app_windows =
      extensions::AppWindowRegistry::Get(profile())->app_windows();
  for (extensions::AppWindow* app_window : app_windows) {
    std::string window_id = lacros_window_utility::GetRootWindowUniqueId(
        app_window->GetNativeWindow()->GetRootWindow());
    app_window->GetBaseWindow()->Close();
    ASSERT_TRUE(browser_test_util::WaitForWindowDestruction(window_id));
  }

  // Confirm that there are no open windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());

  // Clicking on the item in the shelf should launch the app again.
  success = false;
  waiter.SelectItemInShelf(app_id(), &success);
  ASSERT_TRUE(success);

  // Wait for a window to open.
  base::RunLoop run_loop;
  base::RepeatingTimer timer;
  auto wait_for_window = base::BindRepeating(
      [](base::RunLoop* run_loop, Profile* profile) {
        if (!extensions::AppWindowRegistry::Get(profile)
                 ->app_windows()
                 .empty()) {
          run_loop->Quit();
        }
      },
      &run_loop, profile());
  timer.Start(FROM_HERE, base::Milliseconds(1), std::move(wait_for_window));
  run_loop.Run();

  // Now we must unpin the item to ensure ash-chrome is in consistent state.
  waiter.PinOrUnpinItemInShelf(app_id(), /*pin=*/false, &success);
}

// Test that the default context menu for an extension app has the correct
// items.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, DefaultContextMenu) {
  // If ash does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kGetContextMenuForShelfItemMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Create the controller and publisher.
  std::unique_ptr<LacrosExtensionAppsPublisher> publisher =
      LacrosExtensionAppsPublisher::MakeForChromeApps();
  publisher->Initialize();
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->Initialize(publisher->publisher());

  // No item should exist in the shelf before the window is launched.
  InstallApp();
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/false));

  // Launch the app via LacrosExtensionAppsController.
  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id();
  launch_params->launch_source = apps::LaunchSource::kFromTest;
  controller->Launch(std::move(launch_params), base::DoNothing());

  // Wait for item to exist in shelf.
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/true));

  // Get the context menu.
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
  std::vector<std::string> items;
  waiter.GetContextMenuForShelfItem(app_id(), &items);
  ASSERT_EQ(4u, items.size());
  EXPECT_EQ(items[0], "Pin to shelf");
  EXPECT_EQ(items[1], "Close");
  EXPECT_EQ(items[2], "Uninstall");
  EXPECT_EQ(items[3], "App info");
}

// Uninstalls an app via the context menu.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest,
                       UninstallContextMenu) {
  // If ash does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kSelectContextMenuForShelfItemMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Create the controller and publisher.
  std::unique_ptr<LacrosExtensionAppsPublisher> publisher =
      LacrosExtensionAppsPublisher::MakeForChromeApps();
  publisher->Initialize();
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->Initialize(publisher->publisher());

  // No item should exist in the shelf before the window is launched.
  InstallApp();
  LOG(INFO) << "No item starts in shelf";
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/false));

  // Launch the app via LacrosExtensionAppsController.
  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id();
  launch_params->launch_source = apps::LaunchSource::kFromTest;
  controller->Launch(std::move(launch_params), base::DoNothing());

  // Wait for item to exist in shelf.
  LOG(INFO) << "Wait for item to appear in shelf after install";
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/true));

  // Select index 2, which corresponds to Uninstall.
  base::HistogramTester tester;
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
  std::vector<std::string> items;
  bool success = false;
  waiter.SelectContextMenuForShelfItem(app_id(), /*index=*/2, &success);
  ASSERT_TRUE(success);

  // This pops up an ash dialog to confirm uninstall. First we wait fo the
  // dialog to appear, and then we click the confirm button.
  std::string element_name = kAppUninstallDialogOkButtonId.GetName();
  ASSERT_TRUE(browser_test_util::WaitForElementCreation(element_name));
  waiter.ClickElement(element_name, &success);
  ASSERT_TRUE(success);

  // Wait for the item to be no longer visible in the shelf as it's uninstalled
  // which implicitly closes the window.
  LOG(INFO) << "Wait for item to disappear from shelf after uninstall";
  ASSERT_TRUE(browser_test_util::WaitForShelfItem(app_id(), /*exists=*/false));
}

}  // namespace
