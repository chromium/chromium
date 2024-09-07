// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_drivefs_delegate.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/wallpaper/test_wallpaper_drivefs_delegate.h"
#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_pref_manager.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/test_personalization_app_webui_provider.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace ash::personalization_app {

namespace {

constexpr char kDummyUrl[] = "data:image/png;base64,20";
constexpr char kDummyCollectionId[] = "fake_collection_id_0";

const uint64_t kAssetId = 20;

void PutWallpaperInfoInPrefs(AccountId account_id,
                             WallpaperInfo info,
                             PrefService* pref_service,
                             const std::string& pref_name) {
  DCHECK(pref_service);
  ScopedDictPrefUpdate wallpaper_update(pref_service, pref_name);
  base::Value::Dict wallpaper_info_dict = info.ToDict();
  wallpaper_update->Set(account_id.GetUserEmail(),
                        std::move(wallpaper_info_dict));
}

// Helper class to block until wallpaper colors have updated.
class WallpaperChangedWaiter : public WallpaperControllerObserver {
 public:
  explicit WallpaperChangedWaiter(base::OnceClosure on_wallpaper_changed)
      : on_wallpaper_changed_(std::move(on_wallpaper_changed)) {
    wallpaper_controller_observation_.Observe(WallpaperController::Get());
  }

  WallpaperChangedWaiter(const WallpaperChangedWaiter&) = delete;
  WallpaperChangedWaiter& operator=(const WallpaperChangedWaiter&) = delete;

  ~WallpaperChangedWaiter() override = default;

  void OnWallpaperChanged() override {
    DCHECK(on_wallpaper_changed_);
    std::move(on_wallpaper_changed_).Run();
  }

 private:
  base::OnceClosure on_wallpaper_changed_;
  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observation_{this};
};

class PersonalizationAppWallpaperInfoBrowserTest
    : public SystemWebAppBrowserTestBase {
 public:
  PersonalizationAppWallpaperInfoBrowserTest() = default;

  PersonalizationAppWallpaperInfoBrowserTest(
      const PersonalizationAppWallpaperInfoBrowserTest&) = delete;
  PersonalizationAppWallpaperInfoBrowserTest& operator=(
      const PersonalizationAppWallpaperInfoBrowserTest&) = delete;

  ~PersonalizationAppWallpaperInfoBrowserTest() override = default;

  // BrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    WallpaperControllerImpl::SetWallpaperImageDownloaderForTesting(
        std::make_unique<TestWallpaperImageDownloader>());
    SystemWebAppBrowserTestBase::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    SystemWebAppBrowserTestBase::SetUpOnMainThread();

    browser()->window()->Minimize();

    wallpaper_controller()->OverrideDriveFsDelegateForTesting(
        std::make_unique<TestWallpaperDriveFsDelegate>());
    WallpaperControllerClientImpl::Get()->SetWallpaperFetcherDelegateForTesting(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());

    auto wallpaper_controller_test_api =
        std::make_unique<WallpaperControllerTestApi>(wallpaper_controller());
    wallpaper_controller_test_api->SetDefaultWallpaper(
        GetAccountId(browser()->profile()));

    test_chrome_webui_controller_factory_.AddFactoryOverride(
        kChromeUIPersonalizationAppHost, &test_webui_provider_);

