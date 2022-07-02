// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/wallpaper_ash.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/account_id/account_id.h"
#include "extensions/browser/event_router.h"
#include "ui/gfx/codec/jpeg_codec.h"

using content::BrowserThread;
using wallpaper_api_util::GenerateThumbnail;
using wallpaper_api_util::WallpaperDecoder;

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

}  // namespace

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
  wallpaper_decoder_ =
      new WallpaperDecoder(base::BindOnce(&WallpaperAsh::OnWallpaperDecoded,
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
  wallpaper_api_util::RecordCustomWallpaperLayout(layout);

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
      image, gfx::Size(WallpaperFunctionBase::kWallpaperThumbnailWidth,
                       WallpaperFunctionBase::kWallpaperThumbnailHeight));

  WallpaperControllerClientImpl::Get()->RecordWallpaperSourceUMA(
      ash::WallpaperType::kThirdParty);

  std::move(pending_callback_).Run(thumbnail_data);

  // reset remaining state
  wallpaper_settings_ = nullptr;
  extension_id_.clear();
  extension_name_.clear();
}

}  // namespace crosapi
