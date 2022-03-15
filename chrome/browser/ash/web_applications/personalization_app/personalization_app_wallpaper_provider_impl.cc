// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_wallpaper_provider_impl.h"

#include <stdint.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/wallpaper/wallpaper_enumerator.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chromeos/ui/base/window_properties.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace {

using ash::WallpaperController;
using ash::personalization_app::GetAccountId;
using ash::personalization_app::GetUser;

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

// Return the online wallpaper key. Use |info.asset_id| if available so we might
// be able to fallback to the cached attribution.
const std::string GetOnlineWallpaperKey(ash::WallpaperInfo info) {
  return info.asset_id.has_value()
             ? base::NumberToString(info.asset_id.value())
             : base::UnguessableToken::Create().ToString();
}

}  // namespace

PersonalizationAppWallpaperProviderImpl::
    PersonalizationAppWallpaperProviderImpl(content::WebUI* web_ui)
    : web_ui_(web_ui), profile_(Profile::FromWebUI(web_ui)) {
  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
}

PersonalizationAppWallpaperProviderImpl::
    ~PersonalizationAppWallpaperProviderImpl() = default;

void PersonalizationAppWallpaperProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::WallpaperProvider>
        receiver) {
  wallpaper_receiver_.reset();
  wallpaper_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppWallpaperProviderImpl::MakeTransparent() {
  auto* web_contents = web_ui_->GetWebContents();

  // Disable the window backdrop that creates an opaque layer in tablet mode.
  auto* window_backdrop =
      ash::WindowBackdrop::Get(web_contents->GetTopLevelNativeWindow());
  window_backdrop->SetBackdropMode(
      ash::WindowBackdrop::BackdropMode::kDisabled);

  // Set transparency on the top level native window and tell the WM not to
  // change it when window state changes.
  aura::Window* top_level_window = web_contents->GetTopLevelNativeWindow();
  top_level_window->SetProperty(::chromeos::kWindowManagerManagesOpacityKey,
                                false);
  top_level_window->SetTransparent(true);

  // Set the background color to transparent.
  web_contents->GetRenderWidgetHostView()->SetBackgroundColor(
      SK_ColorTRANSPARENT);

  // Set a background color override.
  static_cast<ContentsWebView*>(BrowserView::GetBrowserViewForNativeWindow(
                                    web_contents->GetTopLevelNativeWindow())
                                    ->contents_web_view())
      ->SetBackgroundColorOverride(SK_ColorTRANSPARENT);
}

void PersonalizationAppWallpaperProviderImpl::FetchCollections(
    FetchCollectionsCallback callback) {
  pending_collections_callbacks_.push_back(std::move(callback));
  if (wallpaper_collection_info_fetcher_) {
    // Collection fetching already started. No need to start a second time.
    return;
  }

  wallpaper_collection_info_fetcher_ =
      std::make_unique<wallpaper_handlers::BackdropCollectionInfoFetcher>();

  // base::Unretained is safe to use because |this| outlives
  // |wallpaper_collection_info_fetcher_|.
  wallpaper_collection_info_fetcher_->Start(base::BindOnce(
      &PersonalizationAppWallpaperProviderImpl::OnFetchCollections,
      base::Unretained(this)));
}

void PersonalizationAppWallpaperProviderImpl::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  auto wallpaper_images_info_fetcher =
      std::make_unique<wallpaper_handlers::BackdropImageInfoFetcher>(
          collection_id);

  auto* wallpaper_images_info_fetcher_ptr = wallpaper_images_info_fetcher.get();
  wallpaper_images_info_fetcher_ptr->Start(base::BindOnce(
      &PersonalizationAppWallpaperProviderImpl::OnFetchCollectionImages,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      std::move(wallpaper_images_info_fetcher)));
}

