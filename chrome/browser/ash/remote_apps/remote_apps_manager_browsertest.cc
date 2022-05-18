// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/components/login/auth/user_context.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/test/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/remote_apps/id_generator.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ash/remote_apps/remote_apps_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";
constexpr char kId3[] = "id3";
constexpr char kId4[] = "id4";
constexpr char kMissingId[] = "missing_id";
constexpr char kExtensionId1[] = "extension_id1";
constexpr char kExtensionId2[] = "extension_id2";

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
    app_registry_cache_observation_.Observe(app_registry_cache_);
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

    app_registry_cache_observation_.Reset();
    condition_met_ = true;
    if (callback_)
      std::move(callback_).Run();
  }

  // apps::AppRegistryCache::Observer:
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    app_registry_cache_observation_.Reset();
  }

 private:
  std::string id_;
  apps::AppRegistryCache* app_registry_cache_ = nullptr;
  base::OnceClosure callback_;
  base::RepeatingCallback<bool(const apps::AppUpdate&)> condition_;
  bool condition_met_ = false;
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observation_{this};
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

class MockRemoteAppLaunchObserver
    : public chromeos::remote_apps::mojom::RemoteAppLaunchObserver {
 public:
  MOCK_METHOD(void,
              OnRemoteAppLaunched,
              (const std::string&, const std::string&),
              (override));
};

}  // namespace

