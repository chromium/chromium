// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/snapper_provider.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/gfx/geometry/size.h"

// Uncomment below to enable a fake API for local debugging purposes.
// #define FAKE_SEA_PEN_FETCHER_FOR_DEBUG

#if defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

static_assert(DCHECK_IS_ON(),
              "FakeSeaPenFetcher only allowed in DCHECK builds");

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"

#endif  // defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

namespace wallpaper_handlers {

namespace {

// Double the maximum size that thumbnails are displayed at in SeaPen UI.
constexpr gfx::Size kDesiredThumbnailSize = {880, 440};

#if defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

std::string MakeFakeJpgData() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kDesiredThumbnailSize.width(),
                        kDesiredThumbnailSize.height());
  bitmap.eraseColor(SkColorSetARGB(base::RandInt(0, 255), base::RandInt(0, 255),
                                   base::RandInt(0, 255),
                                   base::RandInt(0, 255)));
  std::vector<unsigned char> encoded_data;
  CHECK(gfx::JPEGCodec::Encode(bitmap, /*quality=*/10, &encoded_data));
  return std::string(encoded_data.begin(), encoded_data.end());
}

std::vector<ash::SeaPenImage> MakeFakeSeaPenImages() {
  std::vector<ash::SeaPenImage> result;
  for (int i = 0; i < base::RandInt(0, 6); i++) {
    result.emplace_back(MakeFakeJpgData(), base::RandInt(0, INT32_MAX),
                        manta::proto::RESOLUTION_1024);
  }
  return result;
}

void RunOnFetchThumbnailsComplete(
    SeaPenFetcher::OnFetchThumbnailsComplete callback,
    std::vector<ash::SeaPenImage> images) {
  std::move(callback).Run(std::move(images), manta::MantaStatusCode::kOk);
}

class FakeSeaPenFetcher : public SeaPenFetcher {
 public:
  FakeSeaPenFetcher()
      : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  FakeSeaPenFetcher(const FakeSeaPenFetcher&) = delete;
  FakeSeaPenFetcher& operator=(const FakeSeaPenFetcher&) = delete;

  ~FakeSeaPenFetcher() override = default;

