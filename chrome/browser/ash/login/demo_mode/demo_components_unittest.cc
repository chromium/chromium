// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_components.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::component_updater::FakeComponentManagerAsh;

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

  void SetUp() override { InitializeComponentManager(); }

  void TearDown() override {
    component_manager_ash_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownComponentManager();
  }

 protected:
  bool FinishComponentLoad(const std::string& component_name,
                           const base::FilePath& mount_path) {
    EXPECT_TRUE(component_manager_ash_->HasPendingInstall(component_name));
    EXPECT_TRUE(component_manager_ash_->UpdateRequested(component_name));

    return component_manager_ash_->FinishLoadRequest(
        component_name, FakeComponentManagerAsh::ComponentInfo(
                            component_updater::ComponentManagerAsh::Error::NONE,
                            base::FilePath("/dev/null"), mount_path));
  }

  void InitializeComponentManager() {
    auto fake_component_manager_ash =
        base::MakeRefCounted<FakeComponentManagerAsh>();
    fake_component_manager_ash->set_queue_load_requests(true);
    fake_component_manager_ash->set_supported_components(
        {kResourcesComponent, kAppComponent});
    component_manager_ash_ = fake_component_manager_ash.get();

    browser_process_platform_part_test_api_.InitializeComponentManager(
        std::move(fake_component_manager_ash));
  }

  raw_ptr<FakeComponentManagerAsh> component_manager_ash_ = nullptr;
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
  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kResourcesComponent));
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
  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kResourcesComponent));

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

  EXPECT_FALSE(component_manager_ash_->HasPendingInstall(kAppComponent));
  EXPECT_EQ(demo_cros_components.default_app_component_path().value(),
            kTestDemoModeAppMountPoint);
}

}  // namespace
}  // namespace ash
