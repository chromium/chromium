// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/remote_apps/id_generator.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ash/remote_apps/remote_apps_model.h"
#include "chrome/browser/ash/remote_apps/remote_apps_types.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/events/test/event_generator.h"
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

static base::RepeatingCallback<bool(const apps::AppUpdate&)> IconChanged() {
  return base::BindRepeating([](const apps::AppUpdate& update) {
    return !update.StateIsNull() && update.IconKeyChanged();
  });
}

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

  gfx::ImageSkia image_skia;
  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  for (const auto scale : scale_factors) {
    image_skia.AddRepresentation(
        gfx::ImageSkiaRep(bitmap, ui::GetScaleForResourceScaleFactor(scale)));
  }
  return image_skia;
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
  RemoteAppsManagerBrowsertest() {
    // Quick App is used for the current implementation of app pinning.
    scoped_feature_list_.InitAndEnableFeature(
        features::kHomeButtonQuickAppAccess);
  }

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
    app_list_syncable_service_ =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
    app_list_model_updater_ = app_list_syncable_service_->GetModelUpdater();
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
    apps::AppUpdateWaiter waiter(profile_, app_id, IconChanged());
    AddApp(source_id, name, folder_id, icon_url, add_to_front);
    waiter.Await();
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

  void ShowLauncherAppsGrid(bool wait_for_opening_animation) {
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    EXPECT_FALSE(client->GetAppListWindow());
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        AcceleratorAction::kToggleAppList, {});
    ash::AppListTestApi().WaitForBubbleWindow(wait_for_opening_animation);
    EXPECT_TRUE(client->GetAppListWindow());
  }

  std::string LoadExtension(const base::FilePath& extension_path) {
    extensions::ChromeTestExtensionLoader loader(profile_);
    loader.set_location(extensions::mojom::ManifestLocation::kExternalPolicy);
    loader.set_pack_extension(true);
    // When |set_pack_extension_| is true, the |loader| first packs and then
    // loads the extension. The packing step creates a _metadata folder which
    // causes an install warning when loading.
    loader.set_ignore_manifest_warnings(true);
    return loader.LoadExtension(extension_path)->id();
  }

  // Returns a list of app ids following the ordinal increasing order.
  std::vector<std::string> GetAppIdsInOrdinalOrder(
      const std::vector<std::string>& ids) {
    std::vector<std::string> copy_ids(ids);
    std::sort(
        copy_ids.begin(), copy_ids.end(),
        [&](const std::string& id1, const std::string& id2) {
          return app_list_model_updater_->FindItem(id1)->position().LessThan(
              app_list_model_updater_->FindItem(id2)->position());
        });
    return copy_ids;
  }

  void OnReorderAnimationDone(base::OnceClosure closure,
                              bool aborted,
                              AppListGridAnimationStatus status) {
    EXPECT_FALSE(aborted);
    EXPECT_EQ(AppListGridAnimationStatus::kReorderFadeIn, status);
    std::move(closure).Run();
  }

  const std::string& PinnedAppId() {
    return AppListModelProvider::Get()
        ->quick_app_access_model()
        ->quick_app_id();
  }

  void ExpectNoAppIsPinned() {
    // When no app is pinned, QuickAppAccessMode::quick_app_id() returns an
    // empty string.
    EXPECT_EQ(PinnedAppId(), "");
  }

 protected:
  // Launch healthcare application on device (COM_HEALTH_CUJ1_TASK2_WF1).
  void AddScreenplayTag() {
    base::AddTagToTestResult("feature_id",
                             "screenplay-446812cc-07af-4094-bfb2-00150301ede3");
  }

  raw_ptr<app_list::AppListSyncableService, DanglingUntriaged>
      app_list_syncable_service_;
  raw_ptr<AppListModelUpdater, DanglingUntriaged> app_list_model_updater_;
  ash::AppListTestApi app_list_test_api_;
  raw_ptr<RemoteAppsManager, DanglingUntriaged> manager_ = nullptr;
  raw_ptr<MockImageDownloader, DanglingUntriaged> image_downloader_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO: b/316517034 - Enable the test when flakiness issue is resolved.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, DISABLED_AddApp) {
  AddScreenplayTag();

  // Show launcher UI so that app icons are loaded.
  ShowLauncherAppsGrid(/*wait_for_opening_animation=*/false);

  std::string name = "name";
  GURL icon_url("icon_url");
  gfx::ImageSkia icon = CreateTestIcon(32, SK_ColorRED);

  // App has id kId1.
  AddAppAndWaitForIconChange(kExtensionId1, kId1, name, std::string(), icon_url,
                             icon, /*add_to_front=*/false);

  ash::AppListItem* item = GetAppListItem(kId1);
  EXPECT_FALSE(item->is_folder());
  EXPECT_EQ(name, item->name());
  EXPECT_TRUE(item->GetMetadata()->is_ephemeral);

  base::test::TestFuture<apps::IconValuePtr> future;
  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = apps::IconType::kStandard;
  iv->uncompressed = icon;
  apps::ApplyIconEffects(
      profile_, /*app_id=*/std::nullopt, apps::IconEffects::kCrOsStandardIcon,
      /*size_hint_in_dip=*/64, std::move(iv), future.GetCallback());

  // App's icon is the downloaded icon.
  CheckIconsEqual(future.Get()->uncompressed, item->GetDefaultIcon());
}

