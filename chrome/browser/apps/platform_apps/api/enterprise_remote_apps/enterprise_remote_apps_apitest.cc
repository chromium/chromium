// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/platform_apps/api/enterprise_remote_apps/enterprise_remote_apps_api.h"

#include <string>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/remote_apps/id_generator.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ash/remote_apps/remote_apps_model.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
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

namespace chrome_apps {
namespace api {

namespace {

constexpr char kApiExtensionRelativePath[] =
    "extensions/remote_apps/extension_api";
constexpr char kMojoExtensionRelativePath[] =
    "extensions/remote_apps/extension_mojo";
constexpr char kExtensionPemRelativePath[] =
    "extensions/remote_apps/remote_apps.pem";
// ID associated with the .pem.
constexpr char kExtensionId[] = "ceddkihciiemhnpnhbndbinppokgoidh";

constexpr char kId1[] = "Id 1";
constexpr char kId2[] = "Id 2";
constexpr char kId3[] = "Id 3";
constexpr char kId4[] = "Id 4";

}  // namespace

// Tests both the Remote Apps Extension API and the Remote Apps private Mojo
// API. The test extensions are found at
// //chrome/test/data/remote_apps/extension_api and extension_mojo
// respectively. Both test extensions implement the same test cases, only
// differing in which API is used.
class RemoteAppsApitest : public policy::DevicePolicyCrosBrowserTest,
                          public testing::WithParamInterface<std::string> {
 public:
  RemoteAppsApitest() {
    // Quick App is used for the current implementation of app pinning.
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kHomeButtonQuickAppAccess);
  }

  // DevicePolicyCrosBrowserTest:
  void SetUp() override {
    app_list::AppListSyncableServiceFactory::SetUseInTesting(true);
    ash::RemoteAppsImpl::SetBypassChecksForTesting(true);
    DevicePolicyCrosBrowserTest::SetUp();
  }

  // DevicePolicyCrosBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kExtensionId);
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
  }

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    SetUpDeviceLocalAccountPolicy();
    ash::SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

