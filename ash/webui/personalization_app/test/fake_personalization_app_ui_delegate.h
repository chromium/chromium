// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_UI_DELEGATE_H_
#define ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_UI_DELEGATE_H_

#include "ash/webui/personalization_app/personalization_app_ui_delegate.h"

#include <stdint.h>

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

class FakePersonalizationAppUiDelegate
    : public ash::PersonalizationAppUiDelegate {
 public:
  explicit FakePersonalizationAppUiDelegate(content::WebUI* web_ui);

  FakePersonalizationAppUiDelegate(const FakePersonalizationAppUiDelegate&) =
      delete;
  FakePersonalizationAppUiDelegate& operator=(
      const FakePersonalizationAppUiDelegate&) = delete;

  ~FakePersonalizationAppUiDelegate() override;

  // PersonalizationAppUIDelegate:
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::WallpaperProvider>
          receiver) override;

  void MakeTransparent() override {}

  void FetchCollections(FetchCollectionsCallback callback) override;

  void FetchImagesForCollection(
      const std::string& collection_id,
      FetchImagesForCollectionCallback callback) override;

  void FetchGooglePhotosCount(FetchGooglePhotosCountCallback callback) override;

  void GetLocalImages(GetLocalImagesCallback callback) override;

  void GetLocalImageThumbnail(const base::FilePath& path,
                              GetLocalImageThumbnailCallback callback) override;

  void SetWallpaperObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
          observer) override;

  void SelectWallpaper(uint64_t image_asset_id,
                       bool preview_mode,
                       SelectWallpaperCallback callback) override;

  void SelectLocalImage(const base::FilePath& path,
                        ash::WallpaperLayout layout,
                        bool preview_mode,
                        SelectLocalImageCallback callback) override;

  void SetCustomWallpaperLayout(ash::WallpaperLayout layout) override;

  void SetDailyRefreshCollectionId(const std::string& collection_id) override;

  void GetDailyRefreshCollectionId(
      GetDailyRefreshCollectionIdCallback callback) override;

  void UpdateDailyRefreshWallpaper(
      UpdateDailyRefreshWallpaperCallback callback) override;

  void IsInTabletMode(IsInTabletModeCallback callback) override;

  void ConfirmPreviewWallpaper() override;

  void CancelPreviewWallpaper() override;

 private:
  mojo::Receiver<ash::personalization_app::mojom::WallpaperProvider>
      wallpaper_receiver_{this};
};

#endif  // ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_UI_DELEGATE_H_
