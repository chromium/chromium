// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/fake_personalization_app_wallpaper_provider.h"

#include <stdint.h>
#include <vector>

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/check_op.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::personalization_app {

namespace {
const char kFakeCollectionId[] = "fake_collection_id";
}  // namespace

FakePersonalizationAppWallpaperProvider::
    FakePersonalizationAppWallpaperProvider(content::WebUI* web_ui) {}

FakePersonalizationAppWallpaperProvider::
    ~FakePersonalizationAppWallpaperProvider() = default;

void FakePersonalizationAppWallpaperProvider::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::WallpaperProvider>
        receiver) {
  wallpaper_receiver_.reset();
  wallpaper_receiver_.Bind(std::move(receiver));
}

void FakePersonalizationAppWallpaperProvider::GetWallpaperAsJpegBytes(
    content::WebUIDataSource::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedBytes>());
}

bool FakePersonalizationAppWallpaperProvider::IsEligibleForGooglePhotos() {
  return true;
}

void FakePersonalizationAppWallpaperProvider::FetchCollections(
    FetchCollectionsCallback callback) {
  std::vector<backdrop::Collection> collections;
  backdrop::Collection collection;
  collection.set_collection_id(kFakeCollectionId);
  collection.set_collection_name("Test Collection");
  backdrop::Image* image = collection.add_preview();
  image->set_asset_id(1);
  image->set_image_url(std::string());
  collections.push_back(collection);
  std::move(callback).Run(std::move(collections));
}

void FakePersonalizationAppWallpaperProvider::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  DCHECK_EQ(collection_id, kFakeCollectionId);
  std::vector<backdrop::Image> images;
  backdrop::Image image;
  image.set_asset_id(1);
  image.set_image_url("about:blank");
  image.add_attribution()->set_text("test");
  image.set_unit_id(3);
  image.set_image_type(backdrop::Image_ImageType_IMAGE_TYPE_UNKNOWN);
  images.push_back(image);
  std::move(callback).Run(std::move(images));
}

void FakePersonalizationAppWallpaperProvider::FetchGooglePhotosAlbums(
    const absl::optional<std::string>& resume_token,
    FetchGooglePhotosAlbumsCallback callback) {
  std::move(callback).Run(
      ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse::New());
}

void FakePersonalizationAppWallpaperProvider::FetchGooglePhotosEnabled(
    FetchGooglePhotosEnabledCallback callback) {
  std::move(callback).Run(
      ash::personalization_app::mojom::GooglePhotosEnablementState::kEnabled);
}

void FakePersonalizationAppWallpaperProvider::FetchGooglePhotosPhotos(
    const absl::optional<std::string>& item_id,
    const absl::optional<std::string>& album_id,
    const absl::optional<std::string>& resume_token,
    FetchGooglePhotosPhotosCallback callback) {
  std::move(callback).Run(
      ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse::New());
}

void FakePersonalizationAppWallpaperProvider::GetDefaultImageThumbnail(
    GetDefaultImageThumbnailCallback callback) {
  std::move(callback).Run(GURL());
}

void FakePersonalizationAppWallpaperProvider::GetLocalImages(
    GetLocalImagesCallback callback) {
  std::move(callback).Run({});
}

void FakePersonalizationAppWallpaperProvider::GetLocalImageThumbnail(
    const base::FilePath& path,
    GetLocalImageThumbnailCallback callback) {
  std::move(callback).Run(GURL());
}

void FakePersonalizationAppWallpaperProvider::SetWallpaperObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
        observer) {}

void FakePersonalizationAppWallpaperProvider::SelectWallpaper(
    uint64_t image_asset_id,
    bool preview_mode,
    SelectWallpaperCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void FakePersonalizationAppWallpaperProvider::SelectDefaultImage(
    SelectDefaultImageCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void FakePersonalizationAppWallpaperProvider::SelectGooglePhotosPhoto(
    const std::string& id,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SelectGooglePhotosPhotoCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void FakePersonalizationAppWallpaperProvider::SelectGooglePhotosAlbum(
    const std::string& id,
    SelectGooglePhotosAlbumCallback callback) {
  std::move(callback).Run(mojom::SetDailyRefreshResponse::New(
      /*success=*/false, /*force_refresh=*/false));
}

void FakePersonalizationAppWallpaperProvider::
    GetGooglePhotosDailyRefreshAlbumId(
        GetGooglePhotosDailyRefreshAlbumIdCallback callback) {
  std::move(callback).Run("");
}

void FakePersonalizationAppWallpaperProvider::SelectLocalImage(
    const base::FilePath& path,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SelectLocalImageCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void FakePersonalizationAppWallpaperProvider::SetCurrentWallpaperLayout(
    ash::WallpaperLayout layout) {
  return;
}

void FakePersonalizationAppWallpaperProvider::SetDailyRefreshCollectionId(
    const std::string& collection_id,
    SetDailyRefreshCollectionIdCallback callback) {
  return;
}

void FakePersonalizationAppWallpaperProvider::GetDailyRefreshCollectionId(
    GetDailyRefreshCollectionIdCallback callback) {
  std::move(callback).Run(kFakeCollectionId);
}

void FakePersonalizationAppWallpaperProvider::UpdateDailyRefreshWallpaper(
    UpdateDailyRefreshWallpaperCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void FakePersonalizationAppWallpaperProvider::IsInTabletMode(
    IsInTabletModeCallback callback) {
  std::move(callback).Run(/*tablet_mode=*/false);
}

void FakePersonalizationAppWallpaperProvider::ConfirmPreviewWallpaper() {
  return;
}

void FakePersonalizationAppWallpaperProvider::CancelPreviewWallpaper() {
  return;
}

}  // namespace ash::personalization_app
