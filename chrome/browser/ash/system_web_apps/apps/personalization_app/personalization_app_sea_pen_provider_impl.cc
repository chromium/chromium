// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper/wallpaper_enumerator.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/manta/features.h"
#include "components/manta/proto/manta.pb.h"
#include "content/public/browser/web_ui.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::personalization_app {

namespace {
constexpr int kSeaPenImageThumbnailSizeDip = 512;
}

PersonalizationAppSeaPenProviderImpl::PersonalizationAppSeaPenProviderImpl(
    content::WebUI* web_ui,
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate)
    : profile_(Profile::FromWebUI(web_ui)),
      wallpaper_fetcher_delegate_(std::move(wallpaper_fetcher_delegate)) {}

PersonalizationAppSeaPenProviderImpl::~PersonalizationAppSeaPenProviderImpl() =
    default;

void PersonalizationAppSeaPenProviderImpl::BindInterface(
    mojo::PendingReceiver<::ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  CHECK(manta::features::IsMantaServiceEnabled() &&
        features::IsSeaPenEnabled());
  sea_pen_receiver_.reset();
  sea_pen_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppSeaPenProviderImpl::SearchWallpaper(
    const mojom::SeaPenQueryPtr query,
    SearchWallpaperCallback callback) {
  if (query->is_text_query() && query->get_text_query().length() >
                                    mojom::kMaximumSearchWallpaperTextBytes) {
    sea_pen_receiver_.ReportBadMessage(
        "SearchWallpaper exceeded maximum text length");
    return;
  }
  auto* sea_pen_fetcher = GetOrCreateSeaPenFetcher();
  CHECK(sea_pen_fetcher);
  sea_pen_fetcher->FetchThumbnails(
      query, base::BindOnce(
                 &PersonalizationAppSeaPenProviderImpl::OnFetchThumbnailsDone,
                 weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderImpl::SelectSeaPenThumbnail(
    uint32_t id,
    SelectSeaPenThumbnailCallback callback) {
  const auto it = sea_pen_images_.find(id);
  if (it == sea_pen_images_.end()) {
    sea_pen_receiver_.ReportBadMessage("Unknown wallpaper image selected");
    return;
  }

  auto* sea_pen_fetcher = GetOrCreateSeaPenFetcher();
  CHECK(sea_pen_fetcher);
  sea_pen_fetcher->FetchWallpaper(
      it->second,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderImpl::OnFetchWallpaperDone,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderImpl::SelectRecentSeaPenImage(
    const base::FilePath& path,
    SelectRecentSeaPenImageCallback callback) {
  auto* wallpaper_controller = ash::WallpaperController::Get();
  DCHECK(wallpaper_controller);

  wallpaper_controller->SetSeaPenWallpaperFromFile(GetAccountId(profile_), path,
                                                   std::move(callback));
}

void PersonalizationAppSeaPenProviderImpl::GetRecentSeaPenImages(
    GetRecentSeaPenImagesCallback callback) {
  base::FilePath wallpaper_dir;
  CHECK(
      base::PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
  const base::FilePath sea_pen_wallpaper_dir = wallpaper_dir.Append("sea_pen/");
  ash::EnumerateJpegFilesFromDir(
      profile_, sea_pen_wallpaper_dir,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderImpl::OnGetRecentSeaPenImages,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderImpl::GetRecentSeaPenImageThumbnail(
    const base::FilePath& path,
    GetRecentSeaPenImageThumbnailCallback callback) {
  if (recent_sea_pen_images_.count(path) == 0) {
    LOG(ERROR) << __func__ << " Invalid sea pen image received";
    std::move(callback).Run(GURL());
    return;
  }
  image_util::DecodeImageFile(
      base::BindOnce(&PersonalizationAppSeaPenProviderImpl::
                         OnGetRecentSeaPenImageThumbnail,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      path);
}

wallpaper_handlers::SeaPenFetcher*
PersonalizationAppSeaPenProviderImpl::GetOrCreateSeaPenFetcher() {
  if (!sea_pen_fetcher_) {
    sea_pen_fetcher_ =
        wallpaper_fetcher_delegate_->CreateSeaPenFetcher(profile_);
  }
  return sea_pen_fetcher_.get();
}

void PersonalizationAppSeaPenProviderImpl::OnFetchThumbnailsDone(
    SearchWallpaperCallback callback,
    absl::optional<std::vector<SeaPenImage>> images) {
  if (!images) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  sea_pen_images_.clear();
  std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr> result;
  for (auto& image : images.value()) {
    const auto image_id = image.id;
    auto [it, _] = sea_pen_images_.insert(
        std::pair<uint32_t, SeaPenImage>(image_id, std::move(image)));
    result.emplace_back(absl::in_place, GetJpegDataUrl(it->second.jpg_bytes),
                        image_id);
  }
  std::move(callback).Run(std::move(result));
}

void PersonalizationAppSeaPenProviderImpl::OnFetchWallpaperDone(
    SelectSeaPenThumbnailCallback callback,
    absl::optional<SeaPenImage> image) {
  if (!image) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto* wallpaper_controller = ash::WallpaperController::Get();
  wallpaper_controller->SetSeaPenWallpaper(GetAccountId(profile_), *image,
                                           std::move(callback));
}

void PersonalizationAppSeaPenProviderImpl::OnGetRecentSeaPenImages(
    GetRecentSeaPenImagesCallback callback,
    const std::vector<base::FilePath>& images) {
  recent_sea_pen_images_ =
      std::set<base::FilePath>(images.begin(), images.end());
  std::move(callback).Run(images);
}

void PersonalizationAppSeaPenProviderImpl::OnGetRecentSeaPenImageThumbnail(
    GetRecentSeaPenImageThumbnailCallback callback,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Do not call |mojom::ReportBadMessage| here. The message is valid, but
    // the jpeg file may be corrupt or unreadable.
    std::move(callback).Run(GURL());
    return;
  }
  std::move(callback).Run(GURL(webui::GetBitmapDataUrl(
      *WallpaperResizer::GetResizedImage(image, kSeaPenImageThumbnailSizeDip)
           .bitmap())));
}

}  // namespace ash::personalization_app