  void FetchThumbnails(
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      OnFetchThumbnailsComplete callback) override {
    sequenced_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&MakeFakeSeaPenImages),
        base::BindOnce(&RunOnFetchThumbnailsComplete, std::move(callback)));
  }

  void FetchWallpaper(
      const ash::SeaPenImage& thumbnail,
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      OnFetchWallpaperComplete callback) override {
    std::move(callback).Run(ash::SeaPenImage(thumbnail.jpg_bytes, thumbnail.id,
                                             thumbnail.resolution));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

#else  // defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

class SeaPenFetcherImpl : public SeaPenFetcher {
 public:
  explicit SeaPenFetcherImpl(Profile* profile) {
    CHECK(ash::features::IsSeaPenEnabled() ||
          ash::features::IsVcBackgroundReplaceEnabled());
    CHECK(manta::features::IsMantaServiceEnabled());
    auto* manta_service = manta::MantaServiceFactory::GetForProfile(profile);
    if (manta_service) {
      snapper_provider_ = manta_service->CreateSnapperProvider();
    }
  }

  SeaPenFetcherImpl(const SeaPenFetcherImpl&) = delete;
  SeaPenFetcherImpl& operator=(const SeaPenFetcherImpl&) = delete;

  ~SeaPenFetcherImpl() override = default;

  void FetchThumbnails(
      manta::proto::FeatureName feature_name,
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      OnFetchThumbnailsComplete callback) override {
    if (!snapper_provider_) {
      LOG(WARNING) << "SnapperProvider not available";
      std::move(callback).Run(std::nullopt,
                              manta::MantaStatusCode::kGenericError);
      return;
    }
    if (query->is_text_query() &&
        query->get_text_query().size() >
            ash::personalization_app::mojom::kMaximumSearchWallpaperTextBytes) {
      LOG(WARNING) << "Query too long. Size received: "
                   << query->get_text_query().size();
      std::move(callback).Run(std::nullopt,
                              manta::MantaStatusCode::kInvalidInput);
      return;
    }
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (pending_fetch_thumbnails_callback_) {
      std::move(pending_fetch_thumbnails_callback_)
          .Run(std::nullopt, manta::MantaStatusCode::kOk);
    }
    pending_fetch_thumbnails_callback_ = std::move(callback);
    auto request = CreateMantaRequest(query, std::nullopt,
                                      /*num_outputs=*/8, kDesiredThumbnailSize,
                                      feature_name);
    snapper_provider_->Call(
        request, base::BindOnce(&SeaPenFetcherImpl::OnFetchThumbnailsDone,
                                weak_ptr_factory_.GetWeakPtr(), query.Clone()));
  }

  void OnFetchThumbnailsDone(
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      std::unique_ptr<manta::proto::Response> response,
      manta::MantaStatus status) {
    DCHECK(pending_fetch_thumbnails_callback_);
    if (status.status_code != manta::MantaStatusCode::kOk || !response) {
      LOG(WARNING) << "Failed to fetch manta response: " << status.message;
      std::move(pending_fetch_thumbnails_callback_)
          .Run(std::nullopt, status.status_code);
      return;
    }

    std::vector<ash::SeaPenImage> images;
    for (auto& data : *response->mutable_output_data()) {
      if (!IsValidOutput(data, __func__)) {
        continue;
      }
      images.emplace_back(
          std::move(*data.mutable_image()->mutable_serialized_bytes()),
          data.generation_seed());
    }
    std::move(pending_fetch_thumbnails_callback_)
        .Run(std::move(images), status.status_code);
  }

  void FetchWallpaper(
      manta::proto::FeatureName feature_name,
      const ash::SeaPenImage& thumbnail,
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      OnFetchWallpaperComplete callback) override {
    if (!snapper_provider_) {
      LOG(WARNING) << "SnapperProvider not available";
      std::move(callback).Run(std::nullopt);
      return;
    }
    if (query->is_text_query()) {
      CHECK_LE(
          query->get_text_query().size(),
          ash::personalization_app::mojom::kMaximumSearchWallpaperTextBytes);
    }
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (pending_fetch_wallpaper_callback_) {
      std::move(pending_fetch_wallpaper_callback_).Run(std::nullopt);
    }
    pending_fetch_wallpaper_callback_ = std::move(callback);

    snapper_provider_->Call(
        CreateMantaRequest(query, thumbnail.id, /*num_outputs=*/1,
                           GetLargestDisplaySizeLandscape(), feature_name),
        base::BindOnce(&SeaPenFetcherImpl::OnFetchWallpaperDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnFetchWallpaperDone(std::unique_ptr<manta::proto::Response> response,
                            manta::MantaStatus status) {
    DCHECK(pending_fetch_wallpaper_callback_);
    if (status.status_code != manta::MantaStatusCode::kOk || !response) {
      LOG(WARNING) << "Failed to fetch manta response: " << status.message;
      std::move(pending_fetch_wallpaper_callback_).Run(std::nullopt);
      return;
    }
    std::vector<ash::SeaPenImage> images;
    for (auto& data : *response->mutable_output_data()) {
      if (!IsValidOutput(data, __func__)) {
        continue;
      }
      images.emplace_back(
          std::move(*data.mutable_image()->mutable_serialized_bytes()),
          data.generation_seed());
    }
    if (images.empty()) {
      LOG(WARNING) << "Got empty images";
      std::move(pending_fetch_wallpaper_callback_).Run(std::nullopt);
      return;
    }
    if (images.size() > 1) {
      LOG(WARNING) << "Got more than 1 output image";
    }
    std::move(pending_fetch_wallpaper_callback_).Run(std::move(images.at(0)));
  }

 private:
  OnFetchThumbnailsComplete pending_fetch_thumbnails_callback_;
  OnFetchWallpaperComplete pending_fetch_wallpaper_callback_;
  std::unique_ptr<manta::SnapperProvider> snapper_provider_;
  base::WeakPtrFactory<SeaPenFetcherImpl> weak_ptr_factory_{this};
};

#endif  // defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

}  // namespace

SeaPenFetcher::SeaPenFetcher() = default;

SeaPenFetcher::~SeaPenFetcher() = default;

std::unique_ptr<SeaPenFetcher> SeaPenFetcher::MakeSeaPenFetcher(
    Profile* profile) {
#ifdef FAKE_SEA_PEN_FETCHER_FOR_DEBUG
  return std::make_unique<FakeSeaPenFetcher>();
#else   // FAKE_SEA_PEN_FETCHER_FOR_DEBUG
  return std::make_unique<SeaPenFetcherImpl>(profile);
#endif  // FAKE_SEA_PEN_FETCHER_FOR_DEBUG
}

}  // namespace wallpaper_handlers