    user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    profile_ = ash::ProfileHelper::Get()->GetProfileByUser(user);
  }

  // TODO(b/239145899): Refactor to not use MGS setup any more.
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

  std::string LoadExtension(base::FilePath extension_path,
                            base::FilePath pem_path = base::FilePath()) {
    extensions::ChromeTestExtensionLoader loader(profile_);
    loader.set_location(extensions::mojom::ManifestLocation::kExternalPolicy);
    loader.set_pack_extension(true);
    if (!pem_path.empty())
      loader.set_pem_path(pem_path);

    // When |set_pack_extension_| is true, the |loader| first packs and then
    // loads the extension. The packing step creates a _metadata folder which
    // causes an install warning when loading.
    loader.set_ignore_manifest_warnings(true);
    return loader.LoadExtension(extension_path)->id();
  }

  void LoadExtensionAndRunTest(const std::string& test_name) {
    config_.Set("customArg", base::Value(test_name));
    extensions::TestGetConfigFunction::set_test_config_state(&config_);

    std::unique_ptr<ash::FakeIdGenerator> id_generator =
        std::make_unique<ash::FakeIdGenerator>(
            std::vector<std::string>{kId1, kId2, kId3, kId4});
    ash::RemoteAppsManagerFactory::GetForProfile(profile_)
        ->GetModelForTesting()
        ->SetIdGeneratorForTesting(std::move(id_generator));

    base::FilePath test_dir_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir_path);
    base::FilePath extension_path = test_dir_path.AppendASCII(GetParam());
    base::FilePath pem_path =
        test_dir_path.AppendASCII(kExtensionPemRelativePath);

    std::string extension_id = LoadExtension(extension_path, pem_path);
    ASSERT_FALSE(extension_id.empty());
  }

  ash::AppListItem* GetAppListItem(const std::string& id) {
    return ash::AppListModelProvider::Get()->model()->FindItem(id);
  }

  int GetAppListItemIndex(const std::string& id) {
    ash::AppListModel* const model = ash::AppListModelProvider::Get()->model();
    ash::AppListItemList* const item_list = model->top_level_item_list();

    size_t index;
    if (!item_list->FindItemIndex(id, &index))
      return -1;
    return index;
  }

  bool IsAppListItemInFront(const std::string& id) {
    const int index = GetAppListItemIndex(id);
    DCHECK_GE(index, 0);
    return index == 0;
  }

  bool IsAppListItemLast(const std::string& id) {
    const int index = GetAppListItemIndex(id);
    DCHECK_GE(index, 0);
    const int model_size = ash::AppListModelProvider::Get()
                               ->model()
                               ->top_level_item_list()
                               ->item_count();
    return index == model_size - 1;
  }

  const std::string& PinnedAppId() {
    return ash::AppListModelProvider::Get()
        ->quick_app_access_model()
        ->quick_app_id();
  }

  void ExpectNoAppIsPinned() {
    // When no app is pinned, QuickAppAccessMode::quick_app_id() returns an
    // empty string.
    EXPECT_EQ(PinnedAppId(), "");
  }

  // Launch healthcare application on device (COM_HEALTH_CUJ1_TASK2_WF1).
  void AddScreenplayTag() {
    base::AddTagToTestResult("feature_id",
                             "screenplay-446812cc-07af-4094-bfb2-00150301ede3");
  }

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_;
  base::Value::Dict config_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, AddApp) {
  AddScreenplayTag();

  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddApp");
  ASSERT_TRUE(catcher.GetNextResult());

  ash::AppListItem* app = GetAppListItem(kId1);
  EXPECT_FALSE(app->is_folder());
  EXPECT_FALSE(app->is_new_install());
  EXPECT_EQ("App 1", app->name());
  // If `add_to_front` is not set, the item should be added to the back of the
  // app list.
  EXPECT_TRUE(IsAppListItemLast(kId1));
}

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, AddAppBadIconUrl) {
  if (GetParam() != kApiExtensionRelativePath)
    GTEST_SKIP() << "iconUrl validation not done for Mojo API";

  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddAppBadIconUrl");

  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, AddAppNoIconUrl) {
  if (GetParam() != kApiExtensionRelativePath)
    GTEST_SKIP() << "iconUrl validation not done for Mojo API";

  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddAppNoIconUrl");

  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, AddAppToFront) {
  AddScreenplayTag();

  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddAppToFront");
  ASSERT_TRUE(catcher.GetNextResult());

  // Check that App 2 is in front.
  EXPECT_TRUE(IsAppListItemInFront(kId2));
}

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, AddFolderAndApps) {
  AddScreenplayTag();

  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddFolderAndApps");
  ASSERT_TRUE(catcher.GetNextResult());

  ash::AppListItem* folder = GetAppListItem(kId1);
  EXPECT_TRUE(folder->is_folder());
  EXPECT_EQ("Folder 1", folder->name());
  EXPECT_EQ(2u, folder->ChildItemCount());
  EXPECT_TRUE(folder->FindChildItem(kId2));
  EXPECT_TRUE(folder->FindChildItem(kId3));
  EXPECT_FALSE(folder->is_new_install());

  ash::AppListItem* app1 = GetAppListItem(kId2);
  EXPECT_EQ(kId1, app1->folder_id());
  EXPECT_FALSE(app1->is_new_install());

  ash::AppListItem* app2 = GetAppListItem(kId3);
  EXPECT_EQ(kId1, app2->folder_id());
  EXPECT_FALSE(app2->is_new_install());

  EXPECT_TRUE(IsAppListItemLast(kId1));
}

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, AddFolderToFront) {
  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("AddFolderToFront");
  ASSERT_TRUE(catcher.GetNextResult());

  // Check that folder is in front.
  EXPECT_TRUE(IsAppListItemInFront(kId2));
}

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, DeleteApp) {
  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("DeleteApp");
  ASSERT_TRUE(catcher.GetNextResult());

  // Check that app is deleted.
  EXPECT_FALSE(GetAppListItem(kId1));
}

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, DeleteAppInFolder) {
  extensions::ResultCatcher catcher;
  LoadExtensionAndRunTest("DeleteAppInFolder");
  ASSERT_TRUE(catcher.GetNextResult());

  // Check that folder and app are not present.
  EXPECT_FALSE(GetAppListItem(kId1));
  EXPECT_FALSE(GetAppListItem(kId2));
}

IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, OnRemoteAppLaunched) {
  AddScreenplayTag();

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("Remote app added");
  listener.set_extension_id(kExtensionId);
  LoadExtensionAndRunTest("OnRemoteAppLaunched");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ChromeShelfController::instance()->LaunchApp(
      ash::ShelfID(kId1), ash::ShelfLaunchSource::LAUNCH_FROM_APP_LIST,
      /*event_flags=*/0, /*display_id=*/0);
  ASSERT_TRUE(catcher.GetNextResult());
}

