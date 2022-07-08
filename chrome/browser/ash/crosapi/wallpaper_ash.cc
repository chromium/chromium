// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/wallpaper_ash.h"

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia_operations.h"

using content::BrowserThread;

namespace {
ash::WallpaperLayout GetLayoutEnum(crosapi::mojom::WallpaperLayout layout) {
  switch (layout) {
    case crosapi::mojom::WallpaperLayout::kStretch:
      return ash::WALLPAPER_LAYOUT_STRETCH;
    case crosapi::mojom::WallpaperLayout::kCenter:
      return ash::WALLPAPER_LAYOUT_CENTER;
    case crosapi::mojom::WallpaperLayout::kCenterCropped:
      return ash::WALLPAPER_LAYOUT_CENTER_CROPPED;
    default:
      return ash::WALLPAPER_LAYOUT_CENTER;
  }
}

constexpr int kWallpaperThumbnailWidth = 108;
constexpr int kWallpaperThumbnailHeight = 68;

// Returns an image of |size| that contains as much of |image| as possible
// without distorting the |image|.  Unused areas are cropped away.
gfx::ImageSkia ScaleAspectRatioAndCropCenter(const gfx::Size& size,
                                             const gfx::ImageSkia& image) {
  float scale = std::min(static_cast<float>(image.width()) / size.width(),
                         static_cast<float>(image.height()) / size.height());
  gfx::Size scaled_size = {base::ClampFloor(scale * size.width()),
                           base::ClampFloor(scale * size.height())};
  gfx::Rect bounds{{0, 0}, image.size()};
  bounds.ClampToCenteredSize(scaled_size);
  auto scaled_and_cropped_image = gfx::ImageSkiaOperations::CreateTiledImage(
      image, bounds.x(), bounds.y(), bounds.width(), bounds.height());
  return gfx::ImageSkiaOperations::CreateResizedImage(
      scaled_and_cropped_image, skia::ImageOperations::RESIZE_LANCZOS3, size);
}

const int kThumbnailEncodeQuality = 90;

void RecordCustomWallpaperLayout(const ash::WallpaperLayout& layout) {
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.CustomLayout", layout,
                            ash::NUM_WALLPAPER_LAYOUT);
}

std::vector<uint8_t> GenerateThumbnail(const gfx::ImageSkia& image,
                                       const gfx::Size& size) {
  std::vector<uint8_t> data_out;
  gfx::JPEGCodec::Encode(*ScaleAspectRatioAndCropCenter(size, image).bitmap(),
                         kThumbnailEncodeQuality, &data_out);
  return data_out;
}

}  // namespace

namespace wallpaper_api_util {

WallpaperDecoder::WallpaperDecoder(DecodedCallback decoded_cb,
                                   CanceledCallback canceled_cb,
                                   FailedCallback failed_cb)
    : decoded_cb_(std::move(decoded_cb)),
      canceled_cb_(std::move(canceled_cb)),
      failed_cb_(std::move(failed_cb)) {}

WallpaperDecoder::~WallpaperDecoder() = default;

void WallpaperDecoder::Start(const std::vector<uint8_t>& image_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CHECK(chromeos::LoginState::Get()->IsUserLoggedIn());
  ImageDecoder::StartWithOptions(this, image_data, ImageDecoder::DEFAULT_CODEC,
                                 true);
}

void WallpaperDecoder::Cancel() {
  cancel_flag_.Set();
}

void WallpaperDecoder::OnImageDecoded(const SkBitmap& decoded_image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  SkBitmap immutable(decoded_image);
  immutable.setImmutable();
  gfx::ImageSkia final_image = gfx::ImageSkia::CreateFrom1xBitmap(immutable);
  final_image.MakeThreadSafe();
  if (cancel_flag_.IsSet()) {
    std::move(canceled_cb_).Run();
    delete this;
    return;
  }
  std::move(decoded_cb_).Run(final_image);
  delete this;
}

void WallpaperDecoder::OnDecodeImageFailed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(failed_cb_)
      .Run(l10n_util::GetStringUTF8(IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER));
  delete this;
}

}  // namespace wallpaper_api_util

namespace crosapi {

WallpaperAsh::WallpaperAsh() = default;

WallpaperAsh::~WallpaperAsh() = default;

void WallpaperAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Wallpaper> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WallpaperAsh::SetWallpaper(mojom::WallpaperSettingsPtr wallpaper,
                                const std::string& extension_id,
                                const std::string& extension_name,
                                SetWallpaperCallback callback) {
  // Cancel any ongoing SetWallpaper call as it will be replaced by this new
  // one.
  CancelAndReset();

  pending_callback_ = std::move(callback);
  wallpaper_settings_ = std::move(wallpaper);
  extension_id_ = extension_id;
  extension_name_ = extension_name;

  StartDecode(wallpaper_settings_->data);
}

void WallpaperAsh::CancelAndReset() {
  if (wallpaper_decoder_) {
    wallpaper_decoder_->Cancel();
    wallpaper_decoder_ = nullptr;
  }
  if (pending_callback_) {
    std::move(pending_callback_).Run(std::vector<uint8_t>());
  }
  wallpaper_settings_ = nullptr;
  extension_id_.clear();
  extension_name_.clear();
}

void WallpaperAsh::StartDecode(const std::vector<uint8_t>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (wallpaper_decoder_)
    wallpaper_decoder_->Cancel();
  wallpaper_decoder_ = new wallpaper_api_util::WallpaperDecoder(
      base::BindOnce(&WallpaperAsh::OnWallpaperDecoded,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WallpaperAsh::OnDecodingCanceled,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WallpaperAsh::OnDecodingFailed,
                     weak_ptr_factory_.GetWeakPtr()));
  wallpaper_decoder_->Start(data);
}

void WallpaperAsh::OnDecodingCanceled() {
  wallpaper_decoder_ = nullptr;
  CancelAndReset();
}

void WallpaperAsh::OnDecodingFailed(const std::string& error) {
  wallpaper_decoder_ = nullptr;
  CancelAndReset();
}

void WallpaperAsh::OnWallpaperDecoded(const gfx::ImageSkia& image) {
  ash::WallpaperLayout layout = GetLayoutEnum(wallpaper_settings_->layout);
  RecordCustomWallpaperLayout(layout);

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  auto account_id = user->GetAccountId();

  const std::string file_name =
      base::FilePath(wallpaper_settings_->filename).BaseName().value();
  WallpaperControllerClientImpl::Get()->SetCustomWallpaper(
      account_id, file_name, layout, image,
      /*preview_mode=*/false);
  wallpaper_decoder_ = nullptr;

  // We need to generate thumbnail image anyway to make the current third party
  // wallpaper syncable through different devices.
  image.EnsureRepsForSupportedScales();
  std::vector<uint8_t> thumbnail_data = GenerateThumbnail(
      image, gfx::Size(kWallpaperThumbnailWidth, kWallpaperThumbnailHeight));

  WallpaperControllerClientImpl::Get()->RecordWallpaperSourceUMA(
      ash::WallpaperType::kThirdParty);

  std::move(pending_callback_).Run(thumbnail_data);

  // reset remaining state
  wallpaper_settings_ = nullptr;
  extension_id_.clear();
  extension_name_.clear();
}

}  // namespace crosapi
