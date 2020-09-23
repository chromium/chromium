// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"

#include "ash/public/cpp/shelf_model.h"
#include "base/files/file_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/plugin_vm/mock_plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/scoped_set_running_on_chromeos_for_testing.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/mock_base_window.h"

namespace plugin_vm {

using EnsureDefaultSharedDirExistsCallback =
    testing::StrictMock<base::MockCallback<
        base::OnceCallback<void(const base::FilePath& dir, bool result)>>>;

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

class PluginVmFilesTest : public testing::Test {
 protected:
  base::FilePath GetMyFilesFolderPath() {
    return file_manager::util::GetMyFilesFolderForProfile(&profile_);
  }

  base::FilePath GetPvmDefaultPath() {
    return GetMyFilesFolderPath().Append("PvmDefault");
  }

  struct ScopedDBusThreadManager {
    ScopedDBusThreadManager() { chromeos::DBusThreadManager::Initialize(); }
    ~ScopedDBusThreadManager() { chromeos::DBusThreadManager::Shutdown(); }
  } dbus_thread_manager_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release_{kLsbRelease, {}};
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

  const std::string app_id =
      testing::UnitTest::GetInstance()->current_test_info()->name() +
      std::string{":app_id_1"};
  const std::string vm_name = kPluginVmName;
  const std::string container_name = "penguin";

  PluginVmTestHelper test_helper(&profile_);
  auto& plugin_vm_manager = *static_cast<MockPluginVmManager*>(
      PluginVmManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile_,
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return std::make_unique<MockPluginVmManager>();
              })));
  ash::ShelfModel shelf_model;
  ChromeLauncherController chrome_launcher_controller(&profile_, &shelf_model);
  chrome_launcher_controller.Init();

  // Ensure that Plugin VM is allowed.
  test_helper.AllowPluginVm();
  profile_.GetPrefs()->SetBoolean(prefs::kPluginVmImageExists, true);

  AppLaunchedCallback app_launched_callback;
  PluginVmManager::LaunchPluginVmCallback launch_plugin_vm_callback;
  EXPECT_CALL(plugin_vm_manager, LaunchPluginVm(testing::_))
      .WillOnce(testing::Invoke(
          [&](PluginVmManager::LaunchPluginVmCallback callback) {
            launch_plugin_vm_callback = std::move(callback);
          }));
  LaunchPluginVmApp(&profile_,
                    crostini::CrostiniTestHelper::GenerateAppId(app_id, vm_name,
                                                                container_name),
                    {}, app_launched_callback.Get());
  ASSERT_FALSE(launch_plugin_vm_callback.is_null());

  // Add app to app_list.
  {
    vm_tools::apps::ApplicationList app_list;
    app_list.set_vm_type(vm_tools::apps::ApplicationList::PLUGIN_VM);
    app_list.set_vm_name(vm_name);
    app_list.set_container_name(container_name);

    vm_tools::apps::App& app = *app_list.add_apps();
    app.set_desktop_file_id(app_id);
    app.mutable_name()->add_values();

    guest_os::GuestOsRegistryService(&profile_).UpdateApplicationList(app_list);
  }

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

  auto launcher_item_controller =
      std::make_unique<AppWindowLauncherItemController>(
          ash::ShelfID(kPluginVmShelfAppId));
  ui::test::MockBaseWindow mock_window;
  launcher_item_controller->AddWindow(&mock_window);
  shelf_model.SetShelfItemDelegate(ash::ShelfID(kPluginVmShelfAppId),
                                   std::move(launcher_item_controller));
  vm_tools::cicerone::LaunchContainerApplicationResponse response;
  response.set_success(true);
  EXPECT_CALL(mock_window, Activate());
  EXPECT_CALL(app_launched_callback, Run(LaunchPluginVmAppResult::SUCCESS, ""));
  std::move(cicerone_response_callback).Run(std::move(response));
}

}  // namespace plugin_vm
