// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SEA_PEN_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SEA_PEN_PROVIDER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/image/image_skia.h"

namespace ash::vc_background_ui {

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
      personalization_app::DecodeImageCallback callback) override;

  void OnFetchWallpaperDoneInternal(
      const SeaPenImage& sea_pen_image,
      const std::string& query_info,
      base::OnceCallback<void(bool success)> callback) override;
};

}  // namespace ash::vc_background_ui

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SEA_PEN_PROVIDER_IMPL_H_
