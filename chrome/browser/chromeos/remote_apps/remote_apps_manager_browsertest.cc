// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/remote_apps/remote_apps_manager.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/remote_apps/id_generator.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace chromeos {

namespace {

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";
constexpr char kId3[] = "id3";
constexpr char kId4[] = "id4";
constexpr char kMissingId[] = "missing_id";

class AppUpdateWaiter : public apps::AppRegistryCache::Observer {
 public:
  static base::RepeatingCallback<bool(const apps::AppUpdate&)> IconChanged() {
    return base::BindRepeating([](const apps::AppUpdate& update) {
      return !update.StateIsNull() && update.IconKeyChanged();
    });
  }

  AppUpdateWaiter(
      Profile* profile,
      const std::string& id,
      base::RepeatingCallback<bool(const apps::AppUpdate&)> condition =
          base::BindRepeating([](const apps::AppUpdate& update) {
            return true;
          }))
      : id_(id), condition_(condition) {
    app_registry_cache_ = &apps::AppServiceProxyFactory::GetForProfile(profile)
                               ->AppRegistryCache();
    app_registry_cache_observer_.Add(app_registry_cache_);
  }

  void Wait() {
    if (!condition_met_) {
      base::RunLoop run_loop;
      callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    // Allow updates to propagate to other observers.
    base::RunLoop().RunUntilIdle();
  }

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override {
    if (condition_met_ || update.AppId() != id_ || !condition_.Run(update))
      return;

    app_registry_cache_observer_.RemoveAll();
    condition_met_ = true;
    if (callback_)
      std::move(callback_).Run();
  }

  // apps::AppRegistryCache::Observer:
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    app_registry_cache_observer_.RemoveAll();
  }

 private:
  std::string id_;
  apps::AppRegistryCache* app_registry_cache_ = nullptr;
  base::OnceClosure callback_;
  base::RepeatingCallback<bool(const apps::AppUpdate&)> condition_;
  bool condition_met_ = false;
  ScopedObserver<apps::AppRegistryCache, apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

class MockImageDownloader : public RemoteAppsManager::ImageDownloader {
 public:
  MOCK_METHOD(void,
              Download,
              (const GURL& url,
               base::OnceCallback<void(const gfx::ImageSkia&)> callback),
              (override));
};

gfx::ImageSkia CreateTestIcon(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFromBitmap(bitmap, 1.0f);
}

void CheckIconsEqual(const gfx::ImageSkia& expected,
                     const gfx::ImageSkia& actual) {
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(expected.GetRepresentation(1.0f).GetBitmap(),
                                 actual.GetRepresentation(1.0f).GetBitmap()));
}

}  // namespace

