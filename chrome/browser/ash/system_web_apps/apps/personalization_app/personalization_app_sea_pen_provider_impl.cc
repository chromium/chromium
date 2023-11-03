// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/manta/features.h"
#include "content/public/browser/web_ui.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace ash::personalization_app {

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
    const absl::optional<std::vector<ash::SeaPenImage>>& images) {
  if (!images) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr> result;
  for (const auto& image : images.value()) {
    result.emplace_back(absl::in_place, GetJpegDataUrl(image.jpg_bytes),
                        image.id);
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace ash::personalization_app
