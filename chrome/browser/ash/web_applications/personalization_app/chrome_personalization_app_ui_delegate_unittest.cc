// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/chrome_personalization_app_ui_delegate.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome_device_policy.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kTestGaiaId[] = "1234567890";

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  ash::device_settings_cache::RegisterPrefs(local_state->registry());
  user_manager::KnownUser::RegisterPrefs(local_state->registry());
  ash::WallpaperControllerImpl::RegisterLocalStatePrefs(
      local_state->registry());
  policy::DeviceWallpaperImageExternalDataHandler::RegisterPrefs(
      local_state->registry());
  ProfileAttributesStorage::RegisterPrefs(local_state->registry());
  return local_state;
}

void AddAndLoginUser(const AccountId& account_id) {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());

  user_manager->AddUser(account_id);
  user_manager->LoginUser(account_id);
  user_manager->SwitchActiveUser(account_id);
}

// Create a test 1x1 image with a given |color|.
gfx::ImageSkia CreateSolidImageSkia(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

class TestWallpaperObserver
    : public ash::personalization_app::mojom::WallpaperObserver {
 public:
  void OnWallpaperChanged(
      ash::personalization_app::mojom::CurrentWallpaperPtr image) override {
    current_wallpaper_ = std::move(image);
  }

  mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
  pending_remote() {
    DCHECK(!wallpaper_observer_receiver_.is_bound());
    return wallpaper_observer_receiver_.BindNewPipeAndPassRemote();
  }

  ash::personalization_app::mojom::CurrentWallpaper* current_wallpaper() {
    if (!wallpaper_observer_receiver_.is_bound())
      return nullptr;

    wallpaper_observer_receiver_.FlushForTesting();
    return current_wallpaper_.get();
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::WallpaperObserver>
      wallpaper_observer_receiver_{this};

  ash::personalization_app::mojom::CurrentWallpaperPtr current_wallpaper_ =
      nullptr;
};

}  // namespace

class ChromePersonalizationAppUiDelegateTest : public testing::Test {
 public:
  ChromePersonalizationAppUiDelegateTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ChromePersonalizationAppUiDelegateTest(
      const ChromePersonalizationAppUiDelegateTest&) = delete;
  ChromePersonalizationAppUiDelegateTest& operator=(
      const ChromePersonalizationAppUiDelegateTest&) = delete;
  ~ChromePersonalizationAppUiDelegateTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kWallpaperWebUI);
    wallpaper_controller_client_ =
        std::make_unique<WallpaperControllerClientImpl>();
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    AddAndLoginUser(
        AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId));

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    delegate_ = std::make_unique<ChromePersonalizationAppUiDelegate>(&web_ui_);

    delegate_->BindInterface(
        wallpaper_provider_remote_.BindNewPipeAndPassReceiver());
  }

  void AddWallpaperImage(
      uint64_t asset_id,
      const ChromePersonalizationAppUiDelegate::ImageInfo& image_info) {
    delegate_->image_asset_id_map_[asset_id] = image_info;
  }

  TestWallpaperController* test_wallpaper_controller() {
    return &test_wallpaper_controller_;
  }

  mojo::Remote<ash::personalization_app::mojom::WallpaperProvider>*
  wallpaper_provider_remote() {
    return &wallpaper_provider_remote_;
  }

  ChromePersonalizationAppUiDelegate* delegate() { return delegate_.get(); }

  void SetWallpaperObserver() {
    wallpaper_provider_remote_->SetWallpaperObserver(
        test_wallpaper_observer_.pending_remote());
  }

  ash::personalization_app::mojom::CurrentWallpaper* current_wallpaper() {
    wallpaper_provider_remote_.FlushForTesting();
    return test_wallpaper_observer_.current_wallpaper();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  // Required for |ScopedTestCrosSettings|.
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  // Required for |ScopedTestCrosSettings|.
  ash::ScopedTestDeviceSettingsService scoped_device_settings_;
  // Required for |WallpaperControllerClientImpl|.
  ash::ScopedTestCrosSettings scoped_testing_cros_settings_{
      RegisterPrefs(&pref_service_)};
  user_manager::ScopedUserManager scoped_user_manager_;
  TestWallpaperController test_wallpaper_controller_;
  // |wallpaper_controller_client_| must be destructed before
  // |test_wallpaper_controller_|.
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestingProfile* profile_;
  mojo::Remote<ash::personalization_app::mojom::WallpaperProvider>
      wallpaper_provider_remote_;
  TestWallpaperObserver test_wallpaper_observer_;
  std::unique_ptr<ChromePersonalizationAppUiDelegate> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromePersonalizationAppUiDelegateTest, SelectWallpaper) {
  test_wallpaper_controller()->ClearCounts();

  const uint64_t asset_id = 1;

  AddWallpaperImage(asset_id, /*image_info=*/{
                        GURL("test_url"),
                        "collection_id",
                    });

  base::RunLoop loop;
  wallpaper_provider_remote()->get()->SelectWallpaper(
      asset_id, /*preview_mode=*/false,
      base::BindLambdaForTesting([quit = loop.QuitClosure()](bool success) {
        EXPECT_TRUE(success);
        std::move(quit).Run();
      }));
  wallpaper_provider_remote()->FlushForTesting();
  loop.Run();

  EXPECT_EQ(1, test_wallpaper_controller()->set_online_wallpaper_count());
  EXPECT_EQ(
      ash::WallpaperInfo(
          {AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
           absl::make_optional(asset_id), GURL("test_url"), "collection_id",
           ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
           /*preview_mode=*/false, /*from_user=*/true,
           /*daily_refresh_enabled=*/false}),
      test_wallpaper_controller()->wallpaper_info().value());
}

