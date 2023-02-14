// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/personalization_app_mojom_banned_browsertest_fixture.h"

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_ambient_provider.h"
#include "ash/webui/personalization_app/personalization_app_keyboard_backlight_provider.h"
#include "ash/webui/personalization_app/personalization_app_theme_provider.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/personalization_app_user_provider.h"
#include "ash/webui/personalization_app/personalization_app_wallpaper_provider.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::personalization_app {

namespace {

class MockPersonalizationAppAmbientProvider
    : public PersonalizationAppAmbientProvider {
 public:
  MOCK_METHOD(void,
              BindInterface,
              (mojo::PendingReceiver<mojom::AmbientProvider> receiver),
              (override));
  MOCK_METHOD(void,
              IsAmbientModeEnabled,
              (IsAmbientModeEnabledCallback callback),
              (override));
  MOCK_METHOD(void,
              SetAmbientObserver,
              (mojo::PendingRemote<
                  ash::personalization_app::mojom::AmbientObserver> observer),
              (override));
  MOCK_METHOD(void, SetAmbientModeEnabled, (bool enabled), (override));
  MOCK_METHOD(void,
              SetAnimationTheme,
              (ash::AmbientTheme animation_theme),
              (override));
  MOCK_METHOD(void,
              SetTopicSource,
              (ash::AmbientModeTopicSource topic_source),
              (override));
  MOCK_METHOD(void,
              SetTemperatureUnit,
              (ash::AmbientModeTemperatureUnit temperature_unit),
              (override));
  MOCK_METHOD(void,
              SetAlbumSelected,
              (const std::string& id,
               ash::AmbientModeTopicSource topic_source,
               bool selected),
              (override));
  MOCK_METHOD(void, SetPageViewed, (), (override));
  MOCK_METHOD(void, StartScreenSaverPreview, (), (override));
  MOCK_METHOD(void, FetchSettingsAndAlbums, (), (override));
};

class MockPersonalizationAppKeyboardBacklightProvider
    : public PersonalizationAppKeyboardBacklightProvider {
 public:
  MOCK_METHOD(
      void,
      BindInterface,
      (mojo::PendingReceiver<mojom::KeyboardBacklightProvider> receiver),
      (override));
  MOCK_METHOD(void,
              SetKeyboardBacklightObserver,
              (mojo::PendingRemote<mojom::KeyboardBacklightObserver> observer),
              (override));
  MOCK_METHOD(void,
              SetBacklightColor,
              (mojom::BacklightColor backlight_color),
              (override));
  MOCK_METHOD(void,
              ShouldShowNudge,
              (ShouldShowNudgeCallback callback),
              (override));
  MOCK_METHOD(void, HandleNudgeShown, (), (override));
};

class MockPersonalizationAppThemeProvider
    : public PersonalizationAppThemeProvider {
 public:
  MOCK_METHOD(void,
              BindInterface,
              (mojo::PendingReceiver<
                  ash::personalization_app::mojom::ThemeProvider> receiver),
              (override));
  MOCK_METHOD(void,
              SetThemeObserver,
              (mojo::PendingRemote<
                  ash::personalization_app::mojom::ThemeObserver> observer),
              (override));
  MOCK_METHOD(void, SetColorModePref, (bool dark_mode_enabled), (override));
  MOCK_METHOD(void,
              SetColorScheme,
              (ash::ColorScheme color_scheme),
              (override));
  MOCK_METHOD(void, SetStaticColor, (::SkColor static_color), (override));
  MOCK_METHOD(void,
              SetColorModeAutoScheduleEnabled,
              (bool enabled),
              (override));
  MOCK_METHOD(void,
              GenerateSampleColorSchemes,
              (GenerateSampleColorSchemesCallback callback),
              (override));
  MOCK_METHOD(void,
              GetColorScheme,
              (GetColorSchemeCallback callback),
              (override));
  MOCK_METHOD(void,
              GetStaticColor,
              (GetStaticColorCallback callback),
              (override));
  MOCK_METHOD(void,
              IsDarkModeEnabled,
              (IsDarkModeEnabledCallback callback),
              (override));
  MOCK_METHOD(void,
              IsColorModeAutoScheduleEnabled,
              (IsColorModeAutoScheduleEnabledCallback callback),
              (override));
};

class MockPersonalizationAppWallpaperProvider
    : public PersonalizationAppWallpaperProvider {
 public:
  MOCK_METHOD(void,
              BindInterface,
              (mojo::PendingReceiver<
                  ash::personalization_app::mojom::WallpaperProvider> receiver),
              (override));
  bool IsEligibleForGooglePhotos() override { return true; }
  void GetWallpaperAsJpegBytes(
      content::WebUIDataSource::GotDataCallback callback) override {
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedBytes>());
  }
  MOCK_METHOD(void, MakeTransparent, (), (override));
  MOCK_METHOD(void, MakeOpaque, (), (override));
  MOCK_METHOD(void,
              FetchCollections,
              (FetchCollectionsCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchImagesForCollection,
              (const std::string& collection_id,
               FetchImagesForCollectionCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchGooglePhotosAlbums,
              (const absl::optional<std::string>& resume_token,
               FetchGooglePhotosAlbumsCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchGooglePhotosSharedAlbums,
              (const absl::optional<std::string>& resume_token,
               FetchGooglePhotosAlbumsCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchGooglePhotosEnabled,
              (FetchGooglePhotosEnabledCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchGooglePhotosPhotos,
              (const absl::optional<std::string>& item_id,
               const absl::optional<std::string>& album_id,
               const absl::optional<std::string>& resume_token,
               FetchGooglePhotosPhotosCallback callback),
              (override));
  MOCK_METHOD(void,
              GetDefaultImageThumbnail,
              (GetDefaultImageThumbnailCallback callback),
              (override));
  MOCK_METHOD(void,
              GetLocalImages,
              (GetLocalImagesCallback callback),
              (override));
  MOCK_METHOD(void,
              GetLocalImageThumbnail,
              (const base::FilePath& path,
               GetLocalImageThumbnailCallback callback),
              (override));
  MOCK_METHOD(void,
              SetWallpaperObserver,
              (mojo::PendingRemote<
                  ash::personalization_app::mojom::WallpaperObserver> observer),
              (override));
  MOCK_METHOD(void,
              SelectWallpaper,
              (uint64_t image_asset_id,
               bool preview_mode,
               SelectWallpaperCallback callback),
              (override));
  MOCK_METHOD(void,
              SelectDefaultImage,
              (SelectDefaultImageCallback callback),
              (override));
  MOCK_METHOD(void,
              SelectGooglePhotosPhoto,
              (const std::string& id,
               ash::WallpaperLayout layout,
               bool preview_mode,
               SelectGooglePhotosPhotoCallback callback),
              (override));
  MOCK_METHOD(void,
              SelectGooglePhotosAlbum,
              (const std::string& id, SelectGooglePhotosAlbumCallback callback),
              (override));
  MOCK_METHOD(void,
              GetGooglePhotosDailyRefreshAlbumId,
              (GetGooglePhotosDailyRefreshAlbumIdCallback callback),
              (override));
  MOCK_METHOD(void,
              SelectLocalImage,
              (const base::FilePath& path,
               ash::WallpaperLayout layout,
               bool preview_mode,
               SelectLocalImageCallback callback),
              (override));
  MOCK_METHOD(void,
              SetCurrentWallpaperLayout,
              (ash::WallpaperLayout layout),
              (override));
  MOCK_METHOD(void,
              SetDailyRefreshCollectionId,
              (const std::string& collection_id,
               SetDailyRefreshCollectionIdCallback callback),
              (override));
  MOCK_METHOD(void,
              GetDailyRefreshCollectionId,
              (GetDailyRefreshCollectionIdCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateDailyRefreshWallpaper,
              (UpdateDailyRefreshWallpaperCallback callback),
              (override));
  MOCK_METHOD(void,
              IsInTabletMode,
              (IsInTabletModeCallback callback),
              (override));
  MOCK_METHOD(void, ConfirmPreviewWallpaper, (), (override));
  MOCK_METHOD(void, CancelPreviewWallpaper, (), (override));
};

class MockPersonalizationAppUserProvider
    : public PersonalizationAppUserProvider {
 public:
  MOCK_METHOD(void,
              BindInterface,
              (mojo::PendingReceiver<
                  ash::personalization_app::mojom::UserProvider> receiver),
              (override));
  MOCK_METHOD(void,
              SetUserImageObserver,
              (mojo::PendingRemote<
                  ash::personalization_app::mojom::UserImageObserver> observer),
              (override));
  MOCK_METHOD(void, GetUserInfo, (GetUserInfoCallback callback), (override));
  MOCK_METHOD(void,
              GetDefaultUserImages,
              (GetDefaultUserImagesCallback callback),
              (override));
  MOCK_METHOD(void, SelectImageFromDisk, (), (override));
  MOCK_METHOD(void, SelectDefaultImage, (int index), (override));
  MOCK_METHOD(void, SelectProfileImage, (), (override));
  MOCK_METHOD(void,
              SelectCameraImage,
              (::mojo_base::BigBuffer data),
              (override));
  MOCK_METHOD(void, SelectLastExternalUserImage, (), (override));
};

}  // namespace

std::unique_ptr<content::WebUIController>
TestPersonalizationAppMojomBannedWebUIProvider::NewWebUI(content::WebUI* web_ui,
                                                         const GURL& url) {
  auto ambient_provider = std::make_unique<
      testing::StrictMock<MockPersonalizationAppAmbientProvider>>();
  auto keyboard_backlight_provider = std::make_unique<
      testing::StrictMock<MockPersonalizationAppKeyboardBacklightProvider>>();
  auto theme_provider = std::make_unique<
      testing::StrictMock<MockPersonalizationAppThemeProvider>>();
  auto wallpaper_provider = std::make_unique<
      testing::StrictMock<MockPersonalizationAppWallpaperProvider>>();
  auto user_provider = std::make_unique<
      testing::StrictMock<MockPersonalizationAppUserProvider>>();
  return std::make_unique<PersonalizationAppUI>(
      web_ui, std::move(ambient_provider),
      std::move(keyboard_backlight_provider), std::move(theme_provider),
      std::move(user_provider), std::move(wallpaper_provider));
}

void PersonalizationAppMojomBannedBrowserTestFixture::SetUpOnMainThread() {
  MojoWebUIBrowserTest::SetUpOnMainThread();
  test_factory_.AddFactoryOverride(kChromeUIPersonalizationAppHost,
                                   &test_web_ui_provider_);
}

}  // namespace ash::personalization_app
