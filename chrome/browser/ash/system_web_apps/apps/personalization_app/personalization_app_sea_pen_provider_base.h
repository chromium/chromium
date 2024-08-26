// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_BASE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_BASE_H_

#include <map>
#include <memory>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/common/sea_pen_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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

  bool IsEligibleForSeaPen() override;

  bool IsEligibleForSeaPenTextInput() override;

  bool IsManagedSeaPenEnabled() override;

  bool IsManagedSeaPenFeedbackEnabled() override;

  // ::ash::personalization_app::mojom::SeaPenProvider:
  void SetSeaPenObserver(
      mojo::PendingRemote<mojom::SeaPenObserver> observer) override;

  void GetSeaPenThumbnails(mojom::SeaPenQueryPtr query,
                           GetSeaPenThumbnailsCallback callback) override;

  void SelectSeaPenThumbnail(uint32_t id,
                             bool preview_mode,
                             SelectSeaPenThumbnailCallback callback) override;

  void SelectRecentSeaPenImage(
      uint32_t id,
      bool preview_mode,
      SelectRecentSeaPenImageCallback callback) override;

  void GetRecentSeaPenImageIds(
      GetRecentSeaPenImageIdsCallback callback) override;

  void GetRecentSeaPenImageThumbnail(
      uint32_t id,
      GetRecentSeaPenImageThumbnailCallback callback) override;

  void OpenFeedbackDialog(mojom::SeaPenFeedbackMetadataPtr metadata) override;

  void ShouldShowSeaPenIntroductionDialog(
      ShouldShowSeaPenIntroductionDialogCallback callback) override;

  void HandleSeaPenIntroductionDialogClosed() override;

  void IsInTabletMode(IsInTabletModeCallback callback) override;

  void MakeTransparent() override;

  wallpaper_handlers::SeaPenFetcher* GetOrCreateSeaPenFetcher();

 protected:
  virtual void SetSeaPenObserverInternal() = 0;

  virtual void SelectRecentSeaPenImageInternal(
      uint32_t id,
      bool preview_mode,
      SelectRecentSeaPenImageCallback callback) = 0;

  virtual bool IsManagedSeaPenEnabledInternal() = 0;

  virtual bool IsManagedSeaPenFeedbackEnabledInternal() = 0;

  virtual void GetRecentSeaPenImageIdsInternal(
      GetRecentSeaPenImageIdsCallback callback) = 0;

  virtual void GetRecentSeaPenImageThumbnailInternal(
      uint32_t id,
      SeaPenWallpaperManager::GetImageAndMetadataCallback callback) = 0;

  virtual void ShouldShowSeaPenIntroductionDialogInternal(
      ShouldShowSeaPenIntroductionDialogCallback callback) = 0;

  virtual void HandleSeaPenIntroductionDialogClosedInternal() = 0;

  virtual void OnFetchWallpaperDoneInternal(
      const SeaPenImage& sea_pen_image,
      const mojom::SeaPenQueryPtr& query,
      bool preview_mode,
      base::OnceCallback<void(bool success)> callback) = 0;

  manta::proto::FeatureName feature_name_;

  // Provides updates to WebUI about SeaPen changes in browser process.
  mojo::Remote<mojom::SeaPenObserver> sea_pen_observer_remote_;

  // Pointer to profile of user that opened personalization SWA. Not owned.
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  // When recent sea pen images are fetched, store the valid ids in the
  // set. This is checked when the SWA requests thumbnail data or sets an image
  // as the user's background. These images are already stored to disk in a
  // special SeaPen directory.
  std::set<uint32_t> recent_sea_pen_image_ids_;

  mojo::Receiver<mojom::SeaPenProvider> sea_pen_receiver_{this};

 private:
  void OnFetchThumbnailsDone(GetSeaPenThumbnailsCallback callback,
                             const mojom::SeaPenQueryPtr& query,
                             std::optional<std::vector<SeaPenImage>> images,
                             manta::MantaStatusCode status_code);

  void OnFetchWallpaperDone(SelectSeaPenThumbnailCallback callback,
                            const mojom::SeaPenQueryPtr& query,
                            bool preview_mode,
                            std::optional<SeaPenImage> image);

  void OnRecentSeaPenImageSelected(bool success);

  void OnGetRecentSeaPenImageIds(GetRecentSeaPenImageIdsCallback callback,
                                 const std::vector<uint32_t>& ids);

  void OnGetRecentSeaPenImageThumbnail(
      uint32_t id,
      GetRecentSeaPenImageThumbnailCallback callback,
      const gfx::ImageSkia& image,
      mojom::RecentSeaPenImageInfoPtr image_info);

  void NotifyTextQueryHistoryChanged();

  std::optional<
      std::pair<mojom::SeaPenQueryPtr,
                std::map<uint32_t, const SeaPenImage>::const_iterator>>
  FindImageThumbnail(uint32_t id);

  SelectRecentSeaPenImageCallback pending_select_recent_sea_pen_image_callback_;

  const std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
      wallpaper_fetcher_delegate_;

  // A map of image id to image. These are not yet stored to disk. They are
  // thumbnail sized and only stored in memory.
  std::map<uint32_t, const SeaPenImage> sea_pen_images_;

  // The last query made to the sea pen provider. This can be null when
  // GetSeaPenThumbnails() is never called.
  mojom::SeaPenQueryPtr last_query_;

  // Stores the previous text queries. The first element in the pair is the
  // query string and the second element is a map of image id to image.
  std::vector<std::pair<std::string, std::map<uint32_t, const SeaPenImage>>>
      text_query_history_;

  // Perform a network request to search/upscale available wallpapers.
  // Constructed lazily at the time of the first request and then persists for
  // the rest of the delegate's lifetime, unless preemptively or subsequently
  // replaced by a mock in a test.
  std::unique_ptr<wallpaper_handlers::SeaPenFetcher> sea_pen_fetcher_;

  const raw_ptr<content::WebUI> web_ui_ = nullptr;

  base::WeakPtrFactory<PersonalizationAppSeaPenProviderBase> weak_ptr_factory_{
      this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_BASE_H_