void PersonalizationAppWallpaperProviderImpl::FetchGooglePhotosAlbums(
    const absl::optional<std::string>& resume_token,
    FetchGooglePhotosAlbumsCallback callback) {
  if (!ash::features::IsWallpaperGooglePhotosIntegrationEnabled()) {
    mojo::ReportBadMessage(
        "Cannot call `FetchGooglePhotosAlbums()` without Google Photos "
        "Wallpaper integration enabled.");
    std::move(callback).Run(
        ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse::New(
            absl::nullopt, absl::nullopt));
    return;
  }

  if (!google_photos_albums_fetcher_) {
    google_photos_albums_fetcher_ =
        std::make_unique<wallpaper_handlers::GooglePhotosAlbumsFetcher>(
            profile_);
  }
  google_photos_albums_fetcher_->AddRequestAndStartIfNecessary(
      resume_token, std::move(callback));
}

void PersonalizationAppWallpaperProviderImpl::FetchGooglePhotosCount(
    FetchGooglePhotosCountCallback callback) {
  if (!ash::features::IsWallpaperGooglePhotosIntegrationEnabled()) {
    mojo::ReportBadMessage(
        "Cannot call `FetchGooglePhotosCount()` without Google Photos "
        "Wallpaper integration enabled.");
    std::move(callback).Run(-1);
    return;
  }

  if (!google_photos_count_fetcher_) {
    google_photos_count_fetcher_ =
        std::make_unique<wallpaper_handlers::GooglePhotosCountFetcher>(
            profile_);
  }
  google_photos_count_fetcher_->AddRequestAndStartIfNecessary(
      std::move(callback));
}

void PersonalizationAppWallpaperProviderImpl::FetchGooglePhotosEnabled(
    FetchGooglePhotosEnabledCallback callback) {
  if (!ash::features::IsWallpaperGooglePhotosIntegrationEnabled()) {
    mojo::ReportBadMessage(
        "Cannot call `FetchGooglePhotosEnabled()` without Google Photos "
        "Wallpaper integration enabled.");
    std::move(callback).Run(
        ash::personalization_app::mojom::GooglePhotosEnablementState::kError);
    return;
  }

  if (!google_photos_enabled_fetcher_) {
    google_photos_enabled_fetcher_ =
        std::make_unique<wallpaper_handlers::GooglePhotosEnabledFetcher>(
            profile_);
  }
  google_photos_enabled_fetcher_->AddRequestAndStartIfNecessary(
      std::move(callback));
}

void PersonalizationAppWallpaperProviderImpl::FetchGooglePhotosPhotos(
    const absl::optional<std::string>& item_id,
    const absl::optional<std::string>& album_id,
    const absl::optional<std::string>& resume_token,
    FetchGooglePhotosPhotosCallback callback) {
  if (!ash::features::IsWallpaperGooglePhotosIntegrationEnabled()) {
    mojo::ReportBadMessage(
        "Cannot call `FetchGooglePhotosPhotos()` without Google Photos "
        "Wallpaper integration enabled.");
    std::move(callback).Run(
        ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse::New(
            absl::nullopt, absl::nullopt));
    return;
  }

  if (!google_photos_photos_fetcher_) {
    google_photos_photos_fetcher_ =
        std::make_unique<wallpaper_handlers::GooglePhotosPhotosFetcher>(
            profile_);
  }
  google_photos_photos_fetcher_->AddRequestAndStartIfNecessary(
      item_id, album_id, resume_token, std::move(callback));
}

