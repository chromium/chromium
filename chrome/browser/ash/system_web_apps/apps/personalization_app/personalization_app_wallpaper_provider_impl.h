// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_WALLPAPER_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_WALLPAPER_PROVIDER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_wallpaper_provider.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {
class ThumbnailLoader;
}  // namespace ash

namespace backdrop {
class Collection;
class Image;
}  // namespace backdrop

namespace content {
class WebUI;
}  // namespace content

namespace wallpaper_handlers {
class BackdropCollectionInfoFetcher;
class WallpaperFetcherDelegate;
class BackdropImageInfoFetcher;
class GooglePhotosAlbumsFetcher;
class GooglePhotosSharedAlbumsFetcher;
class GooglePhotosEnabledFetcher;
class GooglePhotosPhotosFetcher;
}  // namespace wallpaper_handlers

class Profile;

namespace ash::personalization_app {

// Implemented in //chrome because this relies on chrome |wallpaper_handlers|
// code.
class PersonalizationAppWallpaperProviderImpl
    : public PersonalizationAppWallpaperProvider,
      ash::WallpaperControllerObserver {
 public:
  PersonalizationAppWallpaperProviderImpl(
      content::WebUI* web_ui,
      std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
          wallpaper_fetcher_delegate);

  PersonalizationAppWallpaperProviderImpl(
      const PersonalizationAppWallpaperProviderImpl&) = delete;
  PersonalizationAppWallpaperProviderImpl& operator=(
      const PersonalizationAppWallpaperProviderImpl&) = delete;

  ~PersonalizationAppWallpaperProviderImpl() override;

  // PersonalizationAppWallpaperProvider:
  // |BindInterface| may be called multiple times, for example if the user
  // presses Ctrl+Shift+R while on the personalization app.
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::WallpaperProvider>
          receiver) override;

  void GetWallpaperAsJpegBytes(
      content::WebUIDataSource::GotDataCallback callback) override;

  // Not all users can see google photos. Requires a gaia account to be able to
  // fetch photos.
  bool IsEligibleForGooglePhotos() override;

  // ash::personalization_app::mojom::WallpaperProvider:

  // Configure the window to be transparent so that the user can trigger a "full
  // screen preview" mode. This allows the user to see through the app window to
  // see the chosen wallpaper. This is safe to call multiple times in a row.
  void MakeTransparent() override;

  // Configure the window to be opaque so that after exiting the "full screen
  // preview" mode, the transparent background will be reversed. This is safe
  // to call multiple times in a row.
  void MakeOpaque() override;

  void FetchCollections(FetchCollectionsCallback callback) override;

  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;

  void FetchGooglePhotosAlbums(
      const std::optional<std::string>& resume_token,
      FetchGooglePhotosAlbumsCallback callback) override;

  void FetchGooglePhotosSharedAlbums(
      const std::optional<std::string>& resume_token,
      FetchGooglePhotosAlbumsCallback callback) override;

  void FetchGooglePhotosEnabled(
      FetchGooglePhotosEnabledCallback callback) override;

  void FetchGooglePhotosPhotos(
      const std::optional<std::string>& item_id,
      const std::optional<std::string>& album_id,
      const std::optional<std::string>& resume_token,
      FetchGooglePhotosPhotosCallback callback) override;

  void GetDefaultImageThumbnail(
      GetDefaultImageThumbnailCallback callback) override;

  void GetLocalImages(GetLocalImagesCallback callback) override;

  void GetLocalImageThumbnail(const base::FilePath& file_path,
                              GetLocalImageThumbnailCallback callback) override;

