// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"

#include "ash/public/cpp/shelf_model.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/plugin_vm/fake_plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/mock_plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAppWindowBase : public AppWindowBase {
 public:
  MockAppWindowBase(const ash::ShelfID& shelf_id, views::Widget* widget)
      : AppWindowBase(shelf_id, widget) {}
  ~MockAppWindowBase() = default;
  MockAppWindowBase(const MockAppWindowBase&) = delete;
  MockAppWindowBase& operator=(const MockAppWindowBase&) = delete;

  MOCK_METHOD(void, Activate, (), ());
};

}  // namespace

namespace plugin_vm {

using EnsureDefaultSharedDirExistsCallback =
    testing::StrictMock<base::MockCallback<
        base::OnceCallback<void(const base::FilePath& dir, bool result)>>>;

class PluginVmFilesTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_plugin_vm_features_.set_enabled(true);

    vm_tools::apps::ApplicationList app_list;
    app_list.set_vm_type(vm_tools::apps::ApplicationList::PLUGIN_VM);
    app_list.set_vm_name("PvmDefault");
    app_list.set_container_name("penguin");
    *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("name");
    app_id_ = crostini::CrostiniTestHelper::GenerateAppId("name", "PvmDefault",
                                                          "penguin");
    guest_os::GuestOsRegistryService(&profile_).UpdateApplicationList(app_list);

    mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
    mount_name_ = file_manager::util::GetDownloadsMountPointName(&profile_);
    mount_points_->RegisterFileSystem(
        mount_name_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), GetMyFilesFolderPath());
  }

  void TearDown() override {
    base::DeletePathRecursively(GetPvmDefaultPath());
    mount_points_->RevokeAllFileSystems();
  }

  base::FilePath GetMyFilesFolderPath() {
    return file_manager::util::GetMyFilesFolderForProfile(&profile_);
  }

  base::FilePath GetPvmDefaultPath() {
    return GetMyFilesFolderPath().Append("PvmDefault");
  }

  storage::FileSystemURL GetMyFilesFileSystemURL(const std::string& path) {
    return mount_points_->CreateExternalFileSystemURL(
        url::Origin(), mount_name_, base::FilePath(path));
  }

  struct ScopedDBusThreadManager {
    ScopedDBusThreadManager() { chromeos::DBusThreadManager::Initialize(); }
    ~ScopedDBusThreadManager() { chromeos::DBusThreadManager::Shutdown(); }
  } dbus_thread_manager_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakePluginVmFeatures fake_plugin_vm_features_;
  base::test::ScopedRunningOnChromeOS running_on_chromeos_;
  std::string app_id_;
  storage::ExternalMountPoints* mount_points_;
  std::string mount_name_;
};

