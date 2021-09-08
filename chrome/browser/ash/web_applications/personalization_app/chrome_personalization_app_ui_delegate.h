// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_

#include "chromeos/components/personalization_app/personalization_app_ui_delegate.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/public/cpp/wallpaper/local_image_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
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
class ChromePersonalizationAppUiDelegate : public PersonalizationAppUiDelegate,
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
  void BindInterface(mojo::PendingReceiver<
                     chromeos::personalization_app::mojom::WallpaperProvider>
                         receiver) override;

  // chromeos::personalization_app::mojom::WallpaperProvider:
  void FetchCollections(FetchCollectionsCallback callback) override;

  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;

  void GetLocalImages(GetLocalImagesCallback callback) override;

  void GetLocalImageThumbnail(const base::UnguessableToken& file_path,
                              GetLocalImageThumbnailCallback callback) override;

  void SetWallpaperObserver(
      mojo::PendingRemote<
          chromeos::personalization_app::mojom::WallpaperObserver> observer)
      override;

  // ash::WallpaperControllerObserver:
  void OnWallpaperChanged() override;

  // chromeos::personalization_app::mojom::WallpaperProvider:
  void SelectWallpaper(uint64_t image_asset_id,
                       SelectWallpaperCallback callback) override;

  void SelectLocalImage(const base::UnguessableToken& id,
                        SelectLocalImageCallback callback) override;

  void SetCustomWallpaperLayout(ash::WallpaperLayout layout) override;

  void SetDailyRefreshCollectionId(const std::string& collection_id) override;

  void GetDailyRefreshCollectionId(
      GetDailyRefreshCollectionIdCallback callback) override;

  void UpdateDailyRefreshWallpaper(
      UpdateDailyRefreshWallpaperCallback callback) override;

 private:
  friend class ChromePersonalizationAppUiDelegateTest;

  void OnFetchCollections(bool success,
                          const std::vector<backdrop::Collection>& collections);

  void OnFetchCollectionImages(FetchImagesForCollectionCallback callback,
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

  AccountId GetAccountId() const;

  void NotifyWallpaperChanged(
      chromeos::personalization_app::mojom::CurrentWallpaperPtr
          current_wallpaper);

  std::unique_ptr<backdrop_wallpaper_handlers::CollectionInfoFetcher>
      wallpaper_collection_info_fetcher_;
  std::vector<FetchCollectionsCallback> pending_collections_callbacks_;

  std::unique_ptr<backdrop_wallpaper_handlers::ImageInfoFetcher>
      wallpaper_images_info_fetcher_;

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

  // When local images are fetched, assign each one a random |UnguessableToken|
  // id. Store a mapping from these tokens to |ash::LocalImageInfo|. The SWA
  // passes a token id to get an image thumbnail preview.
  std::map<base::UnguessableToken, ash::LocalImageInfo> local_image_info_map_;

  // Pointer to profile of user that opened personalization SWA. Not owned.
  Profile* const profile_ = nullptr;

  base::ScopedObservation<ash::WallpaperController,
                          ash::WallpaperControllerObserver>
      wallpaper_controller_observer_{this};

  // Place near bottom of class so this is cleaned up before any pending
  // callbacks are dropped.
  mojo::Receiver<chromeos::personalization_app::mojom::WallpaperProvider>
      wallpaper_receiver_{this};

  mojo::Remote<chromeos::personalization_app::mojom::WallpaperObserver>
      wallpaper_observer_remote_;

  // Used for interacting with local filesystem.
  base::WeakPtrFactory<ChromePersonalizationAppUiDelegate>
      backend_weak_ptr_factory_{this};

  // Used for fetching online image attribution.
  base::WeakPtrFactory<ChromePersonalizationAppUiDelegate>
      attribution_weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_
