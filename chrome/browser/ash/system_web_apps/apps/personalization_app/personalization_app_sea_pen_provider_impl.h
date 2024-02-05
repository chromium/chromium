// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_

#include <memory>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::personalization_app {

// Implementation of a SeaPenProvider for Personalization App WebUI.
// Sends/receives images via WallpaperController to set as ChromeOS system
// wallpaper.
class PersonalizationAppSeaPenProviderImpl
    : public PersonalizationAppSeaPenProviderBase {
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

  // ::ash::personalization_app::PersonalizationAppSeaPenProviderBase:
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::SeaPenProvider>
          receiver) override;

  // ::ash::personalization_app::mojom::SeaPenProvider:
  void DeleteRecentSeaPenImage(
      const base::FilePath& path,
      DeleteRecentSeaPenImageCallback callback) override;

 private:
  // ::ash::personalization_app::PersonalizationAppSeaPenProviderBase:
  void SelectRecentSeaPenImageInternal(
      const base::FilePath& path,
      SelectRecentSeaPenImageCallback callback) override;

  void GetRecentSeaPenImagesInternal(
      GetRecentSeaPenImagesCallback callback) override;

  void GetRecentSeaPenImageThumbnailInternal(
      const base::FilePath& path,
      DecodeImageCallback callback) override;

  void OnFetchWallpaperDoneInternal(
      const SeaPenImage& sea_pen_image,
      const mojom::SeaPenQueryPtr& query,
      base::OnceCallback<void(bool success)> callback) override;
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_
