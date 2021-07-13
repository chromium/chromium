// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/chrome_personalization_app_ui_delegate.h"

#include <stdint.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/backdrop_wallpaper_handlers/backdrop_wallpaper.pb.h"
#include "chrome/browser/ash/backdrop_wallpaper_handlers/backdrop_wallpaper_handlers.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/wallpaper/wallpaper_enumerator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chromeos/components/personalization_app/mojom/personalization_app.mojom.h"
#include "chromeos/components/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace {
constexpr int kLocalImageThumbnailSizeDip = 256;

const gfx::ImageSkia GetResizedImage(const gfx::ImageSkia& image) {
  // Resize the image maintaining our aspect ratio.
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = kLocalImageThumbnailSizeDip;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > kLocalImageThumbnailSizeDip) {
    width = kLocalImageThumbnailSizeDip;
    height = static_cast<int>(width / aspect_ratio);
  }
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size(width, height));
}

}  // namespace

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

template <>
struct TypeConverter<chromeos::personalization_app::mojom::LocalImagePtr,
                     base::FilePath> {
  static chromeos::personalization_app::mojom::LocalImagePtr Convert(
      const base::FilePath& path) {
    auto token = base::UnguessableToken::Create();
    std::string name = path.BaseName().RemoveExtension().value();
    return chromeos::personalization_app::mojom::LocalImage::New(token, name);
  }
};

}  // namespace mojo

