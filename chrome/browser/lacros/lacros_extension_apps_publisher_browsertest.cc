// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace {

using Apps = std::vector<apps::AppPtr>;

// This fake intercepts and tracks all calls to Publish().
class LacrosExtensionAppsPublisherFake : public LacrosExtensionAppsPublisher {
 public:
  LacrosExtensionAppsPublisherFake() = default;
  ~LacrosExtensionAppsPublisherFake() override = default;

  LacrosExtensionAppsPublisherFake(const LacrosExtensionAppsPublisherFake&) =
      delete;
  LacrosExtensionAppsPublisherFake& operator=(
      const LacrosExtensionAppsPublisherFake&) = delete;

  std::vector<Apps>& apps_history() { return apps_history_; }

  std::map<std::string, std::string>& app_windows() { return app_windows_; }

 private:
  // Override to intercept calls to Publish().
  void Publish(Apps apps) override { apps_history_.push_back(std::move(apps)); }

  // Override to intercept calls to OnAppWindowAdded().
  void OnAppWindowAdded(const std::string& app_id,
                        const std::string& window_id) override {
    app_windows_[window_id] = app_id;
  }

  // Override to intercept calls to OnAppWindowRemoved().
  void OnAppWindowRemoved(const std::string& app_id,
                          const std::string& window_id) override {
    EXPECT_TRUE(app_windows_.find(window_id) != app_windows_.end());
    EXPECT_EQ(app_windows_[window_id], app_id);
    app_windows_.erase(window_id);
  }

  // Override to pretend that crosapi is initialized.
  bool InitializeCrosapi() override { return true; }

  // Holds the contents of all calls to Publish() in chronological order.
  std::vector<Apps> apps_history_;

  // Holds the list of currently showing app windows, as seen by
  // OnAppWindowAdded() and OnAppWindowRemoved(). The key is the window_id and
  // the value is the app_id.
  std::map<std::string, std::string> app_windows_;
};

using LacrosExtensionAppsPublisherTest = extensions::ExtensionBrowserTest;

// If the profile has extensions, but no apps, then creating a publisher should
// have no effect.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, NoApps) {
  LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  EXPECT_TRUE(publisher->apps_history().empty());
}

// If the profile has one app installed, then creating a publisher should
// immediately result in a call to Publish() with 1 entry.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, OneApp) {
  LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  ASSERT_GE(publisher->apps_history().size(), 1u);
  Apps& apps = publisher->apps_history()[0];
  ASSERT_EQ(1u, apps.size());
}

// Same as OneApp, but with two pre-installed apps.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, TwoApps) {
  LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal_id"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  ASSERT_GE(publisher->apps_history().size(), 1u);
  Apps& apps = publisher->apps_history()[0];
  ASSERT_EQ(2u, apps.size());
}

// If an app is installed after the AppsPublisher is created, there should be a
// corresponding event.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest,
                       InstallAppAfterCreate) {
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  ASSERT_TRUE(publisher->apps_history().empty());
  LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  ASSERT_GE(publisher->apps_history().size(), 1u);
  Apps& apps = publisher->apps_history()[0];
  ASSERT_EQ(1u, apps.size());
}

// If an app is unloaded, there should be a corresponding unload event.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, Unload) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  UnloadExtension(extension->id());

  ASSERT_GE(publisher->apps_history().size(), 2u);

  // The first event should be a ready event.
  {
    Apps& apps = publisher->apps_history()[0];
    ASSERT_EQ(1u, apps.size());
    ASSERT_EQ(apps[0]->readiness, apps::Readiness::kReady);
  }

  // The last event should be an unload event.
  {
    Apps& apps = publisher->apps_history().back();
    ASSERT_EQ(1u, apps.size());
    ASSERT_EQ(apps[0]->readiness, apps::Readiness::kDisabledByUser);
  }
}

// If an app is uninstalled, there should be a corresponding uninstall event.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, Uninstall) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  UninstallExtension(extension->id());

  ASSERT_GE(publisher->apps_history().size(), 2u);

  // The first event should be a ready event.
  {
    Apps& apps = publisher->apps_history()[0];
    ASSERT_EQ(1u, apps.size());
    ASSERT_EQ(apps[0]->readiness, apps::Readiness::kReady);
  }

  // The last event should be an uninstall event.
  {
    Apps& apps = publisher->apps_history().back();
    ASSERT_EQ(1u, apps.size());
    ASSERT_EQ(apps[0]->readiness, apps::Readiness::kUninstalledByUser);
  }
}

// If the app window is loaded after to creating the publisher, everything
// should still work.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, LaunchAppWindow) {
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();

  // There should be no windows tracked.
  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(0u, app_windows.size());
  }

  // Load and launch the app.
  const extensions::Extension* extension =
      LoadAndLaunchApp(test_data_dir_.AppendASCII("platform_apps/minimal"));
  auto* registry = extensions::AppWindowRegistry::Get(profile());
  extensions::AppWindow* app_window =
      registry->GetCurrentAppWindowForApp(extension->id());
  ASSERT_TRUE(app_window);

  // Check that the window is tracked correctly.
  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(1u, app_windows.size());
    EXPECT_EQ(app_windows.begin()->second,
              lacros_extensions_util::MuxId(profile(), extension));
    EXPECT_EQ(app_windows.begin()->first,
              lacros_window_utility::GetRootWindowUniqueId(
                  app_window->GetNativeWindow()));
  }

  // Check that the window is no longer tracked. This process is asynchronous.
  app_window->GetBaseWindow()->Close();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(0u, app_windows.size());
  }
}

// If the app window is loaded prior to creating the publisher, everything
// should still work.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, PreLaunchAppWindow) {
  const extensions::Extension* extension =
      LoadAndLaunchApp(test_data_dir_.AppendASCII("platform_apps/minimal"));
  auto* registry = extensions::AppWindowRegistry::Get(profile());
  extensions::AppWindow* app_window =
      registry->GetCurrentAppWindowForApp(extension->id());
  ASSERT_TRUE(app_window);

  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();

  // Check that the window is tracked correctly.
  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(1u, app_windows.size());
    EXPECT_EQ(app_windows.begin()->second,
              lacros_extensions_util::MuxId(profile(), extension));
    EXPECT_EQ(app_windows.begin()->first,
              lacros_window_utility::GetRootWindowUniqueId(
                  app_window->GetNativeWindow()));
  }

  // Check that the window is no longer tracked. This process is asynchronous.
  app_window->GetBaseWindow()->Close();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(0u, app_windows.size());
  }
}

// Test id muxing and demuxing.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, Mux) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::string muxed_id1 = lacros_extensions_util::MuxId(profile(), extension);
  ASSERT_FALSE(muxed_id1.empty());
  Profile* demuxed_profile = nullptr;
  const extensions::Extension* demuxed_extension = nullptr;
  bool success = lacros_extensions_util::DemuxPlatformAppId(
      muxed_id1, &demuxed_profile, &demuxed_extension);
  ASSERT_TRUE(success);
  EXPECT_EQ(demuxed_profile, profile());
  EXPECT_EQ(demuxed_extension, extension);
}

}  // namespace