// Adds remote and native items and tests that the final order, after calling
// chrome.enterprise.remoteApps.sortLauncher(), is remote apps first, in
// alphabetical, case insensitive order, followed by native apps in
// alphabetical, case insensitive order.
IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, SortLauncher) {
  if (GetParam() != kApiExtensionRelativePath)
    GTEST_SKIP() << "The sortLauncher API method is not available in Mojo API";

  AddScreenplayTag();

  base::FilePath test_dir_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir_path);
  test_dir_path = test_dir_path.AppendASCII("extensions");

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("Ready to sort",
                                        ReplyBehavior::kWillReply);
  listener.set_extension_id(kExtensionId);
  LoadExtensionAndRunTest("AddRemoteItemsForSort");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  std::string app1_id =
      LoadExtension(test_dir_path.AppendASCII("app1"));  // Test App 1
  ASSERT_FALSE(app1_id.empty());
  std::string app2_id =
      LoadExtension(test_dir_path.AppendASCII("app2"));  // Test App 2
  ASSERT_FALSE(app2_id.empty());
  std::string app4_id =
      LoadExtension(test_dir_path.AppendASCII("app4"));  // Test App 4
  ASSERT_FALSE(app4_id.empty());

  // Apps and folders are not ordered. Native and remote apps are added to the
  // front (last app added is now first).
  // Current order: `Test App 4` (native), `Test App 2` (native), `Test App 1`
  // (native), `Test App 6 Folder` (remote), `Test App 7` (remote), `test app 5`
  // (remote).
  int app1_index = GetAppListItemIndex(app1_id);  // Test App 1 (native)
  int app2_index = GetAppListItemIndex(app2_id);  // Test App 2 (native)
  int app4_index = GetAppListItemIndex(app4_id);  // Test App 4 (native)
  int id1_index = GetAppListItemIndex(kId1);      // test app 5 (remote)
  int id2_index = GetAppListItemIndex(kId2);      // Test App 7 (remote)
  int id3_index = GetAppListItemIndex(kId3);      // Test App 6 Folder (remote)
  EXPECT_LT(app4_index, app2_index);              // Test App 4 < Test App 2
  EXPECT_LT(app2_index, app1_index);              // Test App 2 < Test App 1
  EXPECT_LT(app1_index, id3_index);  // Test App 1 < Test App 6 Folder
  EXPECT_LT(id3_index, id2_index);   // Test App 6 Folder < Test App 7
  EXPECT_LT(id2_index, id1_index);   // Test App 7 < test app 5

  // Call chrome.enterprise.remoteApps.sortLauncher().
  listener.Reply("");
  ASSERT_TRUE(catcher.GetNextResult());

  // Verifies that remote apps sorting moves all remote items (apps and folders)
  // to the front, in alphabetical, case insensitive order, followed by native
  // items also in alphabetical, case insensitive order.
  // Sorted order: `test app 5` (remote), `Test App 6 Folder` (remote),
  // `Test App 7` (remote), `App Test 1` (native), `Test App 2` (native),
  // `Test App 4` (native).
  app1_index = GetAppListItemIndex(app1_id);  // Test App 1 (native)
  app2_index = GetAppListItemIndex(app2_id);  // Test App 2 (native)
  app4_index = GetAppListItemIndex(app4_id);  // Test App 4 (native)
  id1_index = GetAppListItemIndex(kId1);      // test app 5 (remote)
  id2_index = GetAppListItemIndex(kId2);      // Test App 7 (remote)
  id3_index = GetAppListItemIndex(kId3);      // Test App 6 Folder (remote)
  EXPECT_LT(id1_index, id3_index);            // test app 5 < Test App 6 Folder
  EXPECT_LT(id3_index, id2_index);            // Test App 6 Folder  < Test App 7
  EXPECT_LT(id2_index, app1_index);           // Test App 7 < Test App 1
  EXPECT_LT(app1_index, app2_index);          // Test App 1 < Test App 2
  EXPECT_LT(app2_index, app4_index);          // Test App 2 < Test App 4
}

// Adds a remote app to the launcher and tests that it can be pinned to the
// shelf.
// TODO(b/279770944): Investigate crashes: when test finishes we get segfault in
// the destructor of QuickAppAccessModel. That can be mitigated by manually
// unpinning the app before the end of the test, i.e. calling
// `ash::AppListModelProvider::Get()->quick_app_access_model()->SetQuickApp("");`
// But then the test becomes flaky: sometimes it crashes in
// `ash::HomeButton::AnimateQuickAppButtonOut()`.
IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, DISABLED_PinSingleApp) {
  if (GetParam() != kApiExtensionRelativePath) {
    GTEST_SKIP() << "The setPinnedApps API method is not available in Mojo API";
  }

  extensions::ResultCatcher catcher;
  // This should pin app with ID `kId1` to the shelf
  LoadExtensionAndRunTest("PinSingleApp");
  ASSERT_TRUE(catcher.GetNextResult());

  EXPECT_EQ(PinnedAppId(), kId1);
}

// Adds multiple remote apps to the launcher and tests that we get an error when
// trying to pin more that one of them.
IN_PROC_BROWSER_TEST_P(RemoteAppsApitest, PinMultipleAppsError) {
  if (GetParam() != kApiExtensionRelativePath) {
    GTEST_SKIP() << "The setPinnedApps API method is not available in Mojo API";
  }

  extensions::ResultCatcher catcher;
  // This will try to pin multiple apps to the shelf which should result in
  // extension error.
  LoadExtensionAndRunTest("PinMultipleAppsError");
  ASSERT_TRUE(catcher.GetNextResult());

  ExpectNoAppIsPinned();
}

INSTANTIATE_TEST_SUITE_P(,
                         RemoteAppsApitest,
                         testing::Values(kApiExtensionRelativePath,
                                         kMojoExtensionRelativePath));

}  // namespace api
}  // namespace chrome_apps