  void SetWallpaperObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
          observer) override;

  // ash::WallpaperControllerObserver:
  void OnWallpaperResized() override;

  // ash::WallpaperControllerObserver:
  void OnWallpaperPreviewEnded() override;

  // ash::personalization_app::mojom::WallpaperProvider:
  void SelectWallpaper(uint64_t unit_id,
                       bool preview_mode,
                       SelectWallpaperCallback callback) override;

  void SelectDefaultImage(SelectDefaultImageCallback callback) override;

  void SelectLocalImage(const base::FilePath& path,
                        ash::WallpaperLayout layout,
                        bool preview_mode,
                        SelectLocalImageCallback callback) override;

  void SelectGooglePhotosPhoto(
      const std::string& id,
      ash::WallpaperLayout layout,
      bool preview_mode,
      SelectGooglePhotosPhotoCallback callback) override;

  void SelectGooglePhotosAlbum(
      const std::string& album_id,
      SelectGooglePhotosAlbumCallback callback) override;

  void GetGooglePhotosDailyRefreshAlbumId(
      GetGooglePhotosDailyRefreshAlbumIdCallback callback) override;

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

  void ShouldShowTimeOfDayWallpaperDialog(
      ShouldShowTimeOfDayWallpaperDialogCallback callback) override;

  wallpaper_handlers::GooglePhotosAlbumsFetcher*
  GetOrCreateGooglePhotosAlbumsFetcher();

  wallpaper_handlers::GooglePhotosSharedAlbumsFetcher*
  GetOrCreateGooglePhotosSharedAlbumsFetcher();

  wallpaper_handlers::GooglePhotosEnabledFetcher*
  GetOrCreateGooglePhotosEnabledFetcher();

  wallpaper_handlers::GooglePhotosPhotosFetcher*
  GetOrCreateGooglePhotosPhotosFetcher();

 private:
  friend class PersonalizationAppWallpaperProviderImplTest;
  friend class PersonalizationAppWallpaperProviderImplGooglePhotosTest;

  void OnFetchCollections(bool success,
                          const std::vector<backdrop::Collection>& collections);

  void OnFetchCollectionImages(
      FetchImagesForCollectionCallback callback,
      std::unique_ptr<wallpaper_handlers::BackdropImageInfoFetcher> fetcher,
      bool success,
      const std::string& collection_id,
      const std::vector<backdrop::Image>& images);

  void OnFetchGooglePhotosEnabled(
      FetchGooglePhotosEnabledCallback callback,
      ash::personalization_app::mojom::GooglePhotosEnablementState state);

  void OnFetchGooglePhotosPhotos(
      std::optional<std::string> album_id,
      FetchGooglePhotosPhotosCallback callback,
      mojo::StructPtr<mojom::FetchGooglePhotosPhotosResponse> response);

  void OnGetDefaultImage(GetDefaultImageThumbnailCallback callback,
                         const gfx::ImageSkia& image);

  void OnGetLocalImages(GetLocalImagesCallback callback,
                        const std::vector<base::FilePath>& images);

  void OnGetLocalImageThumbnail(GetLocalImageThumbnailCallback callback,
                                const SkBitmap* bitmap,
                                base::File::Error error);

  // Called after attempting to select an online wallpaper. Will be dropped if
  // new requests come in.
  void OnOnlineWallpaperSelected(bool success);

  // Called after attempting to select a Google Photos wallpaper. Will be
  // dropped if new requests come in.
  void OnGooglePhotosWallpaperSelected(bool success);

  // Called after attempting to select a local image. Will be dropped if new
  // requests come in.
  void OnLocalImageSelected(bool success);

  // Called after attempting to update a daily refresh wallpaper. Will be
  // dropped if new requests come in.
  void OnDailyRefreshWallpaperUpdated(bool success);

  // Called after forching the current wallpaper image to refresh as part of
  // toggling on daily refresh mode.
  void OnDailyRefreshWallpaperForced(bool success);

  void FindAttribution(
      const ash::WallpaperInfo& info,
      const std::optional<std::vector<backdrop::Collection>>& collections);

  void FindImageMetadataInCollection(
      const ash::WallpaperInfo& info,
      std::size_t current_index,
      const std::optional<std::vector<backdrop::Collection>>& collections,
      bool success,
      const std::string& collection_id,
      const std::vector<backdrop::Image>& images);

  void FindSeaPenWallpaperAttribution(uint32_t id);

  void SendSeaPenWallpaperAttribution(
      uint32_t id,
      const gfx::ImageSkia& image,
      mojom::RecentSeaPenImageInfoPtr sea_pen_metadata);

  void SendGooglePhotosAttribution(
      const ash::WallpaperInfo& info,
      mojo::StructPtr<ash::personalization_app::mojom::GooglePhotosPhoto> photo,
      bool success);

  // Called when the user sets an image, or cancels/confirms preview wallpaper.
  // If a new image is set in preview mode, will minimize all windows except the
  // wallpaper SWA. When canceling or confirming preview mode, will restore the
  // minimized windows to their previous state.
  void SetMinimizedWindowStateForPreview(bool preview_mode);

  void NotifyAttributionChanged(
      ash::personalization_app::mojom::CurrentAttributionPtr attribution);

  void NotifyWallpaperChanged(
      ash::personalization_app::mojom::CurrentWallpaperPtr current_wallpaper);

  std::unique_ptr<wallpaper_handlers::BackdropCollectionInfoFetcher>
      wallpaper_collection_info_fetcher_;
  std::vector<FetchCollectionsCallback> pending_collections_callbacks_;

  std::unique_ptr<wallpaper_handlers::BackdropImageInfoFetcher>
      wallpaper_attribution_info_fetcher_;

  // Fetches the Google Photos albums the user has created. Constructed lazily
  // at the time of the first request and then persists for the rest of the
  // delegate's lifetime, unless preemptively or subsequently replaced by a mock
  // in a test.
  std::unique_ptr<wallpaper_handlers::GooglePhotosAlbumsFetcher>
      google_photos_albums_fetcher_;

  // Fetches the Google Photos albums shared with the user. Constructed lazily
  // at the time of the first request and then persists for the rest of the
  // delegate's lifetime, unless preemptively or subsequently replaced by a mock
  // in a test.
  std::unique_ptr<wallpaper_handlers::GooglePhotosSharedAlbumsFetcher>
      google_photos_shared_albums_fetcher_;

  // Fetches the state of the user's permission to access Google Photos data.
  // Constructed lazily at the time of the first request and then persists for
  // the rest of the delegate's lifetime, unless preemptively or subsequently
  // replaced by a mock in a test.
  std::unique_ptr<wallpaper_handlers::GooglePhotosEnabledFetcher>
      google_photos_enabled_fetcher_;

  // Fetches visible photos from the user's Google Photos library. Constructed
  // lazily at the time of the first request and then persists for the rest of
  // the delegate's lifetime, unless preemptively or subsequently replaced by a
  // mock in a test.
  std::unique_ptr<wallpaper_handlers::GooglePhotosPhotosFetcher>
      google_photos_photos_fetcher_;

  // Set to true when an enabled Google Photos enterprise setting is fetched
  // from the server. Attempting to select a Google Photos wallpaper or fetch
  // Google Photos data other than the enterprise setting itself will fail if
  // this value is false.
  bool is_google_photos_enterprise_enabled_ = false;

  SelectWallpaperCallback pending_select_wallpaper_callback_;

  SelectLocalImageCallback pending_select_local_image_callback_;

  SelectGooglePhotosPhotoCallback pending_select_google_photos_photo_callback_;

  base::OnceCallback<void(bool success)> pending_set_daily_refresh_callback_;

  UpdateDailyRefreshWallpaperCallback
      pending_update_daily_refresh_wallpaper_callback_;

  std::unique_ptr<ash::ThumbnailLoader> thumbnail_loader_;

  struct ImageInfo {
    GURL image_url;
    std::string collection_id;
    uint64_t asset_id;
    uint64_t unit_id;
    backdrop::Image::ImageType type;

    ImageInfo(const GURL& in_image_url,
              const std::string& in_collection_id,
              uint64_t in_asset_id,
              uint64_t in_unit_id,
              backdrop::Image::ImageType in_type)
        : image_url(in_image_url),
          collection_id(in_collection_id),
          asset_id(in_asset_id),
          unit_id(in_unit_id),
          type(in_type) {}
  };

  // Stores the mapping of valid image unit_ids and their image variants. This
  // is filled when a user first visits the Wallpaper subpage of Personalization
  // App.
  std::map<uint64_t, std::vector<ImageInfo>> image_unit_id_map_;

  // Stores the mapping of Google Photos album id to its photos' dedup keys.
  // Used to determine whether an album contains the currently selected
  // wallpaper image.
  std::map<std::string, std::set<std::string>> album_id_dedup_key_map_;

  // When local images are fetched, store the valid file paths in the set. This
  // is checked when the SWA requests thumbnail data or sets an image as the
  // user's background.
  std::set<base::FilePath> local_images_;

  const raw_ptr<content::WebUI> web_ui_ = nullptr;

  // Pointer to profile of user that opened personalization SWA. Not owned.
  const raw_ptr<Profile> profile_ = nullptr;

  base::ScopedObservation<ash::WallpaperController,
                          ash::WallpaperControllerObserver>
      wallpaper_controller_observer_{this};

  const std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
      wallpaper_fetcher_delegate_;

  // Place near bottom of class so this is cleaned up before any pending
  // callbacks are dropped.
  mojo::Receiver<ash::personalization_app::mojom::WallpaperProvider>
      wallpaper_receiver_{this};

  mojo::Remote<ash::personalization_app::mojom::WallpaperObserver>
      wallpaper_observer_remote_;

  // Used for interacting with local filesystem.
  base::WeakPtrFactory<PersonalizationAppWallpaperProviderImpl>
      backend_weak_ptr_factory_{this};

  // Used for fetching online image attribution.
  base::WeakPtrFactory<PersonalizationAppWallpaperProviderImpl>
      attribution_weak_ptr_factory_{this};

  // General use other than the specific cases above.
  base::WeakPtrFactory<PersonalizationAppWallpaperProviderImpl>
      weak_ptr_factory_{this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_WALLPAPER_PROVIDER_IMPL_H_
