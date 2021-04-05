// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_personalization_app_ui_delegate.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/backdrop_wallpaper_handlers/backdrop_wallpaper.pb.h"
#include "chrome/browser/ash/backdrop_wallpaper_handlers/backdrop_wallpaper_handlers.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<
    chromeos::personalization_app::mojom::WallpaperCollectionPtr,
    backdrop::Collection> {
  static chromeos::personalization_app::mojom::WallpaperCollectionPtr Convert(
      const backdrop::Collection& collection) {
    return chromeos::personalization_app::mojom::WallpaperCollection::New(
        collection.collection_id(), collection.collection_name());
  }
};

template <>
struct TypeConverter<chromeos::personalization_app::mojom::WallpaperImagePtr,
                     backdrop::Image> {
  static chromeos::personalization_app::mojom::WallpaperImagePtr Convert(
      const backdrop::Image& image) {
    return chromeos::personalization_app::mojom::WallpaperImage::New(
        GURL(image.image_url()));
  }
};

}  // namespace mojo

ChromePersonalizationAppUiDelegate::ChromePersonalizationAppUiDelegate(
    content::WebUI* web_ui) {}

ChromePersonalizationAppUiDelegate::~ChromePersonalizationAppUiDelegate() =
    default;

void ChromePersonalizationAppUiDelegate::BindInterface(
    mojo::PendingReceiver<
        chromeos::personalization_app::mojom::WallpaperProvider> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
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

void ChromePersonalizationAppUiDelegate::OnFetchCollections(
    FetchCollectionsCallback callback,
    bool success,
    const std::vector<backdrop::Collection>& collections) {
  DCHECK(wallpaper_collection_info_fetcher_);

  using ResultType =
      std::vector<chromeos::personalization_app::mojom::WallpaperCollectionPtr>;

  base::Optional<ResultType> result;
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
    const std::vector<backdrop::Image>& images) {
  DCHECK(wallpaper_images_info_fetcher_);

  using ResultType =
      std::vector<chromeos::personalization_app::mojom::WallpaperImagePtr>;

  base::Optional<ResultType> result;
  if (success && !images.empty()) {
    ResultType data;
    std::transform(images.cbegin(), images.cend(), std::back_inserter(data),
                   chromeos::personalization_app::mojom::WallpaperImage::From<
                       backdrop::Image>);
    result = std::move(data);
  }
  std::move(callback).Run(std::move(result));
  wallpaper_images_info_fetcher_.reset();
}
