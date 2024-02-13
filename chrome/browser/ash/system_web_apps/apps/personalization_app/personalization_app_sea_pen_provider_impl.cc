// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/path_service.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper/wallpaper_enumerator.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/manta/features.h"
#include "content/public/browser/web_ui.h"

namespace ash::personalization_app {

PersonalizationAppSeaPenProviderImpl::PersonalizationAppSeaPenProviderImpl(
    content::WebUI* web_ui,
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate)
    : PersonalizationAppSeaPenProviderBase(
          web_ui,
          std::move(wallpaper_fetcher_delegate),
          manta::proto::FeatureName::CHROMEOS_WALLPAPER) {}

PersonalizationAppSeaPenProviderImpl::~PersonalizationAppSeaPenProviderImpl() =
    default;

void PersonalizationAppSeaPenProviderImpl::BindInterface(
    mojo::PendingReceiver<::ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  CHECK(::ash::features::IsSeaPenEnabled());
  CHECK(manta::features::IsMantaServiceEnabled());
  PersonalizationAppSeaPenProviderBase::BindInterface(std::move(receiver));
}

void PersonalizationAppSeaPenProviderImpl::SelectRecentSeaPenImageInternal(
    const base::FilePath& path,
    SelectRecentSeaPenImageCallback callback) {
  ash::WallpaperController* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  wallpaper_controller->SetSeaPenWallpaperFromFile(GetAccountId(profile_), path,
                                                   std::move(callback));
}

void PersonalizationAppSeaPenProviderImpl::GetRecentSeaPenImagesInternal(
    GetRecentSeaPenImagesCallback callback) {
  base::FilePath wallpaper_dir;
  CHECK(
      base::PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
  const base::FilePath sea_pen_wallpaper_dir =
      wallpaper_dir.Append("sea_pen/")
          .Append(GetAccountId(profile_).GetAccountIdKey());
  ash::EnumerateJpegFilesFromDir(profile_, sea_pen_wallpaper_dir,
                                 std::move(callback));
}

void PersonalizationAppSeaPenProviderImpl::
    GetRecentSeaPenImageThumbnailInternal(const base::FilePath& path,
                                          DecodeImageCallback callback) {
  image_util::DecodeImageFile(std::move(callback), path);
}

void PersonalizationAppSeaPenProviderImpl::DeleteRecentSeaPenImage(
    const base::FilePath& path,
    DeleteRecentSeaPenImageCallback callback) {
  if (recent_sea_pen_images_.count(path) == 0) {
    sea_pen_receiver_.ReportBadMessage("Invalid Sea Pen image received");
    return;
  }

  auto* wallpaper_controller = ash::WallpaperController::Get();
  DCHECK(wallpaper_controller);

  wallpaper_controller->DeleteRecentSeaPenImage(GetAccountId(profile_), path,
                                                std::move(callback));
}

void PersonalizationAppSeaPenProviderImpl::OnFetchWallpaperDoneInternal(
    const SeaPenImage& sea_pen_image,
    const mojom::SeaPenQueryPtr& query,
    base::OnceCallback<void(bool success)> callback) {
  auto* wallpaper_controller = ash::WallpaperController::Get();
  wallpaper_controller->SetSeaPenWallpaper(
      GetAccountId(profile_), sea_pen_image, query, std::move(callback));
}

}  // namespace ash::personalization_app