TEST_F(ChromePersonalizationAppUiDelegateTest, PreviewWallpaper) {
  test_wallpaper_controller()->ClearCounts();

  const uint64_t asset_id = 1;

  AddWallpaperImage(asset_id, /*image_info=*/{
                        GURL("test_url"),
                        "collection_id",
                    });

  base::RunLoop loop;
  wallpaper_provider_remote()->get()->SelectWallpaper(
      asset_id, /*preview_mode=*/true,
      base::BindLambdaForTesting([quit = loop.QuitClosure()](bool success) {
        EXPECT_TRUE(success);
        std::move(quit).Run();
      }));
  wallpaper_provider_remote()->FlushForTesting();
  loop.Run();

  EXPECT_EQ(1, test_wallpaper_controller()->set_online_wallpaper_count());
  EXPECT_EQ(
      ash::WallpaperInfo(
          {AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
           absl::make_optional(asset_id), GURL("test_url"), "collection_id",
           ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
           /*preview_mode=*/true, /*from_user=*/true,
           /*daily_refresh_enabled=*/false}),
      test_wallpaper_controller()->wallpaper_info().value());
}

TEST_F(ChromePersonalizationAppUiDelegateTest, ObserveWallpaperFiresWhenBound) {
  // This will create the data url referenced below in expectation.
  test_wallpaper_controller()->ShowWallpaperImage(
      CreateSolidImageSkia(/*width=*/1, /*height=*/1, SK_ColorBLACK));

  const uint64_t asset_id = 1;

  test_wallpaper_controller()->SetOnlineWallpaper(
      {AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
       absl::make_optional(asset_id), GURL("test_url"), "collection_id",
       ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
       /*preview_mode=*/false, /*from_user=*/true,
       /*daily_refresh_enabled=*/false},
      base::DoNothing());

  EXPECT_EQ(nullptr, current_wallpaper());

  SetWallpaperObserver();

  // WallpaperObserver Should have received an image through mojom.
  ash::personalization_app::mojom::CurrentWallpaper* current =
      current_wallpaper();

  EXPECT_EQ(ash::WallpaperType::kOnline, current->type);
  EXPECT_EQ(ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
            current->layout);
  // Data url of a solid black image scaled up to 256x256.
  EXPECT_EQ(webui::GetBitmapDataUrl(
                *CreateSolidImageSkia(256, 256, SK_ColorBLACK).bitmap()),
            current->url);
}
