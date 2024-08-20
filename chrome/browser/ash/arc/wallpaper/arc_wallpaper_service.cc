// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/wallpaper/arc_wallpaper_service.h"

#include <stdlib.h>

#include <string>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
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

// This enum is used for UMA. Do not reuse or modify values.
enum class ArcWallpaperApi {
  kSet = 0,
  kSetDefault = 1,
  kGet = 2,
  kMaxValue = kGet,
};

void RecordApiUsage(const ArcWallpaperApi api) {
  base::UmaHistogramEnumeration("Arc.WallpaperApiUsage", api);
}

std::vector<uint8_t> EncodeImagePng(const gfx::ImageSkia& image) {
  std::vector<uint8_t> result;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*image.bitmap(), true, &result);
  return result;
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

class DecodeRequestSenderImpl
    : public ArcWallpaperService::DecodeRequestSender {
 public:
  void SendDecodeRequest(ImageDecoder::ImageRequest* request,
                         const std::vector<uint8_t>& data) override {
    ImageDecoder::StartWithOptions(request, data, ImageDecoder::DEFAULT_CODEC,
                                   true, gfx::Size());
  }
};

}  // namespace

class ArcWallpaperService::DecodeRequest : public ImageDecoder::ImageRequest {
 public:
  DecodeRequest(ArcWallpaperService* service, int32_t android_id)
      : service_(service), android_id_(android_id) {}

  DecodeRequest(const DecodeRequest&) = delete;
  DecodeRequest& operator=(const DecodeRequest&) = delete;

  ~DecodeRequest() override = default;

  // ImageDecoder::ImageRequest overrides.
  void OnImageDecoded(const SkBitmap& bitmap) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Make the SkBitmap immutable as we won't modify it. This is important
    // because otherwise it gets duplicated during painting, wasting memory.
    SkBitmap immutable_bitmap(bitmap);
    immutable_bitmap.setImmutable();
    gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(immutable_bitmap);
    image.MakeThreadSafe();

    service_->OnWallpaperDecoded(image, android_id_);
  }

  void OnDecodeImageFailed() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DLOG(ERROR) << "Failed to decode wallpaper image.";
    service_->NotifyWallpaperChangedAndReset(android_id_);
  }

 private:
  // ArcWallpaperService owns DecodeRequest, so it will outlive this.
  const raw_ptr<ArcWallpaperService> service_;
  const int32_t android_id_;
};

ArcWallpaperService::DecodeRequestSender::~DecodeRequestSender() = default;

void ArcWallpaperService::SetDecodeRequestSenderForTesting(
    std::unique_ptr<DecodeRequestSender> sender) {
  decode_request_sender_ = std::move(sender);
}

// static
ArcWallpaperService* ArcWallpaperService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcWallpaperServiceFactory::GetForBrowserContext(context);
}

ArcWallpaperService::ArcWallpaperService(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      decode_request_sender_(std::make_unique<DecodeRequestSenderImpl>()) {
  arc_bridge_service_->wallpaper()->SetHost(this);
}

ArcWallpaperService::~ArcWallpaperService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->wallpaper()->SetHost(nullptr);
}

void ArcWallpaperService::SetWallpaper(const std::vector<uint8_t>& data,
                                       int32_t wallpaper_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RecordApiUsage(ArcWallpaperApi::kSet);

  if (wallpaper_id == 0)
    wallpaper_id = -1;

  // Previous request will be cancelled at destructor of
  // ImageDecoder::ImageRequest.
  decode_request_ = std::make_unique<DecodeRequest>(this, wallpaper_id);
  DCHECK(decode_request_sender_);
  decode_request_sender_->SendDecodeRequest(decode_request_.get(), data);
}

void ArcWallpaperService::SetDefaultWallpaper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RecordApiUsage(ArcWallpaperApi::kSetDefault);

  // Previous request will be cancelled at destructor of
  // ImageDecoder::ImageRequest.
  decode_request_.reset();
  const user_manager::User* const primary_user =
      UserManager::Get()->GetPrimaryUser();
  ash::WallpaperController::Get()->SetDefaultWallpaper(
      primary_user->GetAccountId(),
      primary_user->is_active() /*show_wallpaper=*/, base::DoNothing());
}

void ArcWallpaperService::GetWallpaper(GetWallpaperCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RecordApiUsage(ArcWallpaperApi::kGet);

  gfx::ImageSkia image = ash::WallpaperController::Get()->GetWallpaperImage();
  if (!image.isNull())
    image.SetReadOnly();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&EncodeImagePng, image), std::move(callback));
}

void ArcWallpaperService::OnWallpaperDecoded(const gfx::ImageSkia& image,
                                             int32_t android_id) {
  const AccountId account_id =
      UserManager::Get()->GetPrimaryUser()->GetAccountId();

  const bool result =
      WallpaperControllerClientImpl::Get()->SetThirdPartyWallpaper(
          account_id, kAndroidWallpaperFilename,
          ash::WALLPAPER_LAYOUT_CENTER_CROPPED, image);

  // Notify the Android side whether the request is going through or not.
  if (result)
    NotifyWallpaperChanged(android_id);
  else
    NotifyWallpaperChangedAndReset(android_id);
}

void ArcWallpaperService::NotifyWallpaperChanged(int android_id) {
  mojom::WallpaperInstance* const wallpaper_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->wallpaper(),
                                  OnWallpaperChanged);
  if (wallpaper_instance == nullptr)
    return;
  wallpaper_instance->OnWallpaperChanged(android_id);
}

void ArcWallpaperService::NotifyWallpaperChangedAndReset(int32_t android_id) {
  // Invoke NotifyWallpaperChanged so that setWallpaper completes in Android
  // side.
  NotifyWallpaperChanged(android_id);
  // Invoke NotifyWallpaperChanged with -1 so that Android side regards the
  // wallpaper of |android_id_| is no longer used at Chrome side.
  NotifyWallpaperChanged(-1);
}

// static
void ArcWallpaperService::EnsureFactoryBuilt() {
  ArcWallpaperServiceFactory::GetInstance();
}

}  // namespace arc