void PersonalizationAppWallpaperProviderImpl::GetLocalImages(
    GetLocalImagesCallback callback) {
  // TODO(b/190062481) also load images from android files.
  ash::EnumerateLocalWallpaperFiles(
      profile_,
      base::BindOnce(&PersonalizationAppWallpaperProviderImpl::OnGetLocalImages,
                     backend_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void PersonalizationAppWallpaperProviderImpl::GetLocalImageThumbnail(
    const base::FilePath& path,
    GetLocalImageThumbnailCallback callback) {
  if (local_images_.count(path) == 0) {
    mojo::ReportBadMessage("Invalid local image path received");
    return;
  }
  if (!thumbnail_loader_)
    thumbnail_loader_ = std::make_unique<ash::ThumbnailLoader>(profile_);

  ash::ThumbnailLoader::ThumbnailRequest request(
      path,
      gfx::Size(kLocalImageThumbnailSizeDip, kLocalImageThumbnailSizeDip));

  thumbnail_loader_->Load(
      request,
      base::BindOnce(
          &PersonalizationAppWallpaperProviderImpl::OnGetLocalImageThumbnail,
          base::Unretained(this), std::move(callback)));
}

void PersonalizationAppWallpaperProviderImpl::SetWallpaperObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::WallpaperObserver>
        observer) {
  // May already be bound if user refreshes page.
  wallpaper_observer_remote_.reset();
  wallpaper_observer_remote_.Bind(std::move(observer));
  if (!wallpaper_controller_observer_.IsObserving())
    wallpaper_controller_observer_.Observe(ash::WallpaperController::Get());
  // Call it once to send the first wallpaper.
  OnWallpaperChanged();
}

void PersonalizationAppWallpaperProviderImpl::OnWallpaperChanged() {
  wallpaper_attribution_info_fetcher_.reset();
  attribution_weak_ptr_factory_.InvalidateWeakPtrs();

  auto* controller = WallpaperController::Get();
  auto* client = WallpaperControllerClientImpl::Get();

  ash::WallpaperInfo info = client->GetActiveUserWallpaperInfo();
  const gfx::ImageSkia& current_wallpaper = controller->GetWallpaperImage();
  const gfx::ImageSkia& current_wallpaper_resized =
      GetResizedImage(current_wallpaper);
  const GURL& wallpaper_data_url =
      GURL(webui::GetBitmapDataUrl(*current_wallpaper_resized.bitmap()));

  switch (info.type) {
    case ash::WallpaperType::kDaily:
    case ash::WallpaperType::kOnline: {
      if (info.collection_id.empty() || !info.asset_id.has_value()) {
        DVLOG(2) << "no collection_id or asset_id found";
        // Older versions of ChromeOS do not store these information, need to
        // look up all collections and match URL.
        FetchCollections(base::BindOnce(
            &PersonalizationAppWallpaperProviderImpl::FindAttribution,
            attribution_weak_ptr_factory_.GetWeakPtr(), info,
            wallpaper_data_url));
        return;
      }

      backdrop::Collection collection;
      collection.set_collection_id(info.collection_id);
      FindAttribution(info, wallpaper_data_url,
                      std::vector<backdrop::Collection>{collection});
      return;
    }
    case ash::WallpaperType::kCustomized: {
      base::FilePath file_name = base::FilePath(info.location).BaseName();

      // Match selected wallpaper based on full filename including extension.
      const std::string& key = file_name.value();
      // Do not show file extension in user-visible selected details text.
      std::vector<std::string> attribution = {
          file_name.RemoveExtension().value()};

      NotifyWallpaperChanged(
          ash::personalization_app::mojom::CurrentWallpaper::New(
              wallpaper_data_url, std::move(attribution), info.layout,
              info.type, key));

      return;
    }
    case ash::WallpaperType::kGooglePhotos:
      client->FetchGooglePhotosPhoto(
          GetAccountId(profile_), info.location,
          base::BindOnce(&PersonalizationAppWallpaperProviderImpl::
                             SendGooglePhotosAttribution,
                         weak_ptr_factory_.GetWeakPtr(), info,
                         wallpaper_data_url));
      return;
    case ash::WallpaperType::kDefault:
    case ash::WallpaperType::kDevice:
    case ash::WallpaperType::kOneShot:
    case ash::WallpaperType::kPolicy:
    case ash::WallpaperType::kThirdParty:
      NotifyWallpaperChanged(
          ash::personalization_app::mojom::CurrentWallpaper::New(
              wallpaper_data_url, /*attribution=*/std::vector<std::string>(),
              info.layout, info.type,
              /*key=*/base::UnguessableToken::Create().ToString()));
      return;
    case ash::WallpaperType::kCount:
      mojo::ReportBadMessage("Impossible WallpaperType received");
      return;
  }
}

void PersonalizationAppWallpaperProviderImpl::OnWallpaperPreviewEnded() {
  PersonalizationAppWallpaperProviderImpl::OnWallpaperChanged();
}

void PersonalizationAppWallpaperProviderImpl::SelectWallpaper(
    uint64_t image_asset_id,
    bool preview_mode,
    SelectWallpaperCallback callback) {
  const auto& it = image_asset_id_map_.find(image_asset_id);

  if (it == image_asset_id_map_.end()) {
    mojo::ReportBadMessage("Invalid image asset_id selected");
    return;
  }

  std::vector<ash::OnlineWallpaperVariant> variants;
  for (const auto& entry : image_asset_id_map_) {
    const ImageInfo& image_info = entry.second;
    if (image_info.unit_id == it->second.unit_id &&
        image_info.collection_id == it->second.collection_id) {
      variants.emplace_back(image_info.asset_id, image_info.image_url,
                            image_info.type);
    }
  }

  WallpaperControllerClientImpl* client = WallpaperControllerClientImpl::Get();
  DCHECK(client);

  if (pending_select_wallpaper_callback_)
    std::move(pending_select_wallpaper_callback_).Run(/*success=*/false);
  pending_select_wallpaper_callback_ = std::move(callback);

  SetMinimizedWindowStateForPreview(preview_mode);

  client->RecordWallpaperSourceUMA(ash::WallpaperType::kOnline);

  client->SetOnlineWallpaper(
      ash::OnlineWallpaperParams(
          GetAccountId(profile_), absl::make_optional(image_asset_id),
          GURL(it->second.image_url.spec()), it->second.collection_id,
          ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED, preview_mode,
          /*from_user=*/true,
          /*daily_refresh_enabled=*/false, it->second.unit_id, variants),
      base::BindOnce(
          &PersonalizationAppWallpaperProviderImpl::OnOnlineWallpaperSelected,
          backend_weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppWallpaperProviderImpl::SelectLocalImage(
    const base::FilePath& path,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SelectLocalImageCallback callback) {
  if (local_images_.count(path) == 0) {
    mojo::ReportBadMessage("Invalid local image path selected");
    return;
  }
  if (pending_select_local_image_callback_)
    std::move(pending_select_local_image_callback_).Run(/*success=*/false);
  pending_select_local_image_callback_ = std::move(callback);

  SetMinimizedWindowStateForPreview(preview_mode);

  WallpaperControllerClientImpl::Get()->RecordWallpaperSourceUMA(
      ash::WallpaperType::kCustomized);

  WallpaperController::Get()->SetCustomWallpaper(
      GetAccountId(profile_), path, layout, preview_mode,
      base::BindOnce(
          &PersonalizationAppWallpaperProviderImpl::OnLocalImageSelected,
          backend_weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppWallpaperProviderImpl::SelectGooglePhotosPhoto(
    const std::string& id,
    ash::WallpaperLayout layout,
    SelectGooglePhotosPhotoCallback callback) {
  if (pending_select_google_photos_photo_callback_)
    std::move(pending_select_google_photos_photo_callback_).Run(false);
  pending_select_google_photos_photo_callback_ = std::move(callback);
  WallpaperControllerClientImpl* client = WallpaperControllerClientImpl::Get();
  DCHECK(client);

  client->RecordWallpaperSourceUMA(ash::WallpaperType::kGooglePhotos);

  client->SetGooglePhotosWallpaper(
      ash::GooglePhotosWallpaperParams(GetAccountId(profile_), id, layout),
      base::BindOnce(&PersonalizationAppWallpaperProviderImpl::
                         OnGooglePhotosWallpaperSelected,
                     backend_weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppWallpaperProviderImpl::SetCurrentWallpaperLayout(
    ash::WallpaperLayout layout) {
  WallpaperController::Get()->UpdateCurrentWallpaperLayout(
      GetAccountId(profile_), layout);
}

void PersonalizationAppWallpaperProviderImpl::SetDailyRefreshCollectionId(
    const std::string& collection_id) {
  WallpaperController::Get()->SetDailyRefreshCollectionId(
      GetAccountId(profile_), collection_id);
}

void PersonalizationAppWallpaperProviderImpl::GetDailyRefreshCollectionId(
    GetDailyRefreshCollectionIdCallback callback) {
  auto* controller = WallpaperController::Get();
  std::move(callback).Run(
      controller->GetDailyRefreshCollectionId(GetAccountId(profile_)));
}

void PersonalizationAppWallpaperProviderImpl::UpdateDailyRefreshWallpaper(
    UpdateDailyRefreshWallpaperCallback callback) {
  if (pending_update_daily_refresh_wallpaper_callback_)
    std::move(pending_update_daily_refresh_wallpaper_callback_)
        .Run(/*success=*/false);
  pending_update_daily_refresh_wallpaper_callback_ = std::move(callback);

  WallpaperControllerClientImpl::Get()->RecordWallpaperSourceUMA(
      ash::WallpaperType::kDaily);

  WallpaperController::Get()->UpdateDailyRefreshWallpaper(base::BindOnce(
      &PersonalizationAppWallpaperProviderImpl::OnDailyRefreshWallpaperUpdated,
      backend_weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppWallpaperProviderImpl::IsInTabletMode(
    IsInTabletModeCallback callback) {
  std::move(callback).Run(ash::TabletMode::IsInTabletMode());
}

void PersonalizationAppWallpaperProviderImpl::ConfirmPreviewWallpaper() {
  SetMinimizedWindowStateForPreview(/*preview_mode=*/false);
  WallpaperController::Get()->ConfirmPreviewWallpaper();
}

void PersonalizationAppWallpaperProviderImpl::CancelPreviewWallpaper() {
  SetMinimizedWindowStateForPreview(/*preview_mode=*/false);
  WallpaperController::Get()->CancelPreviewWallpaper();
}

wallpaper_handlers::GooglePhotosAlbumsFetcher*
PersonalizationAppWallpaperProviderImpl::SetGooglePhotosAlbumsFetcherForTest(
    std::unique_ptr<wallpaper_handlers::GooglePhotosAlbumsFetcher> fetcher) {
  google_photos_albums_fetcher_ = std::move(fetcher);
  return google_photos_albums_fetcher_.get();
}

wallpaper_handlers::GooglePhotosCountFetcher*
PersonalizationAppWallpaperProviderImpl::SetGooglePhotosCountFetcherForTest(
    std::unique_ptr<wallpaper_handlers::GooglePhotosCountFetcher> fetcher) {
  google_photos_count_fetcher_ = std::move(fetcher);
  return google_photos_count_fetcher_.get();
}

wallpaper_handlers::GooglePhotosEnabledFetcher*
PersonalizationAppWallpaperProviderImpl::SetGooglePhotosEnabledFetcherForTest(
    std::unique_ptr<wallpaper_handlers::GooglePhotosEnabledFetcher> fetcher) {
  google_photos_enabled_fetcher_ = std::move(fetcher);
  return google_photos_enabled_fetcher_.get();
}

wallpaper_handlers::GooglePhotosPhotosFetcher*
PersonalizationAppWallpaperProviderImpl::SetGooglePhotosPhotosFetcherForTest(
    std::unique_ptr<wallpaper_handlers::GooglePhotosPhotosFetcher> fetcher) {
  google_photos_photos_fetcher_ = std::move(fetcher);
  return google_photos_photos_fetcher_.get();
}

void PersonalizationAppWallpaperProviderImpl::OnFetchCollections(
    bool success,
    const std::vector<backdrop::Collection>& collections) {
  DCHECK(wallpaper_collection_info_fetcher_);
  DCHECK(!pending_collections_callbacks_.empty());

  absl::optional<std::vector<backdrop::Collection>> result;
  if (success && !collections.empty()) {
    result = std::move(collections);
  }

  for (auto& callback : pending_collections_callbacks_) {
    std::move(callback).Run(result);
  }
  pending_collections_callbacks_.clear();
  wallpaper_collection_info_fetcher_.reset();
}

void PersonalizationAppWallpaperProviderImpl::OnFetchCollectionImages(
    FetchImagesForCollectionCallback callback,
    std::unique_ptr<wallpaper_handlers::BackdropImageInfoFetcher> fetcher,
    bool success,
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  absl::optional<std::vector<backdrop::Image>> result;
  if (success && !images.empty()) {
    for (const auto& proto_image : images) {
      if (!proto_image.has_asset_id() || !proto_image.has_image_url()) {
        LOG(WARNING) << "Invalid image discarded";
        continue;
      }
      image_asset_id_map_.insert(
          {proto_image.asset_id(),
           ImageInfo(GURL(proto_image.image_url()), collection_id,
                     proto_image.asset_id(), proto_image.unit_id(),
                     proto_image.has_image_type()
                         ? proto_image.image_type()
                         : backdrop::Image::IMAGE_TYPE_UNKNOWN)});
    }
    result = std::move(images);
  }
  std::move(callback).Run(std::move(result));
}

void PersonalizationAppWallpaperProviderImpl::OnGetLocalImages(
    GetLocalImagesCallback callback,
    const std::vector<base::FilePath>& images) {
  local_images_ = std::set<base::FilePath>(images.begin(), images.end());
  std::move(callback).Run(images);
}

void PersonalizationAppWallpaperProviderImpl::OnGetLocalImageThumbnail(
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

void PersonalizationAppWallpaperProviderImpl::OnOnlineWallpaperSelected(
    bool success) {
  DCHECK(pending_select_wallpaper_callback_);
  std::move(pending_select_wallpaper_callback_).Run(success);
}

void PersonalizationAppWallpaperProviderImpl::OnGooglePhotosWallpaperSelected(
    bool success) {
  DCHECK(pending_select_google_photos_photo_callback_);
  std::move(pending_select_google_photos_photo_callback_).Run(success);
}

void PersonalizationAppWallpaperProviderImpl::OnLocalImageSelected(
    bool success) {
  DCHECK(pending_select_local_image_callback_);
  std::move(pending_select_local_image_callback_).Run(success);
}

void PersonalizationAppWallpaperProviderImpl::OnDailyRefreshWallpaperUpdated(
    bool success) {
  DCHECK(pending_update_daily_refresh_wallpaper_callback_);
  std::move(pending_update_daily_refresh_wallpaper_callback_).Run(success);
}

void PersonalizationAppWallpaperProviderImpl::FindAttribution(
    const ash::WallpaperInfo& info,
    const GURL& wallpaper_data_url,
    const absl::optional<std::vector<backdrop::Collection>>& collections) {
  DCHECK(!wallpaper_attribution_info_fetcher_);
  if (!collections.has_value() || collections->empty()) {
    NotifyWallpaperChanged(
        ash::personalization_app::mojom::CurrentWallpaper::New(
            wallpaper_data_url, /*attribution=*/std::vector<std::string>(),
            info.layout, info.type, GetOnlineWallpaperKey(info)));

    return;
  }

  std::size_t current_index = 0;
  wallpaper_attribution_info_fetcher_ =
      std::make_unique<wallpaper_handlers::BackdropImageInfoFetcher>(
          collections->at(current_index).collection_id());

  wallpaper_attribution_info_fetcher_->Start(base::BindOnce(
      &PersonalizationAppWallpaperProviderImpl::FindAttributionInCollection,
      attribution_weak_ptr_factory_.GetWeakPtr(), info, wallpaper_data_url,
      current_index, collections));
}

void PersonalizationAppWallpaperProviderImpl::FindAttributionInCollection(
    const ash::WallpaperInfo& info,
    const GURL& wallpaper_data_url,
    std::size_t current_index,
    const absl::optional<std::vector<backdrop::Collection>>& collections,
    bool success,
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  DCHECK(wallpaper_attribution_info_fetcher_);

  const backdrop::Image* backend_image = nullptr;
  if (success && !images.empty()) {
    for (const auto& proto_image : images) {
      if (!proto_image.has_image_url() || !proto_image.has_asset_id())
        break;
      bool is_same_asset_id = info.asset_id.has_value() &&
                              proto_image.asset_id() == info.asset_id.value();
      bool is_same_url = info.location.rfind(proto_image.image_url(), 0) == 0;
      if (is_same_asset_id || is_same_url) {
        backend_image = &proto_image;
        break;
      }
    }
  }

  if (backend_image) {
    std::vector<std::string> attributions;
    for (const auto& attr : backend_image->attribution())
      attributions.push_back(attr.text());
    NotifyWallpaperChanged(
        ash::personalization_app::mojom::CurrentWallpaper::New(
            wallpaper_data_url, attributions, info.layout, info.type,
            /*key=*/base::NumberToString(backend_image->asset_id())));
    wallpaper_attribution_info_fetcher_.reset();
    return;
  }

  ++current_index;

  if (current_index >= collections->size()) {
    NotifyWallpaperChanged(
        ash::personalization_app::mojom::CurrentWallpaper::New(
            wallpaper_data_url, /*attribution=*/std::vector<std::string>(),
            info.layout, info.type, GetOnlineWallpaperKey(info)));
    wallpaper_attribution_info_fetcher_.reset();
    return;
  }

  auto fetcher = std::make_unique<wallpaper_handlers::BackdropImageInfoFetcher>(
      collections->at(current_index).collection_id());
  fetcher->Start(base::BindOnce(
      &PersonalizationAppWallpaperProviderImpl::FindAttributionInCollection,
      attribution_weak_ptr_factory_.GetWeakPtr(), info, wallpaper_data_url,
      current_index, collections));
  // resetting the previous fetcher last because the current method is bound
  // to a callback owned by the previous fetcher.
  wallpaper_attribution_info_fetcher_ = std::move(fetcher);
}

void PersonalizationAppWallpaperProviderImpl::SendGooglePhotosAttribution(
    const ash::WallpaperInfo& info,
    const GURL& wallpaper_data_url,
    mojo::StructPtr<ash::personalization_app::mojom::GooglePhotosPhoto> photo) {
  std::vector<std::string> attribution;
  if (!photo.is_null())
    attribution.push_back(photo->name);
  NotifyWallpaperChanged(ash::personalization_app::mojom::CurrentWallpaper::New(
      wallpaper_data_url, attribution, info.layout, info.type,
      /*key=*/info.location));
}

void PersonalizationAppWallpaperProviderImpl::SetMinimizedWindowStateForPreview(
    bool preview_mode) {
  auto* wallpaper_controller = WallpaperController::Get();
  const std::string& user_id_hash = GetUser(profile_)->username_hash();
  if (preview_mode)
    wallpaper_controller->MinimizeInactiveWindows(user_id_hash);
  else
    wallpaper_controller->RestoreMinimizedWindows(user_id_hash);
}

void PersonalizationAppWallpaperProviderImpl::NotifyWallpaperChanged(
    ash::personalization_app::mojom::CurrentWallpaperPtr current_wallpaper) {
  DCHECK(wallpaper_observer_remote_.is_bound());
  wallpaper_observer_remote_->OnWallpaperChanged(std::move(current_wallpaper));
}