// Adds an app with an empty icon URL and checks if the app gets assigned the
// default placeholder icon.
// Flaky (b/41483673)
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest,
                       DISABLED_AddAppPlaceholderIcon) {
  // Show launcher UI so that app icons are loaded.
  ShowLauncherAppsGrid(/*wait_for_opening_animation=*/true);

  const std::string name = "name";

  // App has id kId1. The downloader returns an empty image that is replaced by
  // a placeholder icon.
  AddAppAndWaitForIconChange(kExtensionId1, kId1, name, std::string(), GURL(),
                             gfx::ImageSkia(), /*add_to_front=*/false);

  ash::AppListItem* item = GetAppListItem(kId1);
  EXPECT_FALSE(item->is_folder());
  EXPECT_EQ(name, item->name());

  base::test::TestFuture<apps::IconValuePtr> future;
  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = apps::IconType::kStandard;
  iv->uncompressed =
      manager_->GetPlaceholderIcon(kId1, /*size_hint_in_dip=*/64);
  iv->is_placeholder_icon = true;
  apps::ApplyIconEffects(
      profile_, /*app_id=*/std::nullopt, apps::IconEffects::kCrOsStandardIcon,
      /*size_hint_in_dip=*/64, std::move(iv), future.GetCallback());

  // App's icon is placeholder.
  // TODO(https://crbug.com/1345682): add a pixel diff test for this scenario.
  CheckIconsEqual(future.Get()->uncompressed,
                  app_list_test_api_.GetTopLevelItemViewFromId(kId1)
                      ->icon_image_for_test());
  CheckIconsEqual(future.Get()->uncompressed, item->GetDefaultIcon());

  // App list color sorting should still work for placeholder icons.
  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());
  ash::AppListTestApi::ReorderAnimationEndState actual_state;

  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kColor,
      ash::AppListTestApi::MenuType::kAppListNonFolderItemMenu,
      &event_generator,
      /*target_state=*/
      ash::AppListTestApi::ReorderAnimationEndState::kCompleted, &actual_state);
  EXPECT_EQ(ash::AppListTestApi::ReorderAnimationEndState::kCompleted,
            actual_state);
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
  AddScreenplayTag();

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
  EXPECT_TRUE(folder_item->GetMetadata()->is_ephemeral);
  EXPECT_EQ(folder_name, folder_item->name());
  EXPECT_EQ(1u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(kId2));

  ash::AppListItem* item = GetAppListItem(kId2);
  EXPECT_EQ(kId1, item->folder_id());
  EXPECT_TRUE(item->GetMetadata()->is_ephemeral);
}

IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest,
                       AddFolderWithMultipleApps) {
  AddScreenplayTag();

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
  AddScreenplayTag();

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
  AddScreenplayTag();

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

// Remote app list items are not supposed to be synced. This test verifies that
// the added remote app items are marked as ephemeral and are not synced to
// local storage or uploaded to sync data.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, RemoteAppsNotSynced) {
  std::unique_ptr<syncer::FakeSyncChangeProcessor> sync_processor =
      std::make_unique<syncer::FakeSyncChangeProcessor>();
  app_list_syncable_service_->MergeDataAndStartSyncing(
      syncer::APP_LIST, {},
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          sync_processor.get()));
  content::RunAllTasksUntilIdle();

  // App has id kId1.
  AddAppAndWaitForIconChange(kExtensionId1, kId1, "name", std::string(),
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);

  ash::AppListItem* item = GetAppListItem(kId1);
  EXPECT_FALSE(item->is_folder());
  EXPECT_TRUE(item->GetMetadata()->is_ephemeral);

  // Remote app sync item not added to local storage.
  const base::Value::Dict& local_items =
      profile_->GetPrefs()->GetDict(prefs::kAppListLocalState);
  const base::Value::Dict* dict_item = local_items.FindDict(kId1);
  EXPECT_FALSE(dict_item);

  // Remote app sync item not uploaded to sync data.
  for (auto sync_change : sync_processor->changes()) {
    const std::string item_id =
        sync_change.sync_data().GetSpecifics().app_list().item_id();
    EXPECT_NE(item_id, kId1);
  }
}

// Remote folder list items are not supposed to be synced. This test verifies
// that the added remote folder items are marked as ephemeral and are not synced
// to local storage or uploaded to sync data.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, RemoteFoldersNotSynced) {
  std::unique_ptr<syncer::FakeSyncChangeProcessor> sync_processor =
      std::make_unique<syncer::FakeSyncChangeProcessor>();
  app_list_model_updater_->SetActive(true);
  app_list_syncable_service_->MergeDataAndStartSyncing(
      syncer::APP_LIST, {},
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          sync_processor.get()));
  content::RunAllTasksUntilIdle();

  // Folder has id kId1.
  manager_->AddFolder("folder_name", /*add_to_front=*/false);

  // App has id kId2. Parent is kId1.
  AddAppAndWaitForIconChange(kExtensionId1, kId2, "name", kId1,
                             GURL("icon_url"), CreateTestIcon(32, SK_ColorRED),
                             /*add_to_front=*/false);

  ash::AppListItem* item = GetAppListItem(kId1);
  EXPECT_TRUE(item->is_folder());
  EXPECT_TRUE(item->GetMetadata()->is_ephemeral);

  // Remote folder sync item not added to local storage.
  const base::Value::Dict& local_items =
      profile_->GetPrefs()->GetDict(prefs::kAppListLocalState);
  const base::Value::Dict* dict_item = local_items.FindDict(kId1);
  EXPECT_FALSE(dict_item);

  // Remote folder sync item not uploaded to sync data.
  for (auto sync_change : sync_processor->changes()) {
    const std::string item_id =
        sync_change.sync_data().GetSpecifics().app_list().item_id();
    EXPECT_NE(item_id, kId1);
  }
}

