// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_personalization_app_ui_delegate.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper_info.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "base/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/backdrop_wallpaper_handlers/backdrop_wallpaper.pb.h"
#include "chrome/browser/ash/backdrop_wallpaper_handlers/backdrop_wallpaper_handlers.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace mojo {

template <>
struct TypeConverter<
    chromeos::personalization_app::mojom::WallpaperCollectionPtr,
    backdrop::Collection> {
  static chromeos::personalization_app::mojom::WallpaperCollectionPtr Convert(
      const backdrop::Collection& collection) {
    // All wallpaper collections are expected to have one preview image. For
    // now, continue even if it is missing.
    // TODO(b/185580965) switch to using StructTraits and reject if preview is
    // missing.
    absl::optional<GURL> preview_url;
    if (collection.preview_size() > 0)
      preview_url = GURL(collection.preview(0).image_url());

    return chromeos::personalization_app::mojom::WallpaperCollection::New(
        collection.collection_id(), collection.collection_name(), preview_url);
  }
};

template <>
struct TypeConverter<chromeos::personalization_app::mojom::WallpaperImagePtr,
                     backdrop::Image> {
  static chromeos::personalization_app::mojom::WallpaperImagePtr Convert(
      const backdrop::Image& image) {
    if (!image.has_image_url() || !image.has_asset_id())
      return nullptr;

    GURL image_url(image.image_url());
    if (!image_url.is_valid())
      return nullptr;

    std::vector<std::string> attribution;
    for (const auto& attr : image.attribution())
      attribution.push_back(attr.text());

    return chromeos::personalization_app::mojom::WallpaperImage::New(
        GURL(image.image_url()), std::move(attribution), image.asset_id());
  }
};

}  // namespace mojo

ChromePersonalizationAppUiDelegate::ChromePersonalizationAppUiDelegate(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {}

ChromePersonalizationAppUiDelegate::~ChromePersonalizationAppUiDelegate() =
    default;

void ChromePersonalizationAppUiDelegate::BindInterface(
    mojo::PendingReceiver<
        chromeos::personalization_app::mojom::WallpaperProvider> receiver) {
  wallpaper_receiver_.reset();
  wallpaper_receiver_.Bind(std::move(receiver));
}

void ChromePersonalizationAppUiDelegate::FetchCollections(
    FetchCollectionsCallback callback) {
  DCHECK(!wallpaper_collection_info_fetcher_)
      << "Only one request allowed at a time";
  wallpaper_collection_info_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::CollectionInfoFetcher>();

  // base::Unretained is safe to use because |this| outlives
  // |wallpaper_collection_info_fetcher_|.
  wallpaper_collection_info_fetcher_->Start(
      base::BindOnce(&ChromePersonalizationAppUiDelegate::OnFetchCollections,
                     base::Unretained(this), std::move(callback)));
}

void ChromePersonalizationAppUiDelegate::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  DCHECK(!wallpaper_images_info_fetcher_)
      << "Only one request allowed at a time";
  wallpaper_images_info_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::ImageInfoFetcher>(
          collection_id);

  // base::Unretained is safe to use because |this| outlives
  // |image_info_fetcher_|.
  wallpaper_images_info_fetcher_->Start(base::BindOnce(
      &ChromePersonalizationAppUiDelegate::OnFetchCollectionImages,
      base::Unretained(this), std::move(callback)));
}

void ChromePersonalizationAppUiDelegate::GetCurrentWallpaper(
    GetCurrentWallpaperCallback callback) {
  auto* client = WallpaperControllerClientImpl::Get();
  ash::WallpaperInfo info = client->GetActiveUserWallpaperInfo();

  switch (info.type) {
    case ash::WallpaperType::ONLINE: {
      GURL url(info.location);
      DCHECK(url.is_valid());

      // TODO(b/186575680) fill in actual image attribution details and
      // asset_id.
      std::move(callback).Run(
          chromeos::personalization_app::mojom::WallpaperImage::New(
              url, /*attribution=*/std::vector<std::string>(), /*asset_id=*/0));

      return;
    }
    case ash::WallpaperType::CUSTOMIZED:
    case ash::WallpaperType::DAILY:
    case ash::WallpaperType::DEFAULT:
    case ash::WallpaperType::DEVICE:
    case ash::WallpaperType::ONE_SHOT:
    case ash::WallpaperType::POLICY:
    case ash::WallpaperType::THIRDPARTY:
      NOTIMPLEMENTED();
      std::move(callback).Run(nullptr);
      return;
    case ash::WallpaperType::WALLPAPER_TYPE_COUNT:
      NOTREACHED() << "Impossible WallpaperType";
      std::move(callback).Run(nullptr);
      return;
  }
}

void ChromePersonalizationAppUiDelegate::SelectWallpaper(
    uint64_t image_asset_id,
    SelectWallpaperCallback callback) {
  const auto& it = image_asset_id_map_.find(image_asset_id);

  if (it == image_asset_id_map_.end()) {
    LOG(WARNING) << "Invalid image asset_id selected";
    std::move(callback).Run(false);
    return;
  }

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  WallpaperControllerClientImpl* client = WallpaperControllerClientImpl::Get();
  DCHECK(client);

  client->SetOnlineWallpaper(
      user->GetAccountId(),
      GURL(it->second.image_url.spec() +
           WallpaperControllerClientImpl::GetBackdropWallpaperSuffix()),
      it->second.collection_id,
      ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, std::move(callback));
}

void ChromePersonalizationAppUiDelegate::OnFetchCollections(
    FetchCollectionsCallback callback,
    bool success,
    const std::vector<backdrop::Collection>& collections) {
  DCHECK(wallpaper_collection_info_fetcher_);

  using ResultType =
      std::vector<chromeos::personalization_app::mojom::WallpaperCollectionPtr>;

  absl::optional<ResultType> result;
  if (success && !collections.empty()) {
    ResultType data;
    std::transform(
        collections.cbegin(), collections.cend(), std::back_inserter(data),
        chromeos::personalization_app::mojom::WallpaperCollection::From<
            backdrop::Collection>);

    result = std::move(data);
  }
  std::move(callback).Run(std::move(result));
  wallpaper_collection_info_fetcher_.reset();
}

void ChromePersonalizationAppUiDelegate::OnFetchCollectionImages(
    FetchImagesForCollectionCallback callback,
    bool success,
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  DCHECK(wallpaper_images_info_fetcher_);

  using ResultType =
      std::vector<chromeos::personalization_app::mojom::WallpaperImagePtr>;

  absl::optional<ResultType> result;
  if (success && !images.empty()) {
    ResultType data;
    for (const auto& proto_image : images) {
      auto mojom_image =
          chromeos::personalization_app::mojom::WallpaperImage::From<
              backdrop::Image>(proto_image);

      if (mojom_image.is_null()) {
        LOG(WARNING) << "Invalid image discarded";
        continue;
      }
      image_asset_id_map_.insert(
          {mojom_image->asset_id, {mojom_image->url, collection_id}});
      data.push_back(std::move(mojom_image));
    }
    result = std::move(data);
  }
  std::move(callback).Run(std::move(result));
  wallpaper_images_info_fetcher_.reset();
}