ChromePersonalizationAppUiDelegate::ChromePersonalizationAppUiDelegate(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {
  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
}

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

void ChromePersonalizationAppUiDelegate::GetLocalImages(
    GetLocalImagesCallback callback) {
  // TODO(b/190062481) also load images from android files.
  ash::EnumerateLocalWallpaperFiles(
      profile_,
      base::BindOnce(&ChromePersonalizationAppUiDelegate::OnGetLocalImages,
                     backend_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ChromePersonalizationAppUiDelegate::GetLocalImageThumbnail(
    const base::UnguessableToken& id,
    GetLocalImageThumbnailCallback callback) {
  const auto& entry = local_image_id_map_.find(id);
  if (entry == local_image_id_map_.end()) {
    mojo::ReportBadMessage("Invalid local image id received");
    return;
  }
  const base::FilePath& file_path = entry->second;

  if (!thumbnail_loader_)
    thumbnail_loader_ = std::make_unique<ash::ThumbnailLoader>(profile_);

  ash::ThumbnailLoader::ThumbnailRequest request(
      file_path,
      gfx::Size(kLocalImageThumbnailSizeDip, kLocalImageThumbnailSizeDip));

  thumbnail_loader_->Load(
      request,
      base::BindOnce(
          &ChromePersonalizationAppUiDelegate::OnGetLocalImageThumbnail,
          base::Unretained(this), std::move(callback)));
}

void ChromePersonalizationAppUiDelegate::GetCurrentWallpaper(
    GetCurrentWallpaperCallback callback) {
  auto* controller = ash::WallpaperController::Get();
  auto* client = WallpaperControllerClientImpl::Get();

  ash::WallpaperInfo info = client->GetActiveUserWallpaperInfo();
  const gfx::ImageSkia& current_wallpaper = controller->GetWallpaperImage();
  const gfx::ImageSkia& current_wallpaper_resized =
      GetResizedImage(current_wallpaper);
  const GURL& gurl =
      GURL(webui::GetBitmapDataUrl(*current_wallpaper_resized.bitmap()));
  std::vector<std::string> attribution;

  switch (info.type) {
    case ash::WallpaperType::ONLINE: {
      if (info.collection_id.empty() || !info.asset_id.has_value())
        break;
      if (!wallpaper_attribution_info_fetcher_)
        wallpaper_attribution_info_fetcher_ =
            std::make_unique<backdrop_wallpaper_handlers::ImageInfoFetcher>(
                info.collection_id);

      wallpaper_attribution_info_fetcher_->Start(base::BindOnce(
          &ChromePersonalizationAppUiDelegate::OnGetOnlineImageAttribution,
          backend_weak_ptr_factory_.GetWeakPtr(), info, gurl,
          std::move(callback)));
      return;
    }
    case ash::WallpaperType::CUSTOMIZED:
      attribution.push_back(
          base::FilePath(info.location).BaseName().RemoveExtension().value());
      break;
    case ash::WallpaperType::DAILY:
    case ash::WallpaperType::DEFAULT:
    case ash::WallpaperType::DEVICE:
    case ash::WallpaperType::ONE_SHOT:
    case ash::WallpaperType::POLICY:
    case ash::WallpaperType::THIRDPARTY:
      break;
    case ash::WallpaperType::WALLPAPER_TYPE_COUNT:
      mojo::ReportBadMessage("Impossible WallpaperType received");
      return;
  }
  std::move(callback).Run(
      chromeos::personalization_app::mojom::CurrentWallpaper::New(
          gurl, attribution, info.layout, info.type));
}

void ChromePersonalizationAppUiDelegate::SelectWallpaper(
    uint64_t image_asset_id,
    SelectWallpaperCallback callback) {
  const auto& it = image_asset_id_map_.find(image_asset_id);

  if (it == image_asset_id_map_.end()) {
    mojo::ReportBadMessage("Invalid image asset_id selected");
    return;
  }

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  WallpaperControllerClientImpl* client = WallpaperControllerClientImpl::Get();
  DCHECK(client);

  client->SetOnlineWallpaper(
      ash::OnlineWallpaperParams(
          user->GetAccountId(), absl::make_optional(image_asset_id),
          GURL(it->second.image_url.spec()), it->second.collection_id,
          ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
          /*preview_mode=*/false),
      std::move(callback));
}

void ChromePersonalizationAppUiDelegate::SelectLocalImage(
    const base::UnguessableToken& id,
    SelectLocalImageCallback callback) {
  const auto& it = local_image_id_map_.find(id);

  if (it == local_image_id_map_.end()) {
    mojo::ReportBadMessage("Invalid local image id selected");
    return;
  }

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);

  auto* controller = ash::WallpaperController::Get();
  auto* client = WallpaperControllerClientImpl::Get();

  const auto& account_id = user->GetAccountId();

  controller->SetCustomWallpaper(
      account_id, client->GetFilesId(account_id), it->second,
      ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*preview_mode=*/false, std::move(callback));
}

void ChromePersonalizationAppUiDelegate::SetCustomWallpaperLayout(
    ash::WallpaperLayout layout) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  auto* controller = ash::WallpaperController::Get();

  const auto& account_id = user->GetAccountId();
  controller->UpdateCustomWallpaperLayout(account_id, layout);
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

void ChromePersonalizationAppUiDelegate::OnGetLocalImages(
    GetLocalImagesCallback callback,
    const std::vector<base::FilePath>& images) {
  local_image_id_map_.clear();
  std::vector<chromeos::personalization_app::mojom::LocalImagePtr> result;
  for (const auto& image_path : images) {
    auto local_image =
        chromeos::personalization_app::mojom::LocalImage::From<base::FilePath>(
            image_path);

    local_image_id_map_.insert({local_image->id, image_path});
    result.push_back(std::move(local_image));
  }
  std::move(callback).Run(std::move(result));
}

void ChromePersonalizationAppUiDelegate::OnGetLocalImageThumbnail(
    GetLocalImageThumbnailCallback callback,
    const SkBitmap* bitmap,
    base::File::Error error) {
  if (error != base::File::Error::FILE_OK) {
    // Do not call |mojom::ReportBadMessage| here. The message is valid, but
    // the file may be corrupt or unreadable.
    std::move(callback).Run(std::string());
    return;
  }
  std::move(callback).Run(webui::GetBitmapDataUrl(*bitmap));
}

void ChromePersonalizationAppUiDelegate::OnGetOnlineImageAttribution(
    const ash::WallpaperInfo& info,
    const GURL& gurl,
    GetCurrentWallpaperCallback callback,
    bool success,
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  DCHECK(wallpaper_attribution_info_fetcher_);

  std::vector<std::string> attribution;
  if (success && !images.empty()) {
    for (const auto& proto_image : images) {
      auto mojom_image =
          chromeos::personalization_app::mojom::WallpaperImage::From<
              backdrop::Image>(proto_image);
      if (!mojom_image.is_null() && mojom_image->asset_id == info.asset_id) {
        attribution = mojom_image->attribution;
        break;
      }
    }
  }
  std::move(callback).Run(
      chromeos::personalization_app::mojom::CurrentWallpaper::New(
          gurl, attribution, info.layout, info.type));
  wallpaper_attribution_info_fetcher_.reset();
}
