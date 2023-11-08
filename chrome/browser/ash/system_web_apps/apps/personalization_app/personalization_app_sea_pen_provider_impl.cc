// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/manta/features.h"
#include "content/public/browser/web_ui.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace ash::personalization_app {

namespace {

bool SetImageAsWallpaper(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    LOG(WARNING) << "Failed to decode image";
    return false;
  }
  // TODO(b/304581889) save as a permanent wallpaper type.
  WallpaperController::Get()->ShowOneShotWallpaper(
      gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  return true;
}

}  // namespace

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
    const std::string& text,
    SearchWallpaperCallback callback) {
  if (text.length() > mojom::kMaximumSearchWallpaperTextBytes) {
    sea_pen_receiver_.ReportBadMessage(
        "SearchWallpaper exceeded maximum text length");
    return;
  }
  auto* sea_pen_fetcher = GetOrCreateSeaPenFetcher();
  CHECK(sea_pen_fetcher);
  sea_pen_fetcher->Start(
      text,
      base::BindOnce(&PersonalizationAppSeaPenProviderImpl::OnSeaPenFetcherDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderImpl::SelectSeaPenThumbnail(
    uint32_t id,
    SelectSeaPenThumbnailCallback callback) {
  auto it = sea_pen_images_.find(id);
  if (it == sea_pen_images_.end()) {
    sea_pen_receiver_.ReportBadMessage("Unknown wallpaper image selected");
    return;
  }
  // TODO(b/300127993) save image on disk before setting as wallpaper.
  data_decoder::DecodeImageIsolated(
      base::as_bytes(base::make_span(it->second.jpg_bytes)),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&SetImageAsWallpaper).Then(std::move(callback)));
}

wallpaper_handlers::SeaPenFetcher*
PersonalizationAppSeaPenProviderImpl::GetOrCreateSeaPenFetcher() {
  if (!sea_pen_fetcher_) {
    sea_pen_fetcher_ =
        wallpaper_fetcher_delegate_->CreateSeaPenFetcher(profile_);
  }
  return sea_pen_fetcher_.get();
}

void PersonalizationAppSeaPenProviderImpl::OnSeaPenFetcherDone(
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

}  // namespace ash::personalization_app