    WaitForTestSystemAppInstall();
  }

  void TearDownOnMainThread() override {
    SystemWebAppBrowserTestBase::TearDownOnMainThread();
  }

  content::WebContents* LaunchAppAtWallpaperSubpage(Browser** browser) {
    apps::AppLaunchParams launch_params =
        LaunchParamsForApp(ash::SystemWebAppType::PERSONALIZATION);
    launch_params.override_url =
        GURL(std::string(kChromeUIPersonalizationAppURL) +
             kWallpaperSubpageRelativeUrl);
    return LaunchApp(std::move(launch_params), browser);
  }

  WallpaperControllerImpl* wallpaper_controller() {
    return Shell::Get()->wallpaper_controller();
  }

 private:
  TestChromeWebUIControllerFactory test_chrome_webui_controller_factory_;
  TestPersonalizationAppWebUIProvider test_webui_provider_;
  content::ScopedWebUIControllerFactoryRegistration
      scoped_controller_factory_registration_{
          &test_chrome_webui_controller_factory_};
};

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       BadWallpaperLayoutIgnoredDuringSyncIn) {
  WallpaperInfo info =
      WallpaperInfo(std::string(), static_cast<WallpaperLayout>(1000),
                    WallpaperType::kOnline, base::Time::Now());
  info.collection_id = kDummyCollectionId;
  info.location = kDummyUrl;

  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, info, browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  base::RunLoop().RunUntilIdle();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  EXPECT_EQ(new_info.type, WallpaperType::kDefault);

  // Expects constructing synced info to fail.
  EXPECT_FALSE(wallpaper_controller()
                   ->pref_manager_for_testing()
                   ->GetSyncedWallpaperInfo(account_id, &info));
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       BadWallpaperTypeIgnoredDuringSyncIn) {
  WallpaperInfo info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    static_cast<WallpaperType>(10000), base::Time::Now());
  info.collection_id = kDummyCollectionId;
  info.location = kDummyUrl;

  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, info, browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  base::RunLoop().RunUntilIdle();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  EXPECT_EQ(new_info.type, WallpaperType::kDefault);

  // Expects constructing synced info to fail.
  EXPECT_FALSE(wallpaper_controller()
                   ->pref_manager_for_testing()
                   ->GetSyncedWallpaperInfo(account_id, &info));
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       OnlineWallpaperSyncInSuccessfully) {
  WallpaperInfo info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kOnline, base::Time::Now());
  info.collection_id = kDummyCollectionId;
  info.location = kDummyUrl;
  info.asset_id = kAssetId;
  info.unit_id = kAssetId;
  info.variants.emplace_back(kAssetId, GURL(kDummyUrl),
                             backdrop::Image::IMAGE_TYPE_UNKNOWN);

  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, info, browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  loop.Run();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  // Expects set wallpaper info to match synced info.
  EXPECT_TRUE(new_info.MatchesAsset(info));
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       OnlineWallpaperBadLocationShouldNotCrash) {
  WallpaperInfo info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kOnline, base::Time::Now());
  info.collection_id = kDummyCollectionId;
  info.location = "http://_none_matching_location_from_server";
  info.asset_id = kAssetId;
  info.unit_id = kAssetId;

  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, info, browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  base::RunLoop().RunUntilIdle();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  EXPECT_EQ(new_info.type, WallpaperType::kDefault);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       OnlineWallpaperFetchMissingAssetIdSuccessfully) {
  WallpaperInfo info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kOnline, base::Time::Now());
  info.collection_id = kDummyCollectionId;
  info.location = kDummyUrl;

  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, info, browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  loop.Run();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  // Expects asset_id, unit_id, and variants to be set.
  if (!features::IsVersionWallpaperInfoEnabled()) {
    EXPECT_TRUE(new_info.asset_id.has_value());
  }
  EXPECT_TRUE(new_info.unit_id.has_value());
  EXPECT_EQ(new_info.variants.size(), 1u);
  EXPECT_EQ(new_info.collection_id, kDummyCollectionId);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       OnlineWallpaperFetchMissingVariantsSuccessfully) {
  WallpaperInfo info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kOnline, base::Time::Now());
  info.collection_id = kDummyCollectionId;
  info.location = kDummyUrl;
  info.asset_id = kAssetId;
  info.unit_id = kAssetId;

  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, info, browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  loop.Run();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  // Expects asset_id, unit_id, and variants to be set.
  if (!features::IsVersionWallpaperInfoEnabled()) {
    EXPECT_TRUE(new_info.asset_id.has_value());
  }
  EXPECT_TRUE(new_info.unit_id.has_value());
  EXPECT_EQ(new_info.variants.size(), 1u);
  EXPECT_EQ(new_info.collection_id, kDummyCollectionId);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       OnceGooglePhotosWallpaperSyncInSuccessfully) {
  WallpaperInfo synced_info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kOnceGooglePhotos, base::Time::Now());
  synced_info.location = "_some_google_photos_id";

  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, synced_info,
                          browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  loop.Run();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  EXPECT_TRUE(new_info.MatchesAsset(synced_info));
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       DailyGooglePhotosWallpaper) {
  WallpaperInfo synced_info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kDailyGooglePhotos, base::Time::Now());
  synced_info.collection_id = "_some_google_photos_collection_id";

  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, synced_info,
                          browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  loop.Run();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  EXPECT_EQ(new_info.collection_id, synced_info.collection_id);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperInfoBrowserTest,
                       CustomWallpaperSyncInSuccessfully) {
  WallpaperInfo synced_info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kCustomized, base::Time::Now());
  synced_info.user_file_path = "_some_user_file_path";

  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, synced_info,
                          browser()->profile()->GetPrefs(),
                          WallpaperPrefManager::GetSyncPrefName());
  loop.Run();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  EXPECT_EQ(new_info.user_file_path, synced_info.user_file_path);
}

