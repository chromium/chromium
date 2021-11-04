// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_

#include "ash/webui/personalization_app/personalization_app_ui_delegate.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace ash {
class ThumbnailLoader;
}  // namespace ash

namespace backdrop {
class Collection;
class Image;
}  // namespace backdrop

namespace backdrop_wallpaper_handlers {
class CollectionInfoFetcher;
class ImageInfoFetcher;
}  // namespace backdrop_wallpaper_handlers

namespace content {
class WebUI;
}  // namespace content

class Profile;

// Implemented in //chrome because this will rely on chrome
// |backdrop_wallpaper_handlers| code when fully implemented.
// TODO(b/182012641) add wallpaper API code here.
class ChromePersonalizationAppUiDelegate
    : public ash::PersonalizationAppUiDelegate,
      ash::WallpaperControllerObserver {
 public:
  explicit ChromePersonalizationAppUiDelegate(content::WebUI* web_ui);

  ChromePersonalizationAppUiDelegate(
      const ChromePersonalizationAppUiDelegate&) = delete;
  ChromePersonalizationAppUiDelegate& operator=(
      const ChromePersonalizationAppUiDelegate&) = delete;

  ~ChromePersonalizationAppUiDelegate() override;

  // PersonalizationAppUIDelegate:
  // |BindInterface| may be called multiple times, for example if the user
  // presses Ctrl+Shift+R while on the personalization app.
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::WallpaperProvider>
          receiver) override;

  // ash::personalization_app::mojom::WallpaperProvider:

  // Configure the window to be transparent so that the user can trigger a "full
  // screen preview" mode. This allows the user to see through the app window to
  // see the chosen wallpaper. This is safe to call multiple times in a row.
  void MakeTransparent() override;

  void FetchCollections(FetchCollectionsCallback callback) override;

  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;

  void GetLocalImages(GetLocalImagesCallback callback) override;

  void GetLocalImageThumbnail(const base::FilePath& file_path,
                              GetLocalImageThumbnailCallback callback) override;

  void SetWallpaperObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
          observer) override;

  // ash::WallpaperControllerObserver:
  void OnWallpaperChanged() override;

  // ash::WallpaperControllerObserver:
  void OnWallpaperPreviewEnded() override;

  // ash::personalization_app::mojom::WallpaperProvider:
  void SelectWallpaper(uint64_t image_asset_id,
                       bool preview_mode,
                       SelectWallpaperCallback callback) override;

  void SelectLocalImage(const base::FilePath& path,
                        ash::WallpaperLayout layout,
                        bool preview_mode,
                        SelectLocalImageCallback callback) override;

  void SetCustomWallpaperLayout(ash::WallpaperLayout layout) override;

  void SetDailyRefreshCollectionId(const std::string& collection_id) override;

  void GetDailyRefreshCollectionId(
      GetDailyRefreshCollectionIdCallback callback) override;

  void UpdateDailyRefreshWallpaper(
      UpdateDailyRefreshWallpaperCallback callback) override;

  void IsInTabletMode(IsInTabletModeCallback callback) override;

  void ConfirmPreviewWallpaper() override;

  void CancelPreviewWallpaper() override;

 private:
  friend class ChromePersonalizationAppUiDelegateTest;

  void OnFetchCollections(bool success,
                          const std::vector<backdrop::Collection>& collections);

  void OnFetchCollectionImages(
      FetchImagesForCollectionCallback callback,
      std::unique_ptr<backdrop_wallpaper_handlers::ImageInfoFetcher> fetcher,
      bool success,
      const std::string& collection_id,
      const std::vector<backdrop::Image>& images);

  void OnGetLocalImages(GetLocalImagesCallback callback,
                        const std::vector<base::FilePath>& images);

  void OnGetLocalImageThumbnail(GetLocalImageThumbnailCallback callback,
                                const SkBitmap* bitmap,
                                base::File::Error error);

  // Called after attempting selecting an online wallpaper. Will be dropped if
  // new requests come in.
  void OnOnlineWallpaperSelected(bool success);

  // Called after attempting selecting a local image. Will be dropped if new
  // requests come in.
  void OnLocalImageSelected(bool success);

  // Called after attempting updating a daily refresh wallpaper. Will be dropped
  // if new requests come in.
  void OnDailyRefreshWallpaperUpdated(bool success);

  void FindAttribution(
      const ash::WallpaperInfo& info,
      const GURL& wallpaper_data_url,
      const absl::optional<std::vector<backdrop::Collection>>& collections);

  void FindAttributionInCollection(
      const ash::WallpaperInfo& info,
      const GURL& wallpaper_data_url,
      std::size_t current_index,
      const absl::optional<std::vector<backdrop::Collection>>& collections,
      bool success,
      const std::string& collection_id,
      const std::vector<backdrop::Image>& images);

  // Called when the user sets an image, or cancels/confirms preview wallpaper.
  // If a new image is set in preview mode, will minimize all windows except the
  // wallpaper SWA. When canceling or confirming preview mode, will restore the
  // minimized windows to their previous state.
  void SetMinimizedWindowStateForPreview(bool preview_mode);

  void NotifyWallpaperChanged(
      ash::personalization_app::mojom::CurrentWallpaperPtr current_wallpaper);

  std::unique_ptr<backdrop_wallpaper_handlers::CollectionInfoFetcher>
      wallpaper_collection_info_fetcher_;
  std::vector<FetchCollectionsCallback> pending_collections_callbacks_;

  std::unique_ptr<backdrop_wallpaper_handlers::ImageInfoFetcher>
      wallpaper_attribution_info_fetcher_;

  SelectWallpaperCallback pending_select_wallpaper_callback_;

  SelectLocalImageCallback pending_select_local_image_callback_;

  UpdateDailyRefreshWallpaperCallback
      pending_update_daily_refresh_wallpaper_callback_;

  std::unique_ptr<ash::ThumbnailLoader> thumbnail_loader_;

  struct ImageInfo {
    GURL image_url;
    std::string collection_id;
  };

  // Store a mapping of valid image asset_ids to their ImageInfo to validate
  // user wallpaper selections.
  std::map<uint64_t, ImageInfo> image_asset_id_map_;

  // When local images are fetched, store the valid file paths in the set. This
  // is checked when the SWA requests thumbnail data or sets an image as the
  // user's background.
  std::set<base::FilePath> local_images_;

  content::WebUI* const web_ui_ = nullptr;

  // Pointer to profile of user that opened personalization SWA. Not owned.
  Profile* const profile_ = nullptr;

  base::ScopedObservation<ash::WallpaperController,
                          ash::WallpaperControllerObserver>
      wallpaper_controller_observer_{this};

  // Place near bottom of class so this is cleaned up before any pending
  // callbacks are dropped.
  mojo::Receiver<ash::personalization_app::mojom::WallpaperProvider>
      wallpaper_receiver_{this};

  mojo::Remote<ash::personalization_app::mojom::WallpaperObserver>
      wallpaper_observer_remote_;

  // Used for interacting with local filesystem.
  base::WeakPtrFactory<ChromePersonalizationAppUiDelegate>
      backend_weak_ptr_factory_{this};

  // Used for fetching online image attribution.
  base::WeakPtrFactory<ChromePersonalizationAppUiDelegate>
      attribution_weak_ptr_factory_{this};

  // General use other than the specific cases above.
  base::WeakPtrFactory<ChromePersonalizationAppUiDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_