class RemoteAppsManagerBrowsertest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  RemoteAppsManagerBrowsertest() : policy::DevicePolicyCrosBrowserTest() {}

  // DevicePolicyCrosBrowserTest:
  void SetUp() override {
    DevicePolicyCrosBrowserTest::SetUp();
    app_list::AppListSyncableServiceFactory::SetUseInTesting(true);
  }

  // DevicePolicyCrosBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  }

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override {
    SetUpDeviceLocalAccountPolicy();
    WizardController::SkipPostLoginScreensForTesting();
    SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

    user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    profile_ = ProfileHelper::Get()->GetProfileByUser(user);
    manager_ = RemoteAppsManagerFactory::GetForProfile(profile_);
    std::unique_ptr<FakeIdGenerator> id_generator =
        std::make_unique<FakeIdGenerator>(
            std::vector<std::string>{kId1, kId2, kId3, kId4});
    manager_->GetModelForTesting()->SetIdGeneratorForTesting(
        std::move(id_generator));
    std::unique_ptr<MockImageDownloader> image_downloader =
        std::make_unique<MockImageDownloader>();
    image_downloader_ = image_downloader.get();
    manager_->SetImageDownloaderForTesting(std::move(image_downloader));
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

  void ExpectImageDownloaderDownload(const GURL& icon_url,
                                     const gfx::ImageSkia& icon) {
    EXPECT_CALL(*image_downloader_, Download(icon_url, testing::_))
        .WillOnce(
            [icon](const GURL& icon_url,
                   base::OnceCallback<void(const gfx::ImageSkia&)> callback) {
              std::move(callback).Run(icon);
            });
  }

  ash::AppListItem* GetAppListItem(const std::string& id) {
    ash::AppListControllerImpl* controller =
        ash::Shell::Get()->app_list_controller();
    ash::AppListModel* model = controller->GetModel();
    return model->FindItem(id);
  }

  std::string AddApp(const std::string& name,
                     const std::string& folder_id,
                     const GURL& icon_url,
                     bool add_to_front) {
    base::RunLoop run_loop;
    std::string id;
    manager_->AddApp(name, folder_id, icon_url, add_to_front,
                     base::BindOnce(
                         [](base::RepeatingClosure closure, std::string* id_arg,
                            const std::string& id, RemoteAppsError error) {
                           ASSERT_EQ(RemoteAppsError::kNone, error);

                           ash::AppListControllerImpl* controller =
                               ash::Shell::Get()->app_list_controller();
                           ash::AppListModel* model = controller->GetModel();
                           ASSERT_TRUE(model->FindItem(id));
                           *id_arg = id;

                           closure.Run();
                         },
                         run_loop.QuitClosure(), &id));
    run_loop.Run();
    return id;
  }

  RemoteAppsError DeleteApp(const std::string& id) {
    RemoteAppsError error = manager_->DeleteApp(id);
    // Allow updates to propagate to AppList.
    base::RunLoop().RunUntilIdle();
    return error;
  }

  RemoteAppsError DeleteFolder(const std::string& id) {
    RemoteAppsError error = manager_->DeleteFolder(id);
    // Allow updates to propagate to AppList.
    base::RunLoop().RunUntilIdle();
    return error;
  }

  void AddAppAndWaitForIconChange(const std::string& id,
                                  const std::string& name,
                                  const std::string& folder_id,
                                  const GURL& icon_url,
                                  const gfx::ImageSkia& icon,
                                  bool add_to_front) {
    ExpectImageDownloaderDownload(icon_url, icon);
    AppUpdateWaiter waiter(profile_, id, AppUpdateWaiter::IconChanged());
    AddApp(name, folder_id, icon_url, add_to_front);
    waiter.Wait();
  }

  void AddAppAssertError(RemoteAppsError error,
                         const std::string& name,
                         const std::string& folder_id,
                         const GURL& icon_url,
                         bool add_to_front) {
    base::RunLoop run_loop;
    manager_->AddApp(
        name, folder_id, icon_url, add_to_front,
        base::BindOnce(
            [](base::RepeatingClosure closure, RemoteAppsError expected_error,
               const std::string& id, RemoteAppsError error) {
              ASSERT_EQ(expected_error, error);
              closure.Run();
            },
            run_loop.QuitClosure(), error));
    run_loop.Run();
  }

 protected:
  RemoteAppsManager* manager_ = nullptr;
  MockImageDownloader* image_downloader_ = nullptr;
  Profile* profile_ = nullptr;
  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddApp) {
  std::string name = "name";
  GURL icon_url("icon_url");
  gfx::ImageSkia icon = CreateTestIcon(32, SK_ColorRED);

  // App has id kId1.
  AddAppAndWaitForIconChange(kId1, name, std::string(), icon_url, icon,
                             /*add_to_front=*/false);

  ash::AppListItem* item = GetAppListItem(kId1);
  EXPECT_FALSE(item->is_folder());
  EXPECT_EQ(name, item->name());
  // kShared uses size hint 64 dip.
  apps::IconEffects icon_effects =
      base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)
          ? apps::IconEffects::kCrOsStandardIcon
          : apps::IconEffects::kResizeAndPad;
  apps::ApplyIconEffects(icon_effects, 64, &icon);
  CheckIconsEqual(icon, item->GetDefaultIcon());
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddAppError) {
  std::string name = "name";
  GURL icon_url("icon_url");
  gfx::ImageSkia icon = CreateTestIcon(32, SK_ColorRED);

  AddAppAssertError(RemoteAppsError::kFolderIdDoesNotExist, name, kMissingId,
                    icon_url, /*add_to_front=*/false);
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddAppErrorNotReady) {
  std::string name = "name";
  GURL icon_url("icon_url");
  gfx::ImageSkia icon = CreateTestIcon(32, SK_ColorRED);

  manager_->SetIsInitializedForTesting(false);
  AddAppAssertError(RemoteAppsError::kNotReady, name, std::string(), icon_url,
                    /*add_to_front=*/false);
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, DeleteApp) {
  // App has id kId1.
  AddAppAndWaitForIconChange(kId1, "name", std::string(), GURL("icon_url"),
                             CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);

  RemoteAppsError error = DeleteApp(kId1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(RemoteAppsError::kNone, error);
  EXPECT_FALSE(GetAppListItem(kId1));
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, DeleteAppError) {
  EXPECT_EQ(RemoteAppsError::kAppIdDoesNotExist, DeleteApp(kMissingId));
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddAndDeleteFolder) {
  manager_->AddFolder("folder_name", /*add_to_front=*/false);
  // Empty folder has no AppListItem.
  EXPECT_FALSE(GetAppListItem(kId1));

  EXPECT_EQ(RemoteAppsError::kNone, DeleteFolder(kId1));
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, DeleteFolderError) {
  EXPECT_EQ(RemoteAppsError::kFolderIdDoesNotExist, DeleteFolder(kMissingId));
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddFolderAndApp) {
  std::string folder_name = "folder_name";
  // Folder has id kId1.
  manager_->AddFolder(folder_name, /*add_to_front=*/false);
  // Empty folder has no item.
  EXPECT_FALSE(GetAppListItem(kId1));

  // App has id kId2.
  AddAppAndWaitForIconChange(kId2, "name", kId1, GURL("icon_url"),
                             CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);

  // Folder item was created.
  ash::AppListItem* folder_item = GetAppListItem(kId1);
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(folder_name, folder_item->name());
  EXPECT_EQ(1u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(kId2));

  ash::AppListItem* item = GetAppListItem(kId2);
  EXPECT_EQ(kId1, item->folder_id());
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest,
                       AddFolderWithMultipleApps) {
  // Folder has id kId1.
  manager_->AddFolder("folder_name", /*add_to_front=*/false);

  // App has id kId2.
  AddAppAndWaitForIconChange(kId2, "name", kId1, GURL("icon_url"),
                             CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);
  // App has id kId3.
  AddAppAndWaitForIconChange(kId3, "name2", kId1, GURL("icon_url2"),
                             CreateTestIcon(32, SK_ColorBLUE),
                             /*add_to_front=*/false);

  ash::AppListItem* folder_item = GetAppListItem(kId1);
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(kId2));
  EXPECT_TRUE(folder_item->FindChildItem(kId3));

  DeleteApp(kId2);
  folder_item = GetAppListItem(kId1);
  EXPECT_EQ(1u, folder_item->ChildItemCount());
  EXPECT_FALSE(folder_item->FindChildItem(kId2));

  DeleteApp(kId3);
  // Empty folder is removed.
  EXPECT_FALSE(GetAppListItem(kId1));

  // App has id kId4.
  AddAppAndWaitForIconChange(kId4, "name3", kId1, GURL("icon_url3"),
                             CreateTestIcon(32, SK_ColorGREEN),
                             /*add_to_front=*/false);

  // Folder is re-created.
  folder_item = GetAppListItem(kId1);
  EXPECT_EQ(1u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(kId4));
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest,
                       DeleteFolderWithMultipleApps) {
  // Folder has id kId1.
  manager_->AddFolder("folder_name", /*add_to_front=*/false);

  // App has id kId2.
  AddAppAndWaitForIconChange(kId2, "name", kId1, GURL("icon_url"),
                             CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);
  // App has id kId3.
  AddAppAndWaitForIconChange(kId3, "name2", kId1, GURL("icon_url2"),
                             CreateTestIcon(32, SK_ColorBLUE),
                             /*add_to_front=*/false);

  DeleteFolder(kId1);
  // Folder is removed.
  EXPECT_FALSE(GetAppListItem(kId1));

  // Apps are moved to top-level.
  ash::AppListItem* item1 = GetAppListItem(kId2);
  EXPECT_EQ(std::string(), item1->folder_id());
  ash::AppListItem* item2 = GetAppListItem(kId3);
  EXPECT_EQ(std::string(), item2->folder_id());
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddToFront) {
  // Folder has id kId1.
  manager_->AddFolder("folder_name", /*add_to_front=*/false);

  // App has id kId2.
  AddAppAndWaitForIconChange(kId2, "name", std::string(), GURL("icon_url"),
                             CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);

  EXPECT_FALSE(manager_->ShouldAddToFront(kId1));
  EXPECT_FALSE(manager_->ShouldAddToFront(kId2));

  // Folder has id kId3.
  manager_->AddFolder("folder_name2", /*add_to_front=*/true);

  // App has id kId4.
  AddAppAndWaitForIconChange(kId4, "name2", kId3, GURL("icon_url"),
                             CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/true);

  EXPECT_TRUE(manager_->ShouldAddToFront(kId3));
  // |add_to_front| disabled since app has a parent folder.
  EXPECT_FALSE(manager_->ShouldAddToFront(kId4));
}

}  // namespace chromeos
