// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_sea_pen_provider_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "components/manta/features.h"
#include "content/public/browser/web_ui.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace ash::vc_background_ui {

namespace {

CameraEffectsController* GetCameraEffectsController() {
  return Shell::Get()->camera_effects_controller();
}

void GetImageSkiaFromBackgroundImageInfo(
    personalization_app::DecodeImageCallback callback,
    const std::optional<CameraEffectsController::BackgroundImageInfo>& info) {
  if (!info.has_value()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // Deccode the jpeg content.
  std::unique_ptr<SkBitmap> bitmap = gfx::JPEGCodec::Decode(
      reinterpret_cast<const unsigned char*>(info->jpeg_bytes.data()),
      info->jpeg_bytes.size());

  auto image = gfx::ImageSkia::CreateFrom1xBitmap(*bitmap);

  std::move(callback).Run(image);
}
}  // namespace

VcBackgroundUISeaPenProviderImpl::VcBackgroundUISeaPenProviderImpl(
    content::WebUI* web_ui,
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate)
    : PersonalizationAppSeaPenProviderBase(
          web_ui,
          std::move(wallpaper_fetcher_delegate),
          manta::proto::FeatureName::CHROMEOS_VC_BACKGROUNDS) {}

VcBackgroundUISeaPenProviderImpl::~VcBackgroundUISeaPenProviderImpl() = default;

void VcBackgroundUISeaPenProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  CHECK(::ash::features::IsVcBackgroundReplaceEnabled());
  CHECK(::manta::features::IsMantaServiceEnabled());
  ::ash::personalization_app::PersonalizationAppSeaPenProviderBase::
      BindInterface(std::move(receiver));
}

void VcBackgroundUISeaPenProviderImpl::SelectRecentSeaPenImageInternal(
    const base::FilePath& path,
    SelectRecentSeaPenImageCallback callback) {
  GetCameraEffectsController()->SetBackgroundImage(path, std::move(callback));
}

void VcBackgroundUISeaPenProviderImpl::GetRecentSeaPenImagesInternal(
    GetRecentSeaPenImagesCallback callback) {
  GetCameraEffectsController()->GetBackgroundImageFileNames(
      std::move(callback));
}

void VcBackgroundUISeaPenProviderImpl::GetRecentSeaPenImageThumbnailInternal(
    const base::FilePath& path,
    personalization_app::DecodeImageCallback callback) {
  GetCameraEffectsController()->GetBackgroundImageInfo(
      path, base::BindOnce(&GetImageSkiaFromBackgroundImageInfo,
                           std::move(callback)));
}

void VcBackgroundUISeaPenProviderImpl::DeleteRecentSeaPenImage(
    const base::FilePath& path,
    DeleteRecentSeaPenImageCallback callback) {
  if (recent_sea_pen_images_.count(path) == 0) {
    sea_pen_receiver_.ReportBadMessage("Invalid Sea Pen image received");
    return;
  }

  GetCameraEffectsController()->RemoveBackgroundImage(path,
                                                      std::move(callback));
}

void VcBackgroundUISeaPenProviderImpl::OnFetchWallpaperDoneInternal(
    const SeaPenImage& sea_pen_image,
    const ash::personalization_app::mojom::SeaPenQueryPtr& query,
    base::OnceCallback<void(bool success)> callback) {
  const std::string metadata = QueryDictToXmpString(SeaPenQueryToDict(query));
  GetCameraEffectsController()->SetBackgroundImageFromContent(
      sea_pen_image, metadata, std::move(callback));
}

}  // namespace ash::vc_background_ui
