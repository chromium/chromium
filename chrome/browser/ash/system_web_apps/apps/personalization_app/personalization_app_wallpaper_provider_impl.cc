// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_wallpaper_provider_impl.h"

#include <stdint.h>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_online_variant_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/base64.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper/wallpaper_enumerator.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
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
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_util.h"
#include "url/gurl.h"

namespace ash::personalization_app {

namespace {

using ash::WallpaperController;
using ash::personalization_app::GetAccountId;
using ash::personalization_app::GetUser;

constexpr int kLocalImageThumbnailSizeDip = 256;
constexpr int kCurrentWallpaperThumbnailSizeDip = 1024;

// Return the online wallpaper key. Use |info.unit_id| if available so we might
// be able to fallback to the cached attribution.
const std::string GetOnlineWallpaperKey(ash::WallpaperInfo info) {
  return info.unit_id.has_value() ? base::NumberToString(info.unit_id.value())
                                  : base::UnguessableToken::Create().ToString();
}

std::string GetJpegDataUrl(const unsigned char* data, size_t size) {
  std::string output = "data:image/jpeg;base64,";
  base::Base64EncodeAppend(base::make_span(data, size), &output);
  return output;
}

std::string GetBitmapJpegDataUrl(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  if (!gfx::JPEGCodec::Encode(bitmap, /*quality=*/90, &output)) {
    LOG(ERROR) << "Unable to encode bitmap";
    return std::string();
  }
  return GetJpegDataUrl(output.data(), output.size());
}

// Convenience method to get the current checkpoint.
ScheduleCheckpoint GetCurrentCheckPoint() {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  if (!dark_light_mode_controller) {
    return ScheduleCheckpoint::kDisabled;
  }
  return dark_light_mode_controller->current_checkpoint();
}

}  // namespace

PersonalizationAppWallpaperProviderImpl::
    PersonalizationAppWallpaperProviderImpl(
        content::WebUI* web_ui,
        std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
            wallpaper_fetcher_delegate)
    : web_ui_(web_ui),
      profile_(Profile::FromWebUI(web_ui)),
      wallpaper_fetcher_delegate_(std::move(wallpaper_fetcher_delegate)) {
  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
}

PersonalizationAppWallpaperProviderImpl::
    ~PersonalizationAppWallpaperProviderImpl() {
  if (!image_unit_id_map_.empty()) {
    // User viewed wallpaper page at least once during this session because
    // |image_unit_id_map_| has wallpaper unit ids saved. Check if this user
    // should see a wallpaper HaTS.
    ::ash::personalization_app::PersonalizationAppManagerFactory::
        GetForBrowserContext(profile_)
            ->MaybeStartHatsTimer(
                ::ash::personalization_app::HatsSurveyType::kWallpaper);
  }
}

void PersonalizationAppWallpaperProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::WallpaperProvider>
        receiver) {
  wallpaper_receiver_.reset();
  wallpaper_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppWallpaperProviderImpl::GetWallpaperAsJpegBytes(
    content::WebUIDataSource::GotDataCallback callback) {
  WallpaperController::Get()->LoadPreviewImage(std::move(callback));
}

bool PersonalizationAppWallpaperProviderImpl::IsEligibleForGooglePhotos() {
  return GetUser(profile_)->HasGaiaAccount();
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

  // Turn off the web contents background.
  static_cast<ContentsWebView*>(BrowserView::GetBrowserViewForNativeWindow(
                                    web_contents->GetTopLevelNativeWindow())
                                    ->contents_web_view())
      ->SetBackgroundVisible(false);
}

void PersonalizationAppWallpaperProviderImpl::MakeOpaque() {
  auto* web_contents = web_ui_->GetWebContents();

  // Reversing `contents_web_view` is sufficient to make the view opaque,
  // as `window_backdrop`, `top_level_window` and `web_contents` are not
  // highly impactful to the animated theme change effect.
  static_cast<ContentsWebView*>(BrowserView::GetBrowserViewForNativeWindow(
                                    web_contents->GetTopLevelNativeWindow())
                                    ->contents_web_view())
      ->SetBackgroundVisible(true);
}

void PersonalizationAppWallpaperProviderImpl::FetchCollections(
    FetchCollectionsCallback callback) {
  pending_collections_callbacks_.push_back(std::move(callback));
  if (wallpaper_collection_info_fetcher_) {
    // Collection fetching already started. No need to start a second time.
    return;
  }

  wallpaper_collection_info_fetcher_ =
      wallpaper_fetcher_delegate_->CreateBackdropCollectionInfoFetcher();

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
      wallpaper_fetcher_delegate_->CreateBackdropImageInfoFetcher(
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
  if (!is_google_photos_enterprise_enabled_) {
    wallpaper_receiver_.ReportBadMessage(
        "Cannot call `FetchGooglePhotosAlbums()` without confirming that the "
        "Google Photos enterprise setting is enabled.");
    return;
  }

  GetOrCreateGooglePhotosAlbumsFetcher()->AddRequestAndStartIfNecessary(
      resume_token, std::move(callback));
}

void PersonalizationAppWallpaperProviderImpl::FetchGooglePhotosSharedAlbums(
    const absl::optional<std::string>& resume_token,
    FetchGooglePhotosAlbumsCallback callback) {
  if (!is_google_photos_enterprise_enabled_) {
    wallpaper_receiver_.ReportBadMessage(
        "Cannot call `FetchGooglePhotosAlbums()` without confirming that the "
        "Google Photos enterprise setting is enabled.");
    return;
  }

  GetOrCreateGooglePhotosSharedAlbumsFetcher()->AddRequestAndStartIfNecessary(
      resume_token, std::move(callback));
}

void PersonalizationAppWallpaperProviderImpl::FetchGooglePhotosEnabled(
    FetchGooglePhotosEnabledCallback callback) {
  if (!IsEligibleForGooglePhotos()) {
    wallpaper_receiver_.ReportBadMessage(
        "Cannot call `FetchGooglePhotosEnabled()` without Google Photos "
        "Wallpaper integration enabled.");
    std::move(callback).Run(
        ash::personalization_app::mojom::GooglePhotosEnablementState::kError);
    return;
  }

  // base::Unretained is safe to use because |this| outlives
  // |google_photos_enabled_fetcher_|.
  GetOrCreateGooglePhotosEnabledFetcher()->AddRequestAndStartIfNecessary(
      base::BindOnce(
          &PersonalizationAppWallpaperProviderImpl::OnFetchGooglePhotosEnabled,
          base::Unretained(this), std::move(callback)));
}

void PersonalizationAppWallpaperProviderImpl::FetchGooglePhotosPhotos(
    const absl::optional<std::string>& item_id,
    const absl::optional<std::string>& album_id,
    const absl::optional<std::string>& resume_token,
    FetchGooglePhotosPhotosCallback callback) {
  if (!is_google_photos_enterprise_enabled_) {
    wallpaper_receiver_.ReportBadMessage(
        "Cannot call `FetchGooglePhotosPhotos()` without confirming that the "
        "Google Photos enterprise setting is enabled.");
    std::move(callback).Run(
        ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse::New(
            absl::nullopt, absl::nullopt));
    return;
  }

  GetOrCreateGooglePhotosPhotosFetcher()->AddRequestAndStartIfNecessary(
      item_id, album_id, resume_token, /*shuffle=*/false,
      base::BindOnce(
          &PersonalizationAppWallpaperProviderImpl::OnFetchGooglePhotosPhotos,
          weak_ptr_factory_.GetWeakPtr(), album_id, std::move(callback)));
}

void PersonalizationAppWallpaperProviderImpl::GetDefaultImageThumbnail(
    GetDefaultImageThumbnailCallback callback) {
  auto* wallpaper_controller = WallpaperController::Get();
  const user_manager::User* user = GetUser(profile_);
  base::FilePath default_wallpaper_path =
      wallpaper_controller->GetDefaultWallpaperPath(user->GetType());
  if (default_wallpaper_path.empty()) {
    std::move(callback).Run(GURL());
    return;
  }
  image_util::DecodeImageFile(
      base::BindOnce(
          &PersonalizationAppWallpaperProviderImpl::OnGetDefaultImage,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      default_wallpaper_path);
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
    wallpaper_receiver_.ReportBadMessage("Invalid local image path received");
    return;
  }
  if (!thumbnail_loader_) {
    thumbnail_loader_ = std::make_unique<ash::ThumbnailLoader>(profile_);
  }

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
  if (!wallpaper_controller_observer_.IsObserving()) {
    wallpaper_controller_observer_.Observe(ash::WallpaperController::Get());
  }
  // Call it once to send the first wallpaper.
  OnWallpaperResized();
}

void PersonalizationAppWallpaperProviderImpl::OnWallpaperResized() {
  wallpaper_attribution_info_fetcher_.reset();
  attribution_weak_ptr_factory_.InvalidateWeakPtrs();

  absl::optional<ash::WallpaperInfo> info =
      WallpaperController::Get()->GetActiveUserWallpaperInfo();
  if (!info) {
    DVLOG(1) << "No wallpaper info for active user. This should only happen in "
                "tests.";
    NotifyWallpaperChanged(nullptr);
    NotifyAttributionChanged(nullptr);
    return;
  }

  switch (info->type) {
    case ash::WallpaperType::kDaily:
    case ash::WallpaperType::kOnline: {
      if (info->collection_id.empty() || !info->unit_id.has_value()) {
        DVLOG(2) << "no collection_id or unit_id found";
        // Older versions of ChromeOS do not store these information, need to
        // look up all collections and match URL.
        FetchCollections(base::BindOnce(
            &PersonalizationAppWallpaperProviderImpl::FindAttribution,
            attribution_weak_ptr_factory_.GetWeakPtr(), *info));
        return;
      }

      backdrop::Collection collection;
      collection.set_collection_id(info->collection_id);
      FindAttribution(*info, std::vector<backdrop::Collection>{collection});
      return;
    }
    case ash::WallpaperType::kCustomized: {
      base::FilePath file_name = base::FilePath(info->location).BaseName();

      // Match selected wallpaper based on full filename including extension.
      const std::string& key = info->user_file_path.empty()
                                   ? file_name.value()
                                   : info->user_file_path;
      NotifyWallpaperChanged(
          ash::personalization_app::mojom::CurrentWallpaper::New(
              info->layout, info->type, key,
              /*description_title=*/std::string(),
              /*description_content=*/std::string()));

      // Do not show file extension in user-visible selected details text.
      std::vector<std::string> attribution = {
          file_name.RemoveExtension().value()};
      NotifyAttributionChanged(
          ash::personalization_app::mojom::CurrentAttribution::New(
              std::move(attribution), key));

      return;
    }
    case ash::WallpaperType::kDailyGooglePhotos:
    case ash::WallpaperType::kOnceGooglePhotos:
      WallpaperControllerClientImpl::Get()->FetchGooglePhotosPhoto(
          GetAccountId(profile_), info->location,
          base::BindOnce(&PersonalizationAppWallpaperProviderImpl::
                             SendGooglePhotosAttribution,
                         weak_ptr_factory_.GetWeakPtr(), *info));
      return;
    case ash::WallpaperType::kDefault:
    case ash::WallpaperType::kDevice:
    case ash::WallpaperType::kOneShot:
    case ash::WallpaperType::kOobe:
    case ash::WallpaperType::kPolicy:
    case ash::WallpaperType::kThirdParty: {
      const std::string key = base::UnguessableToken::Create().ToString();
      NotifyWallpaperChanged(
          ash::personalization_app::mojom::CurrentWallpaper::New(
              info->layout, info->type, key,
              /*description_title=*/std::string(),
              /*description_content=*/std::string()));
      NotifyAttributionChanged(
          ash::personalization_app::mojom::CurrentAttribution::New(
              std::vector<std::string>(), key));
      return;
    }
    case ash::WallpaperType::kCount:
      break;
  }

  // This can happen when a WallpaperType object from a different version of
  // ChromeOS persists through an upgrade or is synced to a different
  // version of ChromeOS. Handle the error as gracefully as possible. Pick a
  // safe wallpaper type `kOneShot` to send to personalization app.
  const std::string key = base::UnguessableToken::Create().ToString();
  NotifyWallpaperChanged(ash::personalization_app::mojom::CurrentWallpaper::New(
      info->layout, ash::WallpaperType::kOneShot, key,
      /*description_title=*/std::string(),
      /*description_content=*/std::string()));
  NotifyAttributionChanged(
      ash::personalization_app::mojom::CurrentAttribution::New(
          std::vector<std::string>(), key));

  // Continue to record data on how frequently this happens.
  SCOPED_CRASH_KEY_STRING32(
      "Wallpaper", "WallpaperType",
      base::NumberToString(
          static_cast<std::underlying_type_t<WallpaperType>>(info->type)));
  base::debug::DumpWithoutCrashing();
  return;
}

void PersonalizationAppWallpaperProviderImpl::OnWallpaperPreviewEnded() {
  DCHECK(wallpaper_observer_remote_.is_bound());
  wallpaper_observer_remote_->OnWallpaperPreviewEnded();
  // Make sure to fire another |OnWallpaperResized| after preview is over
  // so that personalization app ends up with correct wallpaper state.
  OnWallpaperResized();
}

void PersonalizationAppWallpaperProviderImpl::SelectWallpaper(
    uint64_t unit_id,
    bool preview_mode,
    SelectWallpaperCallback callback) {
  const auto& it = image_unit_id_map_.find(unit_id);

  if (it == image_unit_id_map_.end()) {
    wallpaper_receiver_.ReportBadMessage("Invalid image unit_id selected");
    return;
  }

  auto* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  if (!wallpaper_controller->CanSetUserWallpaper(GetAccountId(profile_))) {
    wallpaper_receiver_.ReportBadMessage("Invalid request to set wallpaper");
    return;
  }

  std::string collection_id;
  std::vector<ash::OnlineWallpaperVariant> variants;
  for (const auto& image_info : it->second) {
    variants.emplace_back(image_info.asset_id, image_info.image_url,
                          image_info.type);
    collection_id = image_info.collection_id;
  }

  const auto checkpoint = GetCurrentCheckPoint();
  auto* variant = FirstValidVariant(variants, checkpoint);
  DCHECK(variant);

  if (pending_select_wallpaper_callback_) {
    std::move(pending_select_wallpaper_callback_).Run(/*success=*/false);
  }
  pending_select_wallpaper_callback_ = std::move(callback);

  SetMinimizedWindowStateForPreview(preview_mode);

  WallpaperControllerClientImpl* client = WallpaperControllerClientImpl::Get();
  DCHECK(client);
  client->RecordWallpaperSourceUMA(ash::WallpaperType::kOnline);

  wallpaper_controller->SetOnlineWallpaper(
      ash::OnlineWallpaperParams(
          GetAccountId(profile_), variant->asset_id,
          GURL(variant->raw_url.spec()), collection_id,
          ash::WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED, preview_mode,
          /*from_user=*/true,
          /*daily_refresh_enabled=*/false, unit_id, variants),
      base::BindOnce(
          &PersonalizationAppWallpaperProviderImpl::OnOnlineWallpaperSelected,
          backend_weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppWallpaperProviderImpl::SelectDefaultImage(
    SelectDefaultImageCallback callback) {
  WallpaperControllerClientImpl* client = WallpaperControllerClientImpl::Get();
  DCHECK(client);
  client->RecordWallpaperSourceUMA(ash::WallpaperType::kDefault);
  WallpaperController::Get()->SetDefaultWallpaper(
      GetAccountId(profile_), /*show_wallpaper=*/true, std::move(callback));
}

void PersonalizationAppWallpaperProviderImpl::SelectLocalImage(
    const base::FilePath& path,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SelectLocalImageCallback callback) {
  if (local_images_.count(path) == 0) {
    wallpaper_receiver_.ReportBadMessage("Invalid local image path selected");
    return;
  }

  ash::WallpaperController* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  if (!wallpaper_controller->CanSetUserWallpaper(GetAccountId(profile_))) {
    wallpaper_receiver_.ReportBadMessage("Invalid request to set wallpaper");
    return;
  }

  if (pending_select_local_image_callback_) {
    std::move(pending_select_local_image_callback_).Run(/*success=*/false);
  }
  pending_select_local_image_callback_ = std::move(callback);

  SetMinimizedWindowStateForPreview(preview_mode);

  WallpaperControllerClientImpl::Get()->RecordWallpaperSourceUMA(
      ash::WallpaperType::kCustomized);

  wallpaper_controller->SetCustomWallpaper(
      GetAccountId(profile_), path, layout, preview_mode,
      base::BindOnce(
          &PersonalizationAppWallpaperProviderImpl::OnLocalImageSelected,
          backend_weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppWallpaperProviderImpl::SelectGooglePhotosPhoto(
    const std::string& id,
    ash::WallpaperLayout layout,
    bool preview_mode,
    SelectGooglePhotosPhotoCallback callback) {
  if (!is_google_photos_enterprise_enabled_) {
    wallpaper_receiver_.ReportBadMessage(
        "Cannot call `SelectGooglePhotosPhoto()` without confirming that the "
        "Google Photos enterprise setting is enabled.");
    std::move(callback).Run(false);
    return;
  }

  auto* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  if (!wallpaper_controller->CanSetUserWallpaper(GetAccountId(profile_))) {
    wallpaper_receiver_.ReportBadMessage("Invalid request to set wallpaper");
    return;
  }

  if (pending_select_google_photos_photo_callback_) {
    std::move(pending_select_google_photos_photo_callback_).Run(false);
  }
  pending_select_google_photos_photo_callback_ = std::move(callback);

  SetMinimizedWindowStateForPreview(preview_mode);

  WallpaperControllerClientImpl* client = WallpaperControllerClientImpl::Get();
  DCHECK(client);

  client->RecordWallpaperSourceUMA(ash::WallpaperType::kOnceGooglePhotos);

  wallpaper_controller->SetGooglePhotosWallpaper(
      ash::GooglePhotosWallpaperParams(GetAccountId(profile_), id,
                                       /*daily_refresh_enabled=*/false, layout,
                                       preview_mode,
                                       /*dedup_key=*/absl::nullopt),
      base::BindOnce(&PersonalizationAppWallpaperProviderImpl::
                         OnGooglePhotosWallpaperSelected,
                     backend_weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppWallpaperProviderImpl::SelectGooglePhotosAlbum(
    const std::string& album_id,
    SelectGooglePhotosAlbumCallback callback) {
  if (!is_google_photos_enterprise_enabled_) {
    wallpaper_receiver_.ReportBadMessage(
        "Rejected attempt to set Google Photos wallpaper while disabled via "
        "enterprise setting.");
    return;
  }

  auto* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  if (!wallpaper_controller->CanSetUserWallpaper(GetAccountId(profile_))) {
    wallpaper_receiver_.ReportBadMessage(
        "Invalid request to select google photos album");
    return;
  }

  if (pending_set_daily_refresh_callback_) {
    std::move(pending_set_daily_refresh_callback_).Run(/*success=*/false);
  }
  WallpaperControllerClientImpl* client = WallpaperControllerClientImpl::Get();
  DCHECK(client);

  client->RecordWallpaperSourceUMA(ash::WallpaperType::kDailyGooglePhotos);

  bool force_refresh = true;
  if (album_id.empty()) {
    // Empty |album_id| means disabling daily refresh.
    force_refresh = false;
  } else {
    // Only force refresh if the album does not contain the current wallpaper
    // image.
    const auto& it = album_id_dedup_key_map_.find(album_id);
    absl::optional<ash::WallpaperInfo> info =
        wallpaper_controller->GetActiveUserWallpaperInfo();
    if (info.has_value() && info->dedup_key.has_value()) {
      force_refresh =
          it == album_id_dedup_key_map_.end() ||
          it->second.find(info->dedup_key.value()) == it->second.end();
    }
  }
  DVLOG(1) << __func__ << " force_refresh=" << force_refresh;

  wallpaper_controller->SetGooglePhotosDailyRefreshAlbumId(
      GetAccountId(profile_), album_id);

  if (force_refresh) {
    pending_set_daily_refresh_callback_ = std::move(callback);
    wallpaper_controller->UpdateDailyRefreshWallpaper(base::BindOnce(
        &PersonalizationAppWallpaperProviderImpl::OnDailyRefreshWallpaperForced,
        backend_weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  std::move(callback).Run(/*success=*/true);
  // Trigger a `NotifyWallpaperChanged` to clear loading state in
  // Personalization App so it can't get stuck.
  OnWallpaperResized();
}

void PersonalizationAppWallpaperProviderImpl::
    GetGooglePhotosDailyRefreshAlbumId(
        GetGooglePhotosDailyRefreshAlbumIdCallback callback) {
  auto* controller = WallpaperController::Get();
  std::move(callback).Run(
      controller->GetGooglePhotosDailyRefreshAlbumId(GetAccountId(profile_)));
}

void PersonalizationAppWallpaperProviderImpl::SetCurrentWallpaperLayout(
    ash::WallpaperLayout layout) {
  WallpaperController::Get()->UpdateCurrentWallpaperLayout(
      GetAccountId(profile_), layout);
}

void PersonalizationAppWallpaperProviderImpl::SetDailyRefreshCollectionId(
    const std::string& collection_id,
    SetDailyRefreshCollectionIdCallback callback) {
  if (pending_set_daily_refresh_callback_) {
    std::move(pending_set_daily_refresh_callback_).Run(/*success=*/false);
  }

  auto* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  if (!wallpaper_controller->CanSetUserWallpaper(GetAccountId(profile_))) {
    wallpaper_receiver_.ReportBadMessage("Invalid request to set wallpaper");
    return;
  }
  if (collection_id == wallpaper_constants::kTimeOfDayWallpaperCollectionId) {
    wallpaper_receiver_.ReportBadMessage("Unsupported wallpaper collection");
    return;
  }
  wallpaper_controller->SetDailyRefreshCollectionId(GetAccountId(profile_),
                                                    collection_id);

  absl::optional<ash::WallpaperInfo> info =
      wallpaper_controller->GetActiveUserWallpaperInfo();
  DCHECK(info);

  if (collection_id.empty()) {
    // Daily refresh is disabled.
    std::move(callback).Run(/*success=*/info &&
                            info->type != WallpaperType::kDaily);
    return;
  }

  bool force_refresh = !info->unit_id.has_value();
  if (info->unit_id.has_value()) {
    const auto& it = image_unit_id_map_.find(info->unit_id.value());

    // Only force refresh if the current wallpaper image does not belong to
    // this collection.
    force_refresh = it == image_unit_id_map_.end() || it->second.empty() ||
                    it->second[0].collection_id != collection_id;
  }
  DVLOG(1) << __func__ << " info=" << info.value()
           << " collection_id=" << collection_id
           << " force_refresh=" << force_refresh;
  if (force_refresh) {
    pending_set_daily_refresh_callback_ = std::move(callback);
    wallpaper_controller->UpdateDailyRefreshWallpaper(base::BindOnce(
        &PersonalizationAppWallpaperProviderImpl::OnDailyRefreshWallpaperForced,
        weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  std::move(callback).Run(/*success=*/true);
}

void PersonalizationAppWallpaperProviderImpl::GetDailyRefreshCollectionId(
    GetDailyRefreshCollectionIdCallback callback) {
  auto* controller = WallpaperController::Get();
  std::move(callback).Run(
      controller->GetDailyRefreshCollectionId(GetAccountId(profile_)));
}

void PersonalizationAppWallpaperProviderImpl::UpdateDailyRefreshWallpaper(
    UpdateDailyRefreshWallpaperCallback callback) {
  if (pending_update_daily_refresh_wallpaper_callback_) {
    std::move(pending_update_daily_refresh_wallpaper_callback_)
        .Run(/*success=*/false);
  }

  pending_update_daily_refresh_wallpaper_callback_ = std::move(callback);

  auto* wallpaper_controller = WallpaperController::Get();
  absl::optional<ash::WallpaperInfo> info =
      wallpaper_controller->GetActiveUserWallpaperInfo();
  DCHECK(info);
  DCHECK(info->type == WallpaperType::kDaily ||
         info->type == WallpaperType::kDailyGooglePhotos);
  auto* client = WallpaperControllerClientImpl::Get();
  client->RecordWallpaperSourceUMA(info->type);

  wallpaper_controller->UpdateDailyRefreshWallpaper(base::BindOnce(
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
PersonalizationAppWallpaperProviderImpl::
    GetOrCreateGooglePhotosAlbumsFetcher() {
  if (!google_photos_albums_fetcher_) {
    google_photos_albums_fetcher_ =
        wallpaper_fetcher_delegate_->CreateGooglePhotosAlbumsFetcher(profile_);
  }
  return google_photos_albums_fetcher_.get();
}

wallpaper_handlers::GooglePhotosSharedAlbumsFetcher*
PersonalizationAppWallpaperProviderImpl::
    GetOrCreateGooglePhotosSharedAlbumsFetcher() {
  if (!google_photos_shared_albums_fetcher_) {
    google_photos_shared_albums_fetcher_ =
        wallpaper_fetcher_delegate_->CreateGooglePhotosSharedAlbumsFetcher(
            profile_);
  }
  return google_photos_shared_albums_fetcher_.get();
}

wallpaper_handlers::GooglePhotosEnabledFetcher*
PersonalizationAppWallpaperProviderImpl::
    GetOrCreateGooglePhotosEnabledFetcher() {
  if (!google_photos_enabled_fetcher_) {
    google_photos_enabled_fetcher_ =
        wallpaper_fetcher_delegate_->CreateGooglePhotosEnabledFetcher(profile_);
  }
  return google_photos_enabled_fetcher_.get();
}

wallpaper_handlers::GooglePhotosPhotosFetcher*
PersonalizationAppWallpaperProviderImpl::
    GetOrCreateGooglePhotosPhotosFetcher() {
  if (!google_photos_photos_fetcher_) {
    google_photos_photos_fetcher_ =
        wallpaper_fetcher_delegate_->CreateGooglePhotosPhotosFetcher(profile_);
  }
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
    // Do first pass to clear all unit_id associated with the images.
    base::ranges::for_each(images, [&](auto& proto_image) {
      image_unit_id_map_.erase(proto_image.unit_id());
    });
    // Do second pass to repopulate the map with fresh data.
    base::ranges::for_each(images, [&](auto& proto_image) {
      if (proto_image.has_asset_id() && proto_image.has_unit_id() &&
          proto_image.has_image_url()) {
        image_unit_id_map_[proto_image.unit_id()].push_back(
            ImageInfo(GURL(proto_image.image_url()), collection_id,
                      proto_image.asset_id(), proto_image.unit_id(),
                      proto_image.has_image_type()
                          ? proto_image.image_type()
                          : backdrop::Image::IMAGE_TYPE_UNKNOWN));
      }
    });
    result = std::move(images);
  }
  std::move(callback).Run(std::move(result));
}

void PersonalizationAppWallpaperProviderImpl::OnFetchGooglePhotosEnabled(
    FetchGooglePhotosEnabledCallback callback,
    ash::personalization_app::mojom::GooglePhotosEnablementState state) {
  is_google_photos_enterprise_enabled_ =
      state ==
      ash::personalization_app::mojom::GooglePhotosEnablementState::kEnabled;
  std::move(callback).Run(state);
}

void PersonalizationAppWallpaperProviderImpl::OnFetchGooglePhotosPhotos(
    absl::optional<std::string> album_id,
    FetchGooglePhotosPhotosCallback callback,
    mojo::StructPtr<mojom::FetchGooglePhotosPhotosResponse> response) {
  if (!album_id || !response || response.is_null()) {
    // Skip processing |album_id_dedup_key_map_| if there is no info on album or
    // response is invalid.
    std::move(callback).Run(std::move(response));
    return;
  }

  // Process |album_id_dedup_key_map_|.
  auto& photos = response->photos;
  if (photos.has_value()) {
    auto& photos_val = photos.value();
    std::set<std::string> dedup_keys;
    for (auto& photo : photos_val) {
      if (photo->dedup_key.has_value()) {
        dedup_keys.insert(photo->dedup_key.value());
      }
    }
    album_id_dedup_key_map_.insert({album_id.value(), std::move(dedup_keys)});
  }
  std::move(callback).Run(std::move(response));
}

void PersonalizationAppWallpaperProviderImpl::OnGetDefaultImage(
    GetDefaultImageThumbnailCallback callback,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Do not call |mojom::ReportBadMessage| here. The message is valid, but the
    // file may be corrupt or unreadable.
    std::move(callback).Run(GURL());
    return;
  }
  std::move(callback).Run(GURL(
      webui::GetBitmapDataUrl(*WallpaperResizer::GetResizedImage(
                                   image, kCurrentWallpaperThumbnailSizeDip)
                                   .bitmap())));
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
    std::move(callback).Run(GURL());
    return;
  }
  std::move(callback).Run(GURL(GetBitmapJpegDataUrl(*bitmap)));
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

void PersonalizationAppWallpaperProviderImpl::OnDailyRefreshWallpaperForced(
    bool success) {
  if (pending_set_daily_refresh_callback_) {
    std::move(pending_set_daily_refresh_callback_).Run(success);
  }
}

void PersonalizationAppWallpaperProviderImpl::FindAttribution(
    const ash::WallpaperInfo& info,
    const absl::optional<std::vector<backdrop::Collection>>& collections) {
  DCHECK(!wallpaper_attribution_info_fetcher_);
  if (!collections.has_value() || collections->empty()) {
    const std::string key = GetOnlineWallpaperKey(info);
    NotifyWallpaperChanged(
        ash::personalization_app::mojom::CurrentWallpaper::New(
            info.layout, info.type, key,
            /*description_title=*/std::string(),
            /*description_content=*/std::string()));
    NotifyAttributionChanged(
        ash::personalization_app::mojom::CurrentAttribution::New(
            std::vector<std::string>(), key));
    return;
  }

  std::size_t current_index = 0;
  wallpaper_attribution_info_fetcher_ =
      wallpaper_fetcher_delegate_->CreateBackdropImageInfoFetcher(
          collections->at(current_index).collection_id());

  wallpaper_attribution_info_fetcher_->Start(base::BindOnce(
      &PersonalizationAppWallpaperProviderImpl::FindImageMetadataInCollection,
      attribution_weak_ptr_factory_.GetWeakPtr(), info, current_index,
      collections));
}

void PersonalizationAppWallpaperProviderImpl::FindImageMetadataInCollection(
    const ash::WallpaperInfo& info,
    std::size_t current_index,
    const absl::optional<std::vector<backdrop::Collection>>& collections,
    bool success,
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  DCHECK(wallpaper_attribution_info_fetcher_);

  const backdrop::Image* backend_image = nullptr;
  if (success && !images.empty()) {
    for (const auto& proto_image : images) {
      if (!proto_image.has_image_url() || !proto_image.has_unit_id()) {
        break;
      }
      bool is_same_unit_id = info.unit_id.has_value() &&
                             proto_image.unit_id() == info.unit_id.value();
      bool is_same_url = info.location.rfind(proto_image.image_url(), 0) == 0;
      if (is_same_url) {
        backend_image = &proto_image;
        break;
      }
      if (is_same_unit_id) {
        backend_image = &proto_image;
      }
    }
  }

  if (backend_image) {
    NotifyWallpaperChanged(
        ash::personalization_app::mojom::CurrentWallpaper::New(
            info.layout, info.type,
            /*key=*/base::NumberToString(backend_image->unit_id()),
            backend_image->description_title(),
            backend_image->description_content()));
    std::vector<std::string> attributions;
    for (const auto& attr : backend_image->attribution()) {
      attributions.push_back(attr.text());
    }
    NotifyAttributionChanged(
        ash::personalization_app::mojom::CurrentAttribution::New(
            attributions, base::NumberToString(backend_image->unit_id())));
    wallpaper_attribution_info_fetcher_.reset();
    return;
  }

  ++current_index;

  if (current_index >= collections->size()) {
    const std::string key = GetOnlineWallpaperKey(info);
    NotifyWallpaperChanged(
        ash::personalization_app::mojom::CurrentWallpaper::New(
            info.layout, info.type, key,
            /*description_title=*/std::string(),
            /*description_content=*/std::string()));
    NotifyAttributionChanged(
        ash::personalization_app::mojom::CurrentAttribution::New(
            std::vector<std::string>(), key));
    wallpaper_attribution_info_fetcher_.reset();
    return;
  }

  auto fetcher = wallpaper_fetcher_delegate_->CreateBackdropImageInfoFetcher(
      collections->at(current_index).collection_id());
  fetcher->Start(base::BindOnce(
      &PersonalizationAppWallpaperProviderImpl::FindImageMetadataInCollection,
      attribution_weak_ptr_factory_.GetWeakPtr(), info, current_index,
      collections));
  // resetting the previous fetcher last because the current method is bound
  // to a callback owned by the previous fetcher.
  wallpaper_attribution_info_fetcher_ = std::move(fetcher);
}

void PersonalizationAppWallpaperProviderImpl::SendGooglePhotosAttribution(
    const ash::WallpaperInfo& info,
    mojo::StructPtr<ash::personalization_app::mojom::GooglePhotosPhoto> photo,
    bool success) {
  // If the fetch for |photo| succeeded but |photo| does not exist, that means
  // it has been removed from the user's library. When this occurs, the user's
  // wallpaper should be either (a) reset to default or (b) updated to a new
  // photo from the same collection depending on whether daily refresh is
  // enabled.
  if (success && !photo) {
    if (info.type == WallpaperType::kOnceGooglePhotos) {
      SelectDefaultImage(/*callback=*/base::DoNothing());
    } else if (info.type == WallpaperType::kDailyGooglePhotos) {
      UpdateDailyRefreshWallpaper(/*callback=*/base::DoNothing());
    } else {
      NOTREACHED();
    }
    return;
  }

  // NOTE: Old clients may not support |dedup_key| when setting Google Photos
  // wallpaper, so use |location| in such cases for backwards compatibility.
  NotifyWallpaperChanged(ash::personalization_app::mojom::CurrentWallpaper::New(
      info.layout, info.type,
      /*key=*/info.dedup_key.value_or(info.location),
      /*description_title=*/std::string(),
      /*description_content=*/std::string()));
  std::vector<std::string> attribution;
  if (!photo.is_null()) {
    attribution.push_back(photo->name);
  }
  NotifyAttributionChanged(
      ash::personalization_app::mojom::CurrentAttribution::New(
          attribution, info.dedup_key.value_or(info.location)));
}

void PersonalizationAppWallpaperProviderImpl::SetMinimizedWindowStateForPreview(
    bool preview_mode) {
  auto* wallpaper_controller = WallpaperController::Get();
  const std::string& user_id_hash = GetUser(profile_)->username_hash();
  if (preview_mode) {
    wallpaper_controller->MinimizeInactiveWindows(user_id_hash);
  } else {
    wallpaper_controller->RestoreMinimizedWindows(user_id_hash);
  }
}

void PersonalizationAppWallpaperProviderImpl::NotifyAttributionChanged(
    ash::personalization_app::mojom::CurrentAttributionPtr attribution) {
  DCHECK(wallpaper_observer_remote_.is_bound());
  wallpaper_observer_remote_->OnAttributionChanged(std::move(attribution));
}

void PersonalizationAppWallpaperProviderImpl::NotifyWallpaperChanged(
    ash::personalization_app::mojom::CurrentWallpaperPtr current_wallpaper) {
  DCHECK(wallpaper_observer_remote_.is_bound());
  wallpaper_observer_remote_->OnWallpaperChanged(std::move(current_wallpaper));
}

}  // namespace ash::personalization_app