class PersonalizationAppVersionedWallpaperInfoBrowserTest
    : public PersonalizationAppWallpaperInfoBrowserTest {
 public:
  PersonalizationAppVersionedWallpaperInfoBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kVersionedWallpaperInfo);
  }

 protected:
  void TestMigratingLocalWallpaperInfo(const WallpaperInfo& unmigrated_info) {
    const AccountId account_id = GetAccountId(browser()->profile());
    PutWallpaperInfoInPrefs(account_id, unmigrated_info,
                            g_browser_process->local_state(),
                            prefs::kUserWallpaperInfo);
    // Migration is triggered by `OnActiveUserPrefServiceChanged`.
    wallpaper_controller()->OnActiveUserPrefServiceChanged(
        browser()->profile()->GetPrefs());
    base::RunLoop().RunUntilIdle();

    WallpaperInfo new_info =
        *wallpaper_controller()->GetActiveUserWallpaperInfo();

    EXPECT_TRUE(new_info.version.IsValid());
    EXPECT_EQ(new_info.version, GetSupportedVersion(new_info.type));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PersonalizationAppVersionedWallpaperInfoBrowserTest,
    OnlineWallpaperFetchMissingUnitIdSuccessfullyForSyncedPref) {
  WallpaperInfo info =
      WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                    WallpaperType::kOnline, base::Time::Now());
  info.collection_id = kDummyCollectionId;
  info.location = kDummyUrl;

  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, info, browser()->profile()->GetPrefs(),
                          prefs::kSyncableVersionedWallpaperInfo);
  loop.Run();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();

  // Expects asset_id to be empty.
  EXPECT_FALSE(new_info.asset_id.has_value());
  // Expects unit_id, and variants to be set.
  EXPECT_TRUE(new_info.unit_id.has_value());
  EXPECT_EQ(new_info.variants.size(), 1u);
  EXPECT_EQ(new_info.collection_id, kDummyCollectionId);
  EXPECT_TRUE(new_info.version.IsValid());
  EXPECT_EQ(new_info.version, GetSupportedVersion(new_info.type));
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppVersionedWallpaperInfoBrowserTest,
                       LocalPrefIsMigratedSuccessfullyForOnlineWallpaper) {
  WallpaperInfo unmigrated_info;
  unmigrated_info.collection_id = kDummyCollectionId;
  unmigrated_info.location = kDummyUrl;
  unmigrated_info.type = WallpaperType::kOnline;
  unmigrated_info.version = base::Version();

  const AccountId account_id = GetAccountId(browser()->profile());
  PutWallpaperInfoInPrefs(account_id, unmigrated_info,
                          g_browser_process->local_state(),
                          prefs::kUserWallpaperInfo);
  // Migration is triggered by `OnActiveUserPrefServiceChanged`.
  wallpaper_controller()->OnActiveUserPrefServiceChanged(
      browser()->profile()->GetPrefs());
  base::RunLoop().RunUntilIdle();

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();

  // Expects asset_id to be empty.
  EXPECT_FALSE(new_info.asset_id.has_value());
  // Expects unit_id, and variants to be set.
  EXPECT_TRUE(new_info.unit_id.has_value());
  EXPECT_EQ(new_info.variants.size(), 1u);
  EXPECT_EQ(new_info.collection_id, kDummyCollectionId);
  EXPECT_TRUE(new_info.version.IsValid());
  EXPECT_EQ(new_info.version, GetSupportedVersion(new_info.type));
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppVersionedWallpaperInfoBrowserTest,
                       LocalPrefIsMigratedSuccessfullyForDailyWallpaper) {
  WallpaperInfo unmigrated_info;
  unmigrated_info.collection_id = kDummyCollectionId;
  unmigrated_info.location = kDummyUrl;
  unmigrated_info.type = WallpaperType::kDaily;
  unmigrated_info.version = base::Version();
  TestMigratingLocalWallpaperInfo(unmigrated_info);

  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();

  // Expects asset_id to be empty.
  EXPECT_FALSE(new_info.asset_id.has_value());
  // Expects unit_id, and variants to be set.
  EXPECT_TRUE(new_info.unit_id.has_value());
  EXPECT_EQ(new_info.variants.size(), 1u);
  EXPECT_EQ(new_info.collection_id, kDummyCollectionId);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppVersionedWallpaperInfoBrowserTest,
                       LocalPrefIsMigratedSuccessfullyForCustomWallpaper) {
  WallpaperInfo unmigrated_info;
  unmigrated_info.type = WallpaperType::kCustomized;
  unmigrated_info.user_file_path = "_some_user_file_path";
  unmigrated_info.version = base::Version();
  TestMigratingLocalWallpaperInfo(unmigrated_info);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppVersionedWallpaperInfoBrowserTest,
                       LocalPrefIsMigratedSuccessfullyForDefaultWallpaper) {
  WallpaperInfo unmigrated_info;
  unmigrated_info.type = WallpaperType::kDefault;
  unmigrated_info.version = base::Version();
  TestMigratingLocalWallpaperInfo(unmigrated_info);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppVersionedWallpaperInfoBrowserTest,
                       LocalPrefIsMigratedSuccessfullyForPolicyWallpaper) {
  WallpaperInfo unmigrated_info;
  unmigrated_info.type = WallpaperType::kPolicy;
  unmigrated_info.location =
      "547ff840b5bc4aa50d7b57823c47d06cb79c7666/policy-controlled.jpeg";
  unmigrated_info.version = base::Version();
  TestMigratingLocalWallpaperInfo(unmigrated_info);
}