// Tests that the kAlphabeticalEphemeralAppFirst sort order moves the remote
// apps and folders to the front of the launcher, before all native items.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest,
                       SortLauncherWithRemoteAppsFirst) {
  AddScreenplayTag();

  // Show launcher UI so that app icons are loaded.
  ShowLauncherAppsGrid(/*wait_for_opening_animation=*/true);

  base::FilePath test_dir_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir_path);
  test_dir_path = test_dir_path.AppendASCII("extensions");

  // Adds 2 remote apps.
  // Make one app name lower case to test case insensitive.
  std::string remote_app1_id =
      AddApp(kExtensionId1, "test app 5", std::string(), GURL(),
             /*add_to_front=*/true);
  std::string remote_app2_id =
      AddApp(kExtensionId1, "Test App 7", std::string(), GURL(),
             /*add_to_front=*/true);

  // Adds remote folder with one remote app inside.
  std::string remote_folder_id =
      manager_->AddFolder("Test App 6 Folder", /*add_to_front=*/true);
  AddApp(kExtensionId1, "Test App 8", remote_folder_id, GURL(),
         /*add_to_front=*/true);

  // Adds 3 native apps.
  std::string app1_id =
      LoadExtension(test_dir_path.AppendASCII("app1"));  // Test App 1
  ASSERT_FALSE(app1_id.empty());
  std::string app2_id =
      LoadExtension(test_dir_path.AppendASCII("app2"));  // Test App 2
  ASSERT_FALSE(app2_id.empty());
  std::string app3_id =
      LoadExtension(test_dir_path.AppendASCII("app4"));  // Test App 4
  ASSERT_FALSE(app3_id.empty());

  // Moves 2 native apps to a native folder.
  const std::string native_folder_id =
      app_list_test_api_.CreateFolderWithApps({app2_id, app3_id});
  ash::AppListItem* folder_item = GetAppListItem(native_folder_id);
  auto folder_metadata = folder_item->CloneMetadata();
  folder_metadata->name = "Test App 2 Folder";
  folder_item->SetMetadata(std::move(folder_metadata));

  // Current order: `Test App 2 Folder` (native), `Test App 1` (native),
  // `Test App 6 Folder` (remote), `Test App 7` (remote), `test app 5` (remote).
  const std::vector<std::string> ids({app1_id, native_folder_id, remote_app1_id,
                                      remote_app2_id, remote_folder_id});
  EXPECT_EQ(
      GetAppIdsInOrdinalOrder(ids),
      std::vector<std::string>({native_folder_id, app1_id, remote_folder_id,
                                remote_app2_id, remote_app1_id}));

  base::RunLoop run_loop;
  app_list_test_api_.AddReorderAnimationCallback(
      base::BindRepeating(&RemoteAppsManagerBrowsertest::OnReorderAnimationDone,
                          base::Unretained(this), run_loop.QuitClosure()));

  manager_->SortLauncherWithRemoteAppsFirst();
  run_loop.Run();

  // Sorted order: `test app 5` (remote), `Test App 6 Folder` (remote),
  // `Test App 7` (remote), `Test App 1` (native), `Test App 2 Folder` (native).
  EXPECT_EQ(
      GetAppIdsInOrdinalOrder(ids),
      std::vector<std::string>({remote_app1_id, remote_folder_id,
                                remote_app2_id, app1_id, native_folder_id}));
}

// Tests that a single remote app can be pinned to the shelf and then unpinned.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, PinAndUnpinSingleApp) {
  // Add a remote app.
  std::string remote_app1_id =
      AddApp(kExtensionId1, "test app 5", std::string(), GURL(),
             /*add_to_front=*/true);

  std::vector<std::string> app_ids_to_pin{remote_app1_id};
  RemoteAppsError error1 = manager_->SetPinnedApps(app_ids_to_pin);

  EXPECT_EQ(error1, RemoteAppsError::kNone);
  EXPECT_EQ(PinnedAppId(), remote_app1_id);

  // Empty list indicates that any currently pinned apps should be unpinned.
  RemoteAppsError error2 = manager_->SetPinnedApps({});

  EXPECT_EQ(error2, RemoteAppsError::kNone);
  ExpectNoAppIsPinned();
}

// Pinning of multiple apps is not yet supported, but API allows it in case we
// will implement this in the future. Test that current implementation doesn't
// pin anything when asked to pin multiple apps.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest,
                       PinningMultipleAppsNotSupported) {
  // Show launcher UI so that app icons are loaded.
  ShowLauncherAppsGrid(/*wait_for_opening_animation=*/true);

  // Adds2 remote apps.
  std::string remote_app1_id =
      AddApp(kExtensionId1, "test app 5", std::string(), GURL(),
             /*add_to_front=*/true);
  std::string remote_app2_id =
      AddApp(kExtensionId1, "Test App 7", std::string(), GURL(),
             /*add_to_front=*/true);

  std::vector<std::string> app_ids_to_pin{remote_app1_id, remote_app2_id};
  RemoteAppsError error = manager_->SetPinnedApps(app_ids_to_pin);

  EXPECT_EQ(error, RemoteAppsError::kPinningMultipleAppsNotSupported);
  ExpectNoAppIsPinned();
}

// Tests that nothing is pinned if we try to use an invalid app id.
IN_PROC_BROWSER_TEST_F(RemoteAppsManagerBrowsertest, PinInvalidApp) {
  // No apps are added so there is nothing to pin.
  std::vector<std::string> app_ids_to_pin{"invalid id"};
  RemoteAppsError error = manager_->SetPinnedApps(app_ids_to_pin);

  EXPECT_EQ(error, RemoteAppsError::kFailedToPinAnApp);
  ExpectNoAppIsPinned();
}

}  // namespace ash
