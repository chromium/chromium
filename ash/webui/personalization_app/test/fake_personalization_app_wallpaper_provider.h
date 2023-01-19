// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_WALLPAPER_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_WALLPAPER_PROVIDER_H_

#include "ash/webui/personalization_app/personalization_app_wallpaper_provider.h"

#include <stdint.h>
#include <string>

#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

class FakePersonalizationAppWallpaperProvider
    : public PersonalizationAppWallpaperProvider {
 public:
  explicit FakePersonalizationAppWallpaperProvider(content::WebUI* web_ui);

  FakePersonalizationAppWallpaperProvider(
      const FakePersonalizationAppWallpaperProvider&) = delete;
  FakePersonalizationAppWallpaperProvider& operator=(
      const FakePersonalizationAppWallpaperProvider&) = delete;

  ~FakePersonalizationAppWallpaperProvider() override;

  // PersonalizationAppWallpaperProvider:
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::WallpaperProvider>
          receiver) override;

  void GetWallpaperAsJpegBytes(
      content::WebUIDataSource::GotDataCallback callback) override;

  bool IsEligibleForGooglePhotos() override;

  // ash::personalization_app::mojom::WallpaperProvider:
  void MakeTransparent() override {}

  void MakeOpaque() override {}

  void FetchCollections(FetchCollectionsCallback callback) override;

  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;

  void FetchGooglePhotosAlbums(
      const absl::optional<std::string>& resume_token,
      FetchGooglePhotosAlbumsCallback callback) override;

  void FetchGooglePhotosSharedAlbums(
      const absl::optional<std::string>& resume_token,
      FetchGooglePhotosAlbumsCallback callback) override;

  void FetchGooglePhotosEnabled(
      FetchGooglePhotosEnabledCallback callback) override;

  void FetchGooglePhotosPhotos(
      const absl::optional<std::string>& item_id,
      const absl::optional<std::string>& album_id,
      const absl::optional<std::string>& resume_token,
      FetchGooglePhotosPhotosCallback callback) override;

  void GetDefaultImageThumbnail(
      GetDefaultImageThumbnailCallback callback) override;

  void GetLocalImages(GetLocalImagesCallback callback) override;

  void GetLocalImageThumbnail(const base::FilePath& path,
                              GetLocalImageThumbnailCallback callback) override;

  void SetWallpaperObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
          observer) override;

  void SelectWallpaper(uint64_t image_asset_id,
                       bool preview_mode,
                       SelectWallpaperCallback callback) override;

  void SelectDefaultImage(SelectDefaultImageCallback callback) override;

  void SelectGooglePhotosPhoto(
      const std::string& id,
      ash::WallpaperLayout layout,
      bool preview_mode,
      SelectGooglePhotosPhotoCallback callback) override;

  void SelectGooglePhotosAlbum(
      const std::string& id,
      SelectGooglePhotosAlbumCallback callback) override;

  void GetGooglePhotosDailyRefreshAlbumId(
      GetGooglePhotosDailyRefreshAlbumIdCallback callback) override;

  void SelectLocalImage(const base::FilePath& path,
                        ash::WallpaperLayout layout,
                        bool preview_mode,
                        SelectLocalImageCallback callback) override;

  void SetCurrentWallpaperLayout(ash::WallpaperLayout layout) override;

  void SetDailyRefreshCollectionId(
      const std::string& collection_id,
      SetDailyRefreshCollectionIdCallback callback) override;

  void GetDailyRefreshCollectionId(
      GetDailyRefreshCollectionIdCallback callback) override;

  void UpdateDailyRefreshWallpaper(
      UpdateDailyRefreshWallpaperCallback callback) override;

  void IsInTabletMode(IsInTabletModeCallback callback) override;

  void ConfirmPreviewWallpaper() override;

  void CancelPreviewWallpaper() override;

 private:
  void SendOnWallpaperChanged(const WallpaperInfo& wallpaper_info);

  mojo::Remote<ash::personalization_app::mojom::WallpaperObserver>
      wallpaper_observer_remote_;
  mojo::Receiver<ash::personalization_app::mojom::WallpaperProvider>
      wallpaper_receiver_{this};
  base::WeakPtrFactory<FakePersonalizationAppWallpaperProvider>
      weak_ptr_factory_{this};
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_WALLPAPER_PROVIDER_H_
