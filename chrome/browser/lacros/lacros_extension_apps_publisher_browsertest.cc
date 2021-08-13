// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"

#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/lacros/lacros_extension_apps_utility.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using Apps = std::vector<apps::mojom::AppPtr>;

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

 private:
  // Override to intercept calls to Publish().
  void Publish(Apps apps) override { apps_history_.push_back(std::move(apps)); }

  // Override to pretend that crosapi is initialized.
  bool InitializeCrosapi() override { return true; }

  // Holds the contents of all calls to Publish() in chronological order.
  std::vector<Apps> apps_history_;
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
    ASSERT_EQ(apps[0]->readiness, apps::mojom::Readiness::kReady);
  }

  // The last event should be an unload event.
  {
    Apps& apps = publisher->apps_history().back();
    ASSERT_EQ(1u, apps.size());
    ASSERT_EQ(apps[0]->readiness, apps::mojom::Readiness::kDisabledByUser);
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
    ASSERT_EQ(apps[0]->readiness, apps::mojom::Readiness::kReady);
  }

  // The last event should be an uninstall event.
  {
    Apps& apps = publisher->apps_history().back();
    ASSERT_EQ(1u, apps.size());
    ASSERT_EQ(apps[0]->readiness, apps::mojom::Readiness::kUninstalledByUser);
  }
}

// Test id muxing and demuxing.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, Mux) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::string muxed_id1 =
      lacros_extension_apps_utility::MuxId(profile(), extension);
  ASSERT_FALSE(muxed_id1.empty());
  Profile* demuxed_profile = nullptr;
  const extensions::Extension* demuxed_extension = nullptr;
  bool success = lacros_extension_apps_utility::DemuxId(
      muxed_id1, &demuxed_profile, &demuxed_extension);
  ASSERT_TRUE(success);
  EXPECT_EQ(demuxed_profile, profile());
  EXPECT_EQ(demuxed_extension, extension);
}

}  // namespace
