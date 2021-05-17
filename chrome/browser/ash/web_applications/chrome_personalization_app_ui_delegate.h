// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_

#include "chromeos/components/personalization_app/personalization_app_ui_delegate.h"

#include <stdint.h>

#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

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
class ChromePersonalizationAppUiDelegate : public PersonalizationAppUiDelegate {
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

  void SelectWallpaper(uint64_t image_asset_id,
                       SelectWallpaperCallback callback) override;

 private:
  mojo::Receiver<chromeos::personalization_app::mojom::WallpaperProvider>
      wallpaper_receiver_{this};

  void OnFetchCollections(FetchCollectionsCallback callback,
                          bool success,
                          const std::vector<backdrop::Collection>& collections);

  void OnFetchCollectionImages(FetchImagesForCollectionCallback callback,
                               bool success,
                               const std::vector<backdrop::Image>& images);

  std::unique_ptr<backdrop_wallpaper_handlers::CollectionInfoFetcher>
      wallpaper_collection_info_fetcher_;

  std::unique_ptr<backdrop_wallpaper_handlers::ImageInfoFetcher>
      wallpaper_images_info_fetcher_;

  // Store a mapping of valid image asset_ids to their URL to validate user
  // wallpaper selections.
  std::map<uint64_t, GURL> image_asset_id_map_;

  // Pointer to profile of user that opened personalization SWA. Not owned.
  Profile* const profile_ = nullptr;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_PERSONALIZATION_APP_UI_DELEGATE_H_
