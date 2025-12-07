// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/wallpaper/arc_wallpaper_service.h"

#include <stdlib.h>

#include <string>
#include <utility>

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chromeos/ash/experiences/arc/arc_browser_context_keyed_service_factory_base.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/constants.mojom.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

using user_manager::UserManager;

namespace arc {
namespace {

constexpr char kAndroidWallpaperFilename[] = "android.jpg";

std::vector<uint8_t> EncodeImagePng(const gfx::ImageSkia& image) {
  std::optional<std::vector<uint8_t>> result =
      gfx::PNGCodec::FastEncodeBGRASkBitmap(*image.bitmap(),
                                            /*discard_transparency=*/true);
  return result.value_or(std::vector<uint8_t>());
}

// Singleton factory for ArcWallpaperService.
class ArcWallpaperServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcWallpaperService,
          ArcWallpaperServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcWallpaperServiceFactory";

  static ArcWallpaperServiceFactory* GetInstance() {
    return base::Singleton<ArcWallpaperServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcWallpaperServiceFactory>;
  ArcWallpaperServiceFactory() = default;
  ~ArcWallpaperServiceFactory() override = default;
};

class ImageDecoderImpl : public ArcWallpaperService::ImageDecoder {
 public:
  ImageDecoderImpl() = default;
  ImageDecoderImpl(const ImageDecoderImpl&) = delete;
  ImageDecoderImpl& operator=(const ImageDecoderImpl&) = delete;
  ~ImageDecoderImpl() override = default;

  // ArcWallpaperService::ImageDecoder:
  void DecodeImage(const std::vector<uint8_t>& data,
                   ResultCallback callback) override {
    data_decoder::DecodeImageIsolated(
        base::as_byte_span(data), data_decoder::mojom::ImageCodec::kDefault,
        /*shrink_to_fit=*/true,
        static_cast<int64_t>(IPC::mojom::kChannelMaximumMessageSize),
        /*desired_image_frame_size=*/gfx::Size(), std::move(callback));
  }
};

}  // namespace

// static
void ArcWallpaperService::EnsureFactoryBuilt() {
  ArcWallpaperServiceFactory::GetInstance();
}

// static
ArcWallpaperService* ArcWallpaperService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcWallpaperServiceFactory::GetForBrowserContext(context);
}

ArcWallpaperService::ArcWallpaperService(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      image_decoder_(std::make_unique<ImageDecoderImpl>()) {
  arc_bridge_service_->wallpaper()->SetHost(this);
}

ArcWallpaperService::~ArcWallpaperService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->wallpaper()->SetHost(nullptr);
}

void ArcWallpaperService::SetWallpaper(const std::vector<uint8_t>& data,
                                       int32_t wallpaper_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (wallpaper_id == 0)
    wallpaper_id = -1;

  // Cancel previous request if any and start new one.
  weak_ptr_factory_for_decode_.InvalidateWeakPtrs();
  image_decoder_->DecodeImage(
      data,
      base::BindOnce(&ArcWallpaperService::OnImageDecoded,
                     weak_ptr_factory_for_decode_.GetWeakPtr(), wallpaper_id));
}

void ArcWallpaperService::SetDefaultWallpaper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Cancel pending decoding request if any.
  weak_ptr_factory_for_decode_.InvalidateWeakPtrs();

  const user_manager::User* const primary_user =
      UserManager::Get()->GetPrimaryUser();
  ash::WallpaperController::Get()->SetDefaultWallpaper(
      primary_user->GetAccountId(),
      primary_user->is_active() /*show_wallpaper=*/, base::DoNothing());
}

void ArcWallpaperService::GetWallpaper(GetWallpaperCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  gfx::ImageSkia image = ash::WallpaperController::Get()->GetWallpaperImage();
  if (!image.isNull())
    image.SetReadOnly();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&EncodeImagePng, image), std::move(callback));
}

void ArcWallpaperService::SetImageDecoderForTesting(
    std::unique_ptr<ImageDecoder> decoder) {
  CHECK(decoder);
  image_decoder_ = std::move(decoder);
}

void ArcWallpaperService::OnImageDecoded(int wallpaper_id,
                                         const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (bitmap.isNull()) {
    DLOG(ERROR) << "Failed to decode wallpaper image.";
    NotifyWallpaperChangedAndReset(wallpaper_id);
    return;
  }

  // Make the SkBitmap immutable as we won't modify it. This is important
  // because otherwise it gets duplicated during painting, wasting memory.
  SkBitmap immutable_bitmap(bitmap);
  immutable_bitmap.setImmutable();
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(immutable_bitmap);
  image.MakeThreadSafe();

  OnWallpaperDecoded(wallpaper_id, image);
}

void ArcWallpaperService::OnWallpaperDecoded(int32_t wallpaper_id,
                                             const gfx::ImageSkia& image) {
  const AccountId account_id =
      UserManager::Get()->GetPrimaryUser()->GetAccountId();

  const bool result =
      WallpaperControllerClientImpl::Get()->SetThirdPartyWallpaper(
          account_id, kAndroidWallpaperFilename,
          ash::WALLPAPER_LAYOUT_CENTER_CROPPED, image);

  // Notify the Android side whether the request is going through or not.
  if (result)
    NotifyWallpaperChanged(wallpaper_id);
  else
    NotifyWallpaperChangedAndReset(wallpaper_id);
}

void ArcWallpaperService::NotifyWallpaperChanged(int wallpaper_id) {
  mojom::WallpaperInstance* const wallpaper_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->wallpaper(),
                                  OnWallpaperChanged);
  if (wallpaper_instance == nullptr)
    return;
  wallpaper_instance->OnWallpaperChanged(wallpaper_id);
}

void ArcWallpaperService::NotifyWallpaperChangedAndReset(int32_t wallpaper_id) {
  // Invoke NotifyWallpaperChanged so that setWallpaper completes in Android
  // side.
  NotifyWallpaperChanged(wallpaper_id);
  // Invoke NotifyWallpaperChanged with -1 so that Android side regards the
  // wallpaper of |wallpaper_id| is no longer used at Chrome side.
  NotifyWallpaperChanged(-1);
}

}  // namespace arc