IN_PROC_BROWSER_TEST_F(
    PersonalizationAppVersionedWallpaperInfoBrowserTest,
    LocalPrefIsMigratedSuccessfullyForDailyGooglePhotosWallpaper) {
  WallpaperInfo unmigrated_info;
  unmigrated_info.type = WallpaperType::kDailyGooglePhotos;
  unmigrated_info.collection_id = "_some_google_photos_collection_id";
  unmigrated_info.version = base::Version();
  TestMigratingLocalWallpaperInfo(unmigrated_info);
}

IN_PROC_BROWSER_TEST_F(
    PersonalizationAppVersionedWallpaperInfoBrowserTest,
    LocalPrefIsMigratedSuccessfullyForOnceGooglePhotosWallpaper) {
  WallpaperInfo unmigrated_info;
  unmigrated_info.type = WallpaperType::kOnceGooglePhotos;
  unmigrated_info.location = "_some_google_photos_id";
  unmigrated_info.version = base::Version();
  TestMigratingLocalWallpaperInfo(unmigrated_info);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppVersionedWallpaperInfoBrowserTest,
                       LocalPrefIsMigratedSuccessfullyForSeaPenWallpaper) {
  WallpaperInfo unmigrated_info;
  unmigrated_info.type = WallpaperType::kSeaPen;
  unmigrated_info.location = "1864724739";
  unmigrated_info.version = base::Version();
  TestMigratingLocalWallpaperInfo(unmigrated_info);
}

}  // namespace
}  // namespace ash::personalization_app
