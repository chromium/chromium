// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SEA_PEN_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SEA_PEN_PROVIDER_IMPL_H_

#include <memory>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::vc_background_ui {

// Implementation of a SeaPenProvider for VC Background WebUI. Sends/receives
// images via CameraEffectsController to set video chat background.
class VcBackgroundUISeaPenProviderImpl
    : public personalization_app::PersonalizationAppSeaPenProviderBase {
 public:
  explicit VcBackgroundUISeaPenProviderImpl(
      content::WebUI* web_ui,
      std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
          wallpaper_fetcher_delegate);

  VcBackgroundUISeaPenProviderImpl(const VcBackgroundUISeaPenProviderImpl&) =
      delete;
  VcBackgroundUISeaPenProviderImpl& operator=(
      const VcBackgroundUISeaPenProviderImpl&) = delete;

  ~VcBackgroundUISeaPenProviderImpl() override;

  // ::ash::personalization_app::PersonalizationAppSeaPenProviderBase:
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::SeaPenProvider>
          receiver) override;

  // ::ash::personalization_app::mojom::SeaPenProvider:
  void DeleteRecentSeaPenImage(
      uint32_t id,
      DeleteRecentSeaPenImageCallback callback) override;

 private:
  // ::ash::personalization_app::PersonalizationAppSeaPenProviderBase:
  void SelectRecentSeaPenImageInternal(
      uint32_t id,
      SelectRecentSeaPenImageCallback callback) override;

  void GetRecentSeaPenImagesInternal(
      GetRecentSeaPenImagesCallback callback) override;

  void GetRecentSeaPenImageThumbnailInternal(
      uint32_t id,
      SeaPenWallpaperManager::GetImageAndMetadataCallback callback) override;

  void OnFetchWallpaperDoneInternal(
      const SeaPenImage& sea_pen_image,
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      base::OnceCallback<void(bool success)> callback) override;
};

}  // namespace ash::vc_background_ui

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SEA_PEN_PROVIDER_IMPL_H_
