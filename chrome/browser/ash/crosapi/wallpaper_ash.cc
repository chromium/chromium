// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/wallpaper_ash.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
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

namespace crosapi {

WallpaperAsh::WallpaperAsh() = default;

WallpaperAsh::~WallpaperAsh() = default;

void WallpaperAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Wallpaper> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WallpaperAsh::SetWallpaper(mojom::WallpaperSettingsPtr wallpaper_settings,
                                const std::string& extension_id,
                                const std::string& extension_name,
                                SetWallpaperCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(ash::LoginState::Get()->IsUserLoggedIn());
  // Prevent any in progress decodes from changing wallpaper.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Notify the last pending request, if any, that it is canceled.
  if (pending_callback_) {
    std::move(pending_callback_).Run(std::vector<uint8_t>());
  }
  pending_callback_ = std::move(callback);
  const std::vector<uint8_t>& data = wallpaper_settings->data;
  data_decoder::DecodeImage(
      &data_decoder_, data, data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(
          &WallpaperAsh::OnWallpaperDecoded, weak_ptr_factory_.GetWeakPtr(),
          std::move(wallpaper_settings), extension_id, extension_name));
}

void WallpaperAsh::OnWallpaperDecoded(
    mojom::WallpaperSettingsPtr wallpaper_settings,
    const std::string& extension_id,
    const std::string& extension_name,
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (bitmap.isNull()) {
    LOG(ERROR) << "Decoding wallpaper data failed from extension_id '"
               << extension_id << "'";
    DCHECK(pending_callback_);
    std::move(pending_callback_).Run(std::vector<uint8_t>());
    return;
  }
  ash::WallpaperLayout layout = GetLayoutEnum(wallpaper_settings->layout);
  RecordCustomWallpaperLayout(layout);

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  auto account_id = user->GetAccountId();

  const std::string file_name =
      base::FilePath(wallpaper_settings->filename).BaseName().value();

  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  SkBitmap immutable_bitmap(bitmap);
  immutable_bitmap.setImmutable();
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(immutable_bitmap);
  image.MakeThreadSafe();

  WallpaperControllerClientImpl::Get()->SetThirdPartyWallpaper(
      account_id, file_name, layout, image);

  // We need to generate thumbnail image anyway to make the current third party
  // wallpaper syncable through different devices.
  image.EnsureRepsForSupportedScales();
  std::vector<uint8_t> thumbnail_data = GenerateThumbnail(
      image, gfx::Size(kWallpaperThumbnailWidth, kWallpaperThumbnailHeight));

  DCHECK(pending_callback_);
  std::move(pending_callback_).Run(thumbnail_data);
}

}  // namespace crosapi
