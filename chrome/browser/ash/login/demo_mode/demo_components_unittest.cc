// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_session.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::component_updater::FakeCrOSComponentManager;

constexpr char kResourcesComponent[] = "demo-mode-resources";
constexpr char kAppComponent[] = "demo-mode-app";
constexpr char kTestDemoModeAppMountPoint[] = "/run/imageloader/demo_mode_app";
constexpr char kTestDemoModeResourcesMountPoint[] =
    "/run/imageloader/demo_mode_resources";
constexpr char kDemoAppsImageFile[] = "android_demo_apps.squash";
constexpr char kExternalExtensionsPrefsFile[] = "demo_extensions.json";

void SetBoolean(bool* value) {
  *value = true;
}

class DemoComponentsTest : public testing::Test {
 public:
  DemoComponentsTest()
      : browser_process_platform_part_test_api_(
            g_browser_process->platform_part()) {}

  DemoComponentsTest(const DemoComponentsTest&) = delete;
  DemoComponentsTest& operator=(const DemoComponentsTest&) = delete;

  ~DemoComponentsTest() override = default;

  void SetUp() override { InitializeCrosComponentManager(); }

  void TearDown() override {
    cros_component_manager_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownCrosComponentManager();
  }

 protected:
  bool FinishComponentLoad(const std::string& component_name,
                           const base::FilePath& mount_path) {
    EXPECT_TRUE(cros_component_manager_->HasPendingInstall(component_name));
    EXPECT_TRUE(cros_component_manager_->UpdateRequested(component_name));

    return cros_component_manager_->FinishLoadRequest(
        component_name,
        FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/dev/null"), mount_path));
  }

  void InitializeCrosComponentManager() {
    auto fake_cros_component_manager =
        base::MakeRefCounted<FakeCrOSComponentManager>();
    fake_cros_component_manager->set_queue_load_requests(true);
    fake_cros_component_manager->set_supported_components(
        {kResourcesComponent, kAppComponent});
    cros_component_manager_ = fake_cros_component_manager.get();

    browser_process_platform_part_test_api_.InitializeCrosComponentManager(
        std::move(fake_cros_component_manager));
  }

  FakeCrOSComponentManager* cros_component_manager_ = nullptr;
  content::BrowserTaskEnvironment task_environment_;

 private:
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;
};

TEST_F(DemoComponentsTest, GetPaths) {
  DemoComponents demo_components(DemoSession::DemoModeConfig::kOnline);
  demo_components.LoadResourcesComponent(base::DoNothing());
  EXPECT_FALSE(demo_components.resources_component_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishComponentLoad(kResourcesComponent, component_mount_point));

  EXPECT_TRUE(demo_components.resources_component_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_components.GetDemoAndroidAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_components.GetExternalExtensionsPrefsPath());
  EXPECT_EQ(component_mount_point.AppendASCII("foo.txt"),
            demo_components.GetAbsolutePath(base::FilePath("foo.txt")));
  EXPECT_EQ(component_mount_point.AppendASCII("foo/bar.txt"),
            demo_components.GetAbsolutePath(base::FilePath("foo/bar.txt")));
  EXPECT_EQ(component_mount_point.AppendASCII("foo/"),
            demo_components.GetAbsolutePath(base::FilePath("foo/")));
  EXPECT_TRUE(
      demo_components.GetAbsolutePath(base::FilePath("../foo/")).empty());
  EXPECT_TRUE(
      demo_components.GetAbsolutePath(base::FilePath("foo/../bar")).empty());
}

TEST_F(DemoComponentsTest, LoadResourcesComponent) {
  DemoComponents demo_components(DemoSession::DemoModeConfig::kOnline);
  demo_components.LoadResourcesComponent(base::DoNothing());

  EXPECT_FALSE(demo_components.resources_component_loaded());

  ASSERT_TRUE(FinishComponentLoad(
      kResourcesComponent, base::FilePath(kTestDemoModeResourcesMountPoint)));
  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kResourcesComponent));
  EXPECT_TRUE(demo_components.resources_component_loaded());
}

TEST_F(DemoComponentsTest, EnsureResourcesLoadedRepeatedly) {
  DemoComponents demo_components(DemoSession::DemoModeConfig::kOnline);

  bool first_callback_called = false;
  demo_components.LoadResourcesComponent(
      base::BindOnce(&SetBoolean, &first_callback_called));

  bool second_callback_called = false;
  demo_components.LoadResourcesComponent(
      base::BindOnce(&SetBoolean, &second_callback_called));

  bool third_callback_called = false;
  demo_components.LoadResourcesComponent(
      base::BindOnce(&SetBoolean, &third_callback_called));

  EXPECT_FALSE(demo_components.resources_component_loaded());
  EXPECT_FALSE(first_callback_called);
  EXPECT_FALSE(second_callback_called);
  EXPECT_FALSE(third_callback_called);

  ASSERT_TRUE(FinishComponentLoad(
      kResourcesComponent, base::FilePath(kTestDemoModeResourcesMountPoint)));
  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kResourcesComponent));

  EXPECT_TRUE(demo_components.resources_component_loaded());
  EXPECT_TRUE(first_callback_called);
  EXPECT_TRUE(second_callback_called);
  EXPECT_TRUE(third_callback_called);

  bool fourth_callback_called = false;
  demo_components.LoadResourcesComponent(
      base::BindOnce(&SetBoolean, &fourth_callback_called));
  EXPECT_TRUE(fourth_callback_called);

  bool fifth_callback_called = false;
  demo_components.LoadResourcesComponent(
      base::BindOnce(&SetBoolean, &fifth_callback_called));
  EXPECT_TRUE(fifth_callback_called);

  EXPECT_TRUE(demo_components.resources_component_loaded());
}

TEST_F(DemoComponentsTest, LoadAppComponent) {
  DemoComponents demo_cros_components(DemoSession::DemoModeConfig::kOnline);

  demo_cros_components.LoadAppComponent(base::DoNothing());
  ASSERT_TRUE(FinishComponentLoad(kAppComponent,
                                  base::FilePath(kTestDemoModeAppMountPoint)));

  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kAppComponent));
  EXPECT_EQ(demo_cros_components.default_app_component_path().value(),
            kTestDemoModeAppMountPoint);
}

}  // namespace
}  // namespace ash
