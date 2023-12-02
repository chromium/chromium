// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_

#include "ash/webui/personalization_app/mojom/sea_pen.mojom.h"
#include "ash/webui/personalization_app/personalization_app_sea_pen_provider.h"

#include <map>
#include <memory>
#include <string>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class WebUI;
}  // namespace content

namespace wallpaper_handlers {
class WallpaperFetcherDelegate;
class SeaPenFetcher;
}  // namespace wallpaper_handlers

class Profile;

namespace ash::personalization_app {

class PersonalizationAppSeaPenProviderImpl
    : public PersonalizationAppSeaPenProvider {
 public:
  explicit PersonalizationAppSeaPenProviderImpl(
      content::WebUI* web_ui,
      std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
          wallpaper_fetcher_delegate);

  PersonalizationAppSeaPenProviderImpl(
      const PersonalizationAppSeaPenProviderImpl&) = delete;
  PersonalizationAppSeaPenProviderImpl& operator=(
      const PersonalizationAppSeaPenProviderImpl&) = delete;

  ~PersonalizationAppSeaPenProviderImpl() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SeaPenProvider> receiver) override;

  void SearchWallpaper(mojom::SeaPenQueryPtr query,
                       SearchWallpaperCallback callback) override;

  void SelectSeaPenThumbnail(uint32_t id,
                             SelectSeaPenThumbnailCallback callback) override;

  void SelectRecentSeaPenImage(
      const base::FilePath& path,
      SelectRecentSeaPenImageCallback callback) override;

  void GetRecentSeaPenImages(GetRecentSeaPenImagesCallback callback) override;

  void GetRecentSeaPenImageThumbnail(
      const base::FilePath& path,
      GetRecentSeaPenImageThumbnailCallback callback) override;

 private:
  wallpaper_handlers::SeaPenFetcher* GetOrCreateSeaPenFetcher();

  void OnFetchThumbnailsDone(SearchWallpaperCallback callback,
                             absl::optional<std::vector<SeaPenImage>> images);

  void OnFetchWallpaperDone(SelectSeaPenThumbnailCallback callback,
                            absl::optional<SeaPenImage> image);

  void OnGetRecentSeaPenImages(GetRecentSeaPenImagesCallback callback,
                               const std::vector<base::FilePath>& images);

  void OnGetRecentSeaPenImageThumbnail(
      GetRecentSeaPenImageThumbnailCallback callback,
      const gfx::ImageSkia& image);

  // Pointer to profile of user that opened personalization SWA. Not owned.
  const raw_ptr<Profile> profile_;

  const std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
      wallpaper_fetcher_delegate_;

  // A map of image id to image.
  std::map<uint32_t, const SeaPenImage> sea_pen_images_;

  // When recent sea pen images are fetched, store the valid file paths in the
  // set. This is checked when the SWA requests thumbnail data or sets an image
  // as the user's background.
  std::set<base::FilePath> recent_sea_pen_images_;

  // Perform a network request to search/upscale available wallpapers.
  // Constructed lazily at the time of the first request and then persists for
  // the rest of the delegate's lifetime, unless preemptively or subsequently
  // replaced by a mock in a test.
  std::unique_ptr<wallpaper_handlers::SeaPenFetcher> sea_pen_fetcher_;

  mojo::Receiver<mojom::SeaPenProvider> sea_pen_receiver_{this};

  base::WeakPtrFactory<PersonalizationAppSeaPenProviderImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_
