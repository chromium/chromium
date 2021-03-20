// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/remote_apps/remote_apps_impl.h"

#include <string>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/remote_apps/id_generator.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_manager.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_model.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// ID of extension found at chrome/test/data/remote_apps.
constexpr char kExtensionId[] = "ceddkihciiemhnpnhbndbinppokgoidh";

constexpr char kId1[] = "Id 1";
constexpr char kId2[] = "Id 2";
constexpr char kId3[] = "Id 3";

}  // namespace

class RemoteAppsImplBrowsertest : public policy::DevicePolicyCrosBrowserTest {
 public:
  RemoteAppsImplBrowsertest() : policy::DevicePolicyCrosBrowserTest() {}

  // DevicePolicyCrosBrowserTest:
  void SetUp() override {
    app_list::AppListSyncableServiceFactory::SetUseInTesting(true);
    RemoteAppsImpl::SetBypassChecksForTesting(true);
    DevicePolicyCrosBrowserTest::SetUp();
  }

  // DevicePolicyCrosBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kExtensionId);
  }

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    SetUpDeviceLocalAccountPolicy();
    WizardController::SkipPostLoginScreensForTesting();
    SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();
  }

  void SetUpDeviceLocalAccountPolicy() {
    enterprise_management::DeviceLocalAccountsProto* const
        device_local_accounts =
            device_policy()->payload().mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id("user@test");
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    device_local_accounts->set_auto_login_id("user@test");
    device_local_accounts->set_auto_login_delay(0);
    RefreshDevicePolicy();
  }

  void LoadExtensionAndRunTest(const std::string& test_name) {
    config_.SetKey("customArg", base::Value(test_name));
    extensions::TestGetConfigFunction::set_test_config_state(&config_);

    base::FilePath test_dir_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir_path);
    base::FilePath extension_path =
        test_dir_path.AppendASCII("extensions/remote_apps/extension");
    base::FilePath pem_path =
        test_dir_path.AppendASCII("extensions/remote_apps/remote_apps.pem");

    user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);

    std::unique_ptr<FakeIdGenerator> id_generator =
        std::make_unique<FakeIdGenerator>(
            std::vector<std::string>{kId1, kId2, kId3});
    RemoteAppsManagerFactory::GetForProfile(profile)
        ->GetModelForTesting()
        ->SetIdGeneratorForTesting(std::move(id_generator));

    extensions::ChromeTestExtensionLoader loader(profile);
    loader.set_location(extensions::mojom::ManifestLocation::kExternalPolicy);
    loader.set_pack_extension(true);
    loader.set_pem_path(pem_path);
    // When |set_pack_extension_| is true, the |loader| first packs and then
    // loads the extension. The packing step creates a _metadata folder which
    // causes an install warning when loading.
    loader.set_ignore_manifest_warnings(true);
    ASSERT_TRUE(loader.LoadExtension(extension_path));
  }

  ash::AppListItem* GetAppListItem(const std::string& id) {
    ash::AppListControllerImpl* controller =
        ash::Shell::Get()->app_list_controller();
    ash::AppListModel* model = controller->GetModel();
    return model->FindItem(id);
  }

  bool IsAppListItemInFront(const std::string& id) {
    ash::AppListControllerImpl* controller =
        ash::Shell::Get()->app_list_controller();
    ash::AppListModel* model = controller->GetModel();
    ash::AppListItemList* item_list = model->top_level_item_list();

    size_t index;
    if (!item_list->FindItemIndex(id, &index))
      return false;

    return index == 0;
  }

 private:
  base::DictionaryValue config_;
  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(RemoteAppsImplBrowsertest, AddApp) {
  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddApp");
  ASSERT_TRUE(catcher.GetNextResult());

  ash::AppListItem* app = GetAppListItem(kId1);
  EXPECT_FALSE(app->is_folder());
  EXPECT_EQ("App 1", app->name());
}

IN_PROC_BROWSER_TEST_F(RemoteAppsImplBrowsertest, AddAppToFront) {
  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddAppToFront");
  ASSERT_TRUE(catcher.GetNextResult());

  // Check that App 2 is in front.
  EXPECT_TRUE(IsAppListItemInFront(kId2));
}

IN_PROC_BROWSER_TEST_F(RemoteAppsImplBrowsertest, AddFolderAndApps) {
  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddFolderAndApps");
  ASSERT_TRUE(catcher.GetNextResult());

  ash::AppListItem* folder = GetAppListItem(kId1);
  EXPECT_TRUE(folder->is_folder());
  EXPECT_EQ("Folder 1", folder->name());
  EXPECT_EQ(2u, folder->ChildItemCount());
  EXPECT_TRUE(folder->FindChildItem(kId2));
  EXPECT_TRUE(folder->FindChildItem(kId3));

  ash::AppListItem* app1 = GetAppListItem(kId2);
  EXPECT_EQ(kId1, app1->folder_id());

  ash::AppListItem* app2 = GetAppListItem(kId3);
  EXPECT_EQ(kId1, app2->folder_id());
}

IN_PROC_BROWSER_TEST_F(RemoteAppsImplBrowsertest, AddFolderToFront) {
  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddFolderToFront");
  ASSERT_TRUE(catcher.GetNextResult());

  // Check that folder is in front.
  EXPECT_TRUE(IsAppListItemInFront(kId2));
}

IN_PROC_BROWSER_TEST_F(RemoteAppsImplBrowsertest, OnRemoteAppLaunched) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("Remote app added",
                                        /*will_reply=*/false);
  listener.set_extension_id(kExtensionId);
  LoadExtensionAndRunTest("OnRemoteAppLaunched");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ChromeLauncherController::instance()->LaunchApp(
      ash::ShelfID(kId1), ash::ShelfLaunchSource::LAUNCH_FROM_APP_LIST,
      /*event_flags=*/0, /*display_id=*/0);
  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace chromeos
