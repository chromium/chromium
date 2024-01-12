// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_

#include <map>
#include <memory>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"
#include "components/manta/manta_status.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/image/image_skia.h"

namespace ash::personalization_app {

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

  void DeleteRecentSeaPenImage(
      const base::FilePath& path,
      DeleteRecentSeaPenImageCallback callback) override;

 private:
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
      const std::string& query_info,
      base::OnceCallback<void(bool success)> callback) override;
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_SEA_PEN_PROVIDER_IMPL_H_