class RemoteAppsManagerBrowsertest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  // DevicePolicyCrosBrowserTest:
  void SetUp() override {
    DevicePolicyCrosBrowserTest::SetUp();
    app_list::AppListSyncableServiceFactory::SetUseInTesting(true);
  }

  // DevicePolicyCrosBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  }

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override {
    SetUpDeviceLocalAccountPolicy();
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

  AppListItem* GetAppListItem(const std::string& id) {
    return AppListModelProvider::Get()->model()->FindItem(id);
  }

  std::string AddApp(const std::string& source_id,
                     const std::string& name,
                     const std::string& folder_id,
                     const GURL& icon_url,
                     bool add_to_front) {
    base::test::TestFuture<std::string, RemoteAppsError> future;
    manager_->AddApp(source_id, name, folder_id, icon_url, add_to_front,
                     future.GetCallback<const std::string&, RemoteAppsError>());
    // Ideally ASSERT_EQ should be used, but we are in a non-void function.
    EXPECT_EQ(RemoteAppsError::kNone, future.Get<1>());
    return future.Get<0>();
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

  void AddAppAndWaitForIconChange(const std::string& source_id,
                                  const std::string& app_id,
                                  const std::string& name,
                                  const std::string& folder_id,
                                  const GURL& icon_url,
                                  const gfx::ImageSkia& icon,
                                  bool add_to_front) {
    ExpectImageDownloaderDownload(icon_url, icon);
    AppUpdateWaiter waiter(profile_, app_id, AppUpdateWaiter::IconChanged());
    AddApp(source_id, name, folder_id, icon_url, add_to_front);
    waiter.Wait();
  }

  void AddAppAssertError(const std::string& source_id,
                         RemoteAppsError error,
                         const std::string& name,
                         const std::string& folder_id,
                         const GURL& icon_url,
                         bool add_to_front) {
    base::test::TestFuture<std::string, RemoteAppsError> future;
    manager_->AddApp(source_id, name, folder_id, icon_url, add_to_front,
                     future.GetCallback<const std::string&, RemoteAppsError>());
    ASSERT_EQ(error, future.Get<1>());
  }

  void ShowLauncherAppsGrid() {
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    EXPECT_FALSE(client->GetAppListWindow());
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        ash::TOGGLE_APP_LIST_FULLSCREEN, {});
    if (ash::features::IsProductivityLauncherEnabled()) {
      ash::AppListTestApi().WaitForBubbleWindow(
          /*wait_for_opening_animation=*/false);
    }
    EXPECT_TRUE(client->GetAppListWindow());
  }

 protected:
  RemoteAppsManager* manager_ = nullptr;
  MockImageDownloader* image_downloader_ = nullptr;
  Profile* profile_ = nullptr;
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddApp) {
  // Show launcher UI so that app icons are loaded.
  ShowLauncherAppsGrid();

  std::string name = "name";
  GURL icon_url("icon_url");
  gfx::ImageSkia icon = CreateTestIcon(32, SK_ColorRED);

  // App has id kId1.
  AddAppAndWaitForIconChange(kExtensionId1, kId1, name, std::string(), icon_url,
                             icon,
                             /*add_to_front=*/false);

  ash::AppListItem* item = GetAppListItem(kId1);
  EXPECT_FALSE(item->is_folder());
  EXPECT_EQ(name, item->name());
  // kShared uses size hint 64 dip.
  apps::IconEffects icon_effects = apps::IconEffects::kCrOsStandardIcon;

  base::test::TestFuture<apps::IconValuePtr> future;
  auto output_data = std::make_unique<apps::IconValue>();
  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = apps::IconType::kStandard;
  iv->uncompressed = icon;
  iv->is_placeholder_icon = true;
  apps::ApplyIconEffects(icon_effects, 64, std::move(iv), future.GetCallback());
  CheckIconsEqual(future.Get()->uncompressed, item->GetDefaultIcon());
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddAppError) {
  std::string name = "name";
  GURL icon_url("icon_url");
  gfx::ImageSkia icon = CreateTestIcon(32, SK_ColorRED);

  AddAppAssertError(kExtensionId1, RemoteAppsError::kFolderIdDoesNotExist, name,
                    kMissingId, icon_url, /*add_to_front=*/false);
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddAppErrorNotReady) {
  std::string name = "name";
  GURL icon_url("icon_url");
  gfx::ImageSkia icon = CreateTestIcon(32, SK_ColorRED);

  manager_->SetIsInitializedForTesting(false);
  AddAppAssertError(kExtensionId1, RemoteAppsError::kNotReady, name,
                    std::string(), icon_url,
                    /*add_to_front=*/false);
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, DeleteApp) {
  // App has id kId1.
  AddAppAndWaitForIconChange(kExtensionId1, kId1, "name", std::string(),
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
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
  AddAppAndWaitForIconChange(kExtensionId1, kId2, "name", kId1,
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
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
  AddAppAndWaitForIconChange(kExtensionId1, kId2, "name", kId1,
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);
  // App has id kId3.
  AddAppAndWaitForIconChange(kExtensionId1, kId3, "name2", kId1,
                             GURL("icon_url2"),
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
  AddAppAndWaitForIconChange(kExtensionId1, kId4, "name3", kId1,
                             GURL("icon_url3"),
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
  AddAppAndWaitForIconChange(kExtensionId1, kId2, "name", kId1,
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);
  // App has id kId3.
  AddAppAndWaitForIconChange(kExtensionId1, kId3, "name2", kId1,
                             GURL("icon_url2"),
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

// Verifies that folders are not removed after user moves all but single item
// from them.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest,
                       DontRemoveSingleItemFolders) {
  // Folder has id kId1.
  manager_->AddFolder("folder_name", /*add_to_front=*/false);

  // App has id kId2.
  AddAppAndWaitForIconChange(kExtensionId1, kId2, "name", kId1,
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);
  // App has id kId3.
  AddAppAndWaitForIconChange(kExtensionId1, kId3, "name2", kId1,
                             GURL("icon_url2"),
                             CreateTestIcon(32, SK_ColorBLUE),
                             /*add_to_front=*/false);

  ash::AppListItem* folder_item = GetAppListItem(kId1);
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(kId2));
  EXPECT_TRUE(folder_item->FindChildItem(kId3));

  // Move kId2 item to root app list.
  ash::AppListItem* item1 = GetAppListItem(kId2);
  ASSERT_TRUE(item1);
  ash::AppListModelProvider::Get()->model()->MoveItemToRootAt(
      item1, folder_item->position().CreateBefore());

  ASSERT_EQ(folder_item, GetAppListItem(kId1));
  EXPECT_EQ(1u, folder_item->ChildItemCount());
  EXPECT_FALSE(folder_item->FindChildItem(kId2));
  EXPECT_TRUE(folder_item->FindChildItem(kId3));
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, AddToFront) {
  // Folder has id kId1.
  manager_->AddFolder("folder_name", /*add_to_front=*/false);

  // App has id kId2.
  AddAppAndWaitForIconChange(kExtensionId1, kId2, "name", std::string(),
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);

  EXPECT_FALSE(manager_->ShouldAddToFront(kId1));
  EXPECT_FALSE(manager_->ShouldAddToFront(kId2));

  // Folder has id kId3.
  manager_->AddFolder("folder_name2", /*add_to_front=*/true);

  // App has id kId4.
  AddAppAndWaitForIconChange(kExtensionId1, kId4, "name2", kId3,
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/true);

  EXPECT_TRUE(manager_->ShouldAddToFront(kId3));
  // |add_to_front| disabled since app has a parent folder.
  EXPECT_FALSE(manager_->ShouldAddToFront(kId4));
}

// Test that app launched events are only dispatched to the extension which
// added the app, and the all events are dispatched to the Lacros observer.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, OnAppLaunched) {
  base::test::TestFuture<std::string>
      on_remote_app_launched_with_app_id1_future;
  base::test::TestFuture<std::string>
      on_remote_app_launched_with_app_id2_future;
  base::test::TestFuture<std::string>
      on_remote_app_launched_with_app_id1_to_proxy_future;
  base::test::TestFuture<std::string>
      on_remote_app_launched_with_app_id2_to_proxy_future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver1;
  EXPECT_CALL(mockObserver1, OnRemoteAppLaunched(kId1, kExtensionId1))
      .WillOnce([&on_remote_app_launched_with_app_id1_future](
                    const std::string& app_id, const std::string& source_id) {
        on_remote_app_launched_with_app_id1_future.SetValue(app_id);
      });
  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver2;
  EXPECT_CALL(mockObserver2, OnRemoteAppLaunched(kId2, kExtensionId2))
      .WillOnce([&on_remote_app_launched_with_app_id2_future](
                    const std::string& app_id, const std::string& source_id) {
        on_remote_app_launched_with_app_id2_future.SetValue(app_id);
      });

  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote1;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote2;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer1{&mockObserver1};
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer2{&mockObserver2};
  manager_->BindRemoteAppsAndAppLaunchObserver(
      kExtensionId1, remote1.BindNewPipeAndPassReceiver(),
      observer1.BindNewPipeAndPassRemote());
  manager_->BindRemoteAppsAndAppLaunchObserver(
      kExtensionId2, remote2.BindNewPipeAndPassReceiver(),
      observer2.BindNewPipeAndPassRemote());

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver3;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote3;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      proxyObserver{&mockObserver3};
  manager_->BindRemoteAppsAndAppLaunchObserverForLacros(
      remote3.BindNewPipeAndPassReceiver(),
      proxyObserver.BindNewPipeAndPassRemote());

  EXPECT_CALL(mockObserver3, OnRemoteAppLaunched(kId1, kExtensionId1))
      .WillOnce([&on_remote_app_launched_with_app_id1_to_proxy_future](
                    const std::string& app_id, const std::string& source_id) {
        on_remote_app_launched_with_app_id1_to_proxy_future.SetValue(app_id);
      });

  // App has id kId1, added by kExtensionId1.
  AddAppAndWaitForIconChange(kExtensionId1, kId1, "name", std::string(),
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);

  // App has id kId2, added by kExtensionId2.
  AddAppAndWaitForIconChange(kExtensionId2, kId2, "name", std::string(),
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);

  manager_->LaunchApp(kId1);
  ASSERT_EQ(kId1, on_remote_app_launched_with_app_id1_future.Get());
  ASSERT_EQ(kId1, on_remote_app_launched_with_app_id1_to_proxy_future.Get());
  ASSERT_FALSE(on_remote_app_launched_with_app_id2_future.IsReady());

  EXPECT_CALL(mockObserver3, OnRemoteAppLaunched(kId2, kExtensionId2))
      .WillOnce([&on_remote_app_launched_with_app_id2_to_proxy_future](
                    const std::string& app_id, const std::string& source_id) {
        on_remote_app_launched_with_app_id2_to_proxy_future.SetValue(app_id);
      });

  manager_->LaunchApp(kId2);
  ASSERT_EQ(kId2, on_remote_app_launched_with_app_id2_future.Get());
  ASSERT_EQ(kId2, on_remote_app_launched_with_app_id2_to_proxy_future.Get());
}

}  // namespace ash