TEST_F(PluginVmFilesTest, DirNotExists) {
  EnsureDefaultSharedDirExistsCallback callback;
  EnsureDefaultSharedDirExists(&profile_, callback.Get());
  EXPECT_CALL(callback, Run(GetPvmDefaultPath(), true));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, DirAlreadyExists) {
  EXPECT_TRUE(base::CreateDirectory(GetPvmDefaultPath()));

  EnsureDefaultSharedDirExistsCallback callback;
  EnsureDefaultSharedDirExists(&profile_, callback.Get());
  EXPECT_CALL(callback, Run(GetPvmDefaultPath(), true));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, FileAlreadyExists) {
  EXPECT_TRUE(base::CreateDirectory(GetMyFilesFolderPath()));
  EXPECT_TRUE(base::WriteFile(GetPvmDefaultPath(), ""));

  EnsureDefaultSharedDirExistsCallback callback;
  EnsureDefaultSharedDirExists(&profile_, callback.Get());
  EXPECT_CALL(callback, Run(GetPvmDefaultPath(), false));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, LaunchPluginVmApp) {
  using MockPluginVmManager = testing::StrictMock<test::MockPluginVmManager>;
  using AppLaunchedCallback =
      testing::StrictMock<base::MockCallback<LaunchPluginVmAppCallback>>;
  using LaunchContainerApplicationCallback = chromeos::DBusMethodCallback<
      vm_tools::cicerone::LaunchContainerApplicationResponse>;

  auto& plugin_vm_manager = *static_cast<MockPluginVmManager*>(
      PluginVmManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile_,
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return std::make_unique<MockPluginVmManager>();
              })));
  ash::ShelfModel shelf_model;
  ChromeLauncherController chrome_launcher_controller(&profile_, &shelf_model);
  chrome_launcher_controller.SetProfileForTest(&profile_);
  chrome_launcher_controller.SetLauncherControllerHelperForTest(
      std::make_unique<LauncherControllerHelper>(&profile_));
  chrome_launcher_controller.Init();

  AppLaunchedCallback app_launched_callback;
  PluginVmManager::LaunchPluginVmCallback launch_plugin_vm_callback;
  EXPECT_CALL(plugin_vm_manager, LaunchPluginVm(testing::_))
      .WillOnce(testing::Invoke(
          [&](PluginVmManager::LaunchPluginVmCallback callback) {
            launch_plugin_vm_callback = std::move(callback);
          }));
  LaunchPluginVmApp(&profile_, app_id_,
                    {GetMyFilesFileSystemURL("PvmDefault/file")},
                    app_launched_callback.Get());
  ASSERT_FALSE(launch_plugin_vm_callback.is_null());

  LaunchContainerApplicationCallback cicerone_response_callback;
  static_cast<chromeos::FakeCiceroneClient*>(
      chromeos::DBusThreadManager::Get()->GetCiceroneClient())
      ->SetOnLaunchContainerApplicationCallback(base::BindLambdaForTesting(
          [&](const vm_tools::cicerone::LaunchContainerApplicationRequest&
                  request,
              LaunchContainerApplicationCallback callback) {
            EXPECT_TRUE(cicerone_response_callback.is_null());
            cicerone_response_callback = std::move(callback);
          }));
  std::move(launch_plugin_vm_callback).Run(/*success=*/true);
  ASSERT_FALSE(cicerone_response_callback.is_null());

  ash::ShelfID shelf_id(kPluginVmShelfAppId);
  auto launcher_item_controller =
      std::make_unique<AppWindowLauncherItemController>(shelf_id);
  MockAppWindowBase mock_window(shelf_id, nullptr);
  launcher_item_controller->AddWindow(&mock_window);
  mock_window.SetController(launcher_item_controller.get());
  shelf_model.SetShelfItemDelegate(ash::ShelfID(kPluginVmShelfAppId),
                                   std::move(launcher_item_controller));
  vm_tools::cicerone::LaunchContainerApplicationResponse response;
  response.set_success(true);
  EXPECT_CALL(mock_window, Activate());
  EXPECT_CALL(app_launched_callback, Run(LaunchPluginVmAppResult::SUCCESS, ""));
  std::move(cicerone_response_callback).Run(std::move(response));
}

TEST_F(PluginVmFilesTest, LaunchAppFail) {
  LaunchPluginVmAppResult actual_result;
  auto capture_result =
      [](LaunchPluginVmAppResult* actual_result, LaunchPluginVmAppResult result,
         const std::string& failure_reason) { *actual_result = result; };

  // Not enabled.
  fake_plugin_vm_features_.set_enabled(false);
  LaunchPluginVmApp(&profile_, app_id_,
                    {GetMyFilesFileSystemURL("PvmDefault/file")},
                    base::BindOnce(capture_result, &actual_result));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(LaunchPluginVmAppResult::FAILED, actual_result);
  fake_plugin_vm_features_.set_enabled(true);

  // Path in MyFiles, but not MyFiles/PvmDefault.
  LaunchPluginVmApp(&profile_, app_id_,
                    {GetMyFilesFileSystemURL("not/in/PvmDefault")},
                    base::BindOnce(capture_result, &actual_result));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(LaunchPluginVmAppResult::FAILED_DIRECTORY_NOT_SHARED,
            actual_result);

  // Path in different volume.
  mount_points_->RegisterFileSystem(
      "other-volume", storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), GetMyFilesFolderPath());
  storage::FileSystemURL url = mount_points_->CreateExternalFileSystemURL(
      url::Origin(), "other-volume", base::FilePath("other/volume"));
  LaunchPluginVmApp(&profile_, app_id_, {url},
                    base::BindOnce(capture_result, &actual_result));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(LaunchPluginVmAppResult::FAILED_DIRECTORY_NOT_SHARED,
            actual_result);
}

}  // namespace plugin_vm
