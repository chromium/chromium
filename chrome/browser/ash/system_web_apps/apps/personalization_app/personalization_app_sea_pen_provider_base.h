// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_BASE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_BASE_H_

#include <map>
#include <memory>
#include <string>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/common/sea_pen_provider.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
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

using DecodeImageCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

// Base class for PersonalizationApp and VcBackground SeaPen providers.
// The public functions are the interface required for both PersonalizationApp
// and VcBackground. Shared code should be in the public functions or the
// private callback functions; while the non-shared code should be put into each
// implementation and have the protected pure virtual interface here.
class PersonalizationAppSeaPenProviderBase
    : public ::ash::common::SeaPenProvider,
      public ::ash::personalization_app::mojom::SeaPenProvider {
 public:
  PersonalizationAppSeaPenProviderBase(
      content::WebUI* web_ui,
      std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
          wallpaper_fetcher_delegate,
      manta::proto::FeatureName feature_name);

  ~PersonalizationAppSeaPenProviderBase() override;

  // ::ash::common::SeaPenProvider:
  void BindInterface(
      mojo::PendingReceiver<mojom::SeaPenProvider> receiver) override;

  // ::ash::personalization_app::mojom::SeaPenProvider:
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

  void OpenFeedbackDialog(mojom::SeaPenFeedbackMetadataPtr metadata) override;

  void ShouldShowSeaPenTermsOfServiceDialog(
      ShouldShowSeaPenTermsOfServiceDialogCallback callback) override;

  void HandleSeaPenTermsOfServiceAccepted() override;

 protected:
  virtual void SelectRecentSeaPenImageInternal(
      const base::FilePath& path,
      SelectRecentSeaPenImageCallback callback) = 0;

  virtual void GetRecentSeaPenImagesInternal(
      GetRecentSeaPenImagesCallback callback) = 0;

  virtual void GetRecentSeaPenImageThumbnailInternal(
      const base::FilePath& path,
      DecodeImageCallback callback) = 0;

  virtual void OnFetchWallpaperDoneInternal(
      const SeaPenImage& sea_pen_image,
      const std::string& query_info,
      base::OnceCallback<void(bool success)> callback) = 0;

  manta::proto::FeatureName feature_name_;

  // Pointer to profile of user that opened personalization SWA. Not owned.
  const raw_ptr<Profile> profile_;

  // When recent sea pen images are fetched, store the valid file paths in the
  // set. This is checked when the SWA requests thumbnail data or sets an image
  // as the user's background.
  std::set<base::FilePath> recent_sea_pen_images_;

  mojo::Receiver<mojom::SeaPenProvider> sea_pen_receiver_{this};

 private:
  wallpaper_handlers::SeaPenFetcher* GetOrCreateSeaPenFetcher();

  void OnFetchThumbnailsDone(SearchWallpaperCallback callback,
                             std::optional<std::vector<SeaPenImage>> images,
                             manta::MantaStatusCode status_code);

  void OnFetchWallpaperDone(SelectSeaPenThumbnailCallback callback,
                            std::optional<SeaPenImage> image);

  void OnRecentSeaPenImageSelected(bool success);

  void OnGetRecentSeaPenImages(GetRecentSeaPenImagesCallback callback,
                               const std::vector<base::FilePath>& images);

  void OnGetRecentSeaPenImageThumbnail(
      GetRecentSeaPenImageThumbnailCallback callback,
      const gfx::ImageSkia& image);

  SelectRecentSeaPenImageCallback pending_select_recent_sea_pen_image_callback_;

  const std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
      wallpaper_fetcher_delegate_;

  // A map of image id to image.
  std::map<uint32_t, const SeaPenImage> sea_pen_images_;

  // The last query made to the sea pen provider. This can be null when
  // SearchWallpaper() is never called.
  mojom::SeaPenQueryPtr last_query_;

  // Perform a network request to search/upscale available wallpapers.
  // Constructed lazily at the time of the first request and then persists for
  // the rest of the delegate's lifetime, unless preemptively or subsequently
  // replaced by a mock in a test.
  std::unique_ptr<wallpaper_handlers::SeaPenFetcher> sea_pen_fetcher_;

  base::WeakPtrFactory<PersonalizationAppSeaPenProviderBase> weak_ptr_factory_{
      this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_BASE_H_
