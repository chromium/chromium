// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/barrier_callback.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers_metric_utils.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"

namespace wallpaper_handlers {

namespace {

// Double the maximum size that thumbnails are displayed at in SeaPen UI.
constexpr gfx::Size kDesiredThumbnailSize = {880, 440};

const net::NetworkTrafficAnnotationTag kCameraBackgroundTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("camera_background_request", R"(
        semantics {
          sender: "AI Backgrounds"
          description:
            "ChromeOS can help you create a new camera background image by "
            "sending a query with your selected background style to Google's "
            "servers. Google returns suggested background images which you "
            "may choose to use as your camera background."
          trigger:
            "When the camera is in use, the user clicks the video call "
            "controls in the shelf, then clicks the 'Image' button in the "
            "'Background' section, then clicks 'Create with AI', then clicks "
            "'Create'."
          internal {
            contacts {
                email: "cros-manta-team@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: USER_CONTENT
          }
          data:
            "The selected background image style from the provided set of "
            "styles."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-03-19"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Not controlled by a setting. The feature is triggered manually "
            "by the user."
          policy_exception_justification:
            "Not implemented. The feature is not supported on managed devices."
        })");

const net::NetworkTrafficAnnotationTag kWallpaperTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("wallpaper_request", R"(
        semantics {
          sender: "AI Wallpapers"
          description:
            "ChromeOS can help you create a new desktop wallpaper by sending "
            "a query with your selected wallpaper style to Google's servers. "
            "Google returns thumbnails of suggested images which you may "
            "choose to use as your desktop wallpaper. Your choice is sent to "
            "Google's servers and an enlarged version is returned to your "
            "device. "
          trigger:
            "User visits Wallpaper section of Personalization App by right "
            "clicking the desktop and selecting 'Wallpaper and style', or "
            "through 'Wallpaper and style' within the ChromeOS Settings app, "
            "then clicking Wallpaper, then 'Create With AI', and clicking "
            "'Create'."
          internal {
            contacts {
                email: "cros-manta-team@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: USER_CONTENT
          }
          data:
            "The selected wallpaper style from the provided set of styles, "
            "along with dimensions of the user's largest active display, and "
            "an integer seed value indicating which previously-returned "
            "wallpaper thumbnail the user has chosen to enlarge and use."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-03-19"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Not controlled by a setting. The feature is triggered manually "
            "by the user."
          policy_exception_justification:
            "Not implemented. The feature is not supported on managed devices."
        })");

net::NetworkTrafficAnnotationTag TrafficAnnotationForFeature(
    manta::proto::FeatureName feature_name) {
  if (feature_name == manta::proto::FeatureName::CHROMEOS_VC_BACKGROUNDS) {
    return kCameraBackgroundTrafficAnnotation;
  } else if (feature_name == manta::proto::FeatureName::CHROMEOS_WALLPAPER) {
    return kWallpaperTrafficAnnotation;
  } else {
    LOG(FATAL) << "Unknown feature_name " << feature_name;
  }
}

std::optional<ash::SeaPenImage> ToSeaPenImage(const uint32_t generation_seed,
                                              const SkBitmap& decoded_bitmap) {
  base::AssertLongCPUWorkAllowed();
  std::vector<unsigned char> data;
  if (!gfx::JPEGCodec::Encode(decoded_bitmap, /*quality=*/100, &data)) {
    return std::nullopt;
  }
  return ash::SeaPenImage(std::string(data.begin(), data.end()),
                          generation_seed);
}

void EncodeBitmap(
    base::OnceCallback<void(std::optional<ash::SeaPenImage>)> callback,
    uint32_t generation_seed,
    const SkBitmap& decoded_bitmap) {
  if (decoded_bitmap.empty()) {
    LOG(WARNING) << "Failed to decode jpg bytes";
    std::move(callback).Run(std::nullopt);
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ToSeaPenImage, generation_seed, decoded_bitmap),
      std::move(callback));
}

void SanitizeJpgBytes(
    const manta::proto::OutputData& output_data,
    data_decoder::DataDecoder* data_decoder,
    base::OnceCallback<void(std::optional<ash::SeaPenImage>)> callback) {
  if (!IsValidOutput(output_data, __func__)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  data_decoder::DecodeImage(
      data_decoder, base::as_byte_span(output_data.image().serialized_bytes()),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&EncodeBitmap, std::move(callback),
                     output_data.generation_seed()));
}

manta::MantaStatusCode GetMantaStatusCodeForEmptyImageResponse(
    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
    const ::google::protobuf::RepeatedPtrField<::manta::proto::FilteredData>&
        filtered_data) {
  if (!ash::features::IsSeaPenTextInputEnabled() ||
      query_tag !=
          ash::personalization_app::mojom::SeaPenQuery::Tag::kTextQuery) {
    return manta::MantaStatusCode::kGenericError;
  }
  for (auto& filtered_datum : filtered_data) {
    if (filtered_datum.reason() == manta::proto::FilteredReason::IMAGE_SAFETY ||
        filtered_datum.reason() ==
            manta::proto::FilteredReason::TEXT_BLOCKLIST) {
      // If anything has been filtered due to safety, send the blocked
      // outputs result.
      return manta::MantaStatusCode::kBlockedOutputs;
    }
  }
  return manta::MantaStatusCode::kGenericError;
}

std::vector<ash::SeaPenImage> TakeValidImages(
    const std::vector<std::optional<ash::SeaPenImage>>& optional_images) {
  std::vector<ash::SeaPenImage> filtered_images;
  for (auto& image : optional_images) {
    if (image.has_value() && !image->jpg_bytes.empty()) {
      filtered_images.emplace_back(std::move(image->jpg_bytes), image->id);
    }
  }
  return filtered_images;
}

class SeaPenFetcherImpl : public SeaPenFetcher {
 public:
  // `snapper_provider` may be null.
  explicit SeaPenFetcherImpl(
      std::unique_ptr<manta::SnapperProvider> snapper_provider)
      : snapper_provider_(std::move(snapper_provider)) {
    CHECK(ash::features::IsSeaPenEnabled() ||
          ash::features::IsVcBackgroundReplaceEnabled());
    CHECK(manta::features::IsMantaServiceEnabled());
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
            ash::personalization_app::mojom::
                kMaximumGetSeaPenThumbnailsTextBytes) {
      LOG(WARNING) << "Query too long. Size received: "
                   << query->get_text_query().size();
      std::move(callback).Run(std::nullopt,
                              manta::MantaStatusCode::kInvalidInput);
      return;
    }

    fetch_thumbnails_timer_.Stop();
    fetch_thumbnails_weak_ptr_factory_.InvalidateWeakPtrs();

    if (pending_fetch_thumbnails_callback_) {
      std::move(pending_fetch_thumbnails_callback_)
          .Run(std::nullopt, manta::MantaStatusCode::kOk);
    }

    pending_fetch_thumbnails_callback_ = std::move(callback);

    fetch_thumbnails_timer_.Start(
        FROM_HERE, kRequestTimeout,
        base::BindOnce(&SeaPenFetcherImpl::OnFetchThumbnailsTimeout,
                       fetch_thumbnails_weak_ptr_factory_.GetWeakPtr(),
                       query->which()));

    const int num_outputs = query->is_text_query()
                                ? kNumTextThumbnailsRequested
                                : kNumTemplateThumbnailsRequested;
    manta::proto::Request request = CreateMantaRequest(
        query, std::nullopt, num_outputs, kDesiredThumbnailSize, feature_name);
    snapper_provider_->Call(
        request, TrafficAnnotationForFeature(feature_name),
        base::BindOnce(&SeaPenFetcherImpl::OnFetchThumbnailsDone,
                       fetch_thumbnails_weak_ptr_factory_.GetWeakPtr(),
                       base::TimeTicks::Now(), query.Clone()));
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
      CHECK_LE(query->get_text_query().size(),
               ash::personalization_app::mojom::
                   kMaximumGetSeaPenThumbnailsTextBytes);
    }

    fetch_wallpaper_timer_.Stop();
    fetch_wallpaper_weak_ptr_factory_.InvalidateWeakPtrs();

    if (pending_fetch_wallpaper_callback_) {
      std::move(pending_fetch_wallpaper_callback_).Run(std::nullopt);
    }

    pending_fetch_wallpaper_callback_ = std::move(callback);

    fetch_wallpaper_timer_.Start(
        FROM_HERE, kRequestTimeout,
        base::BindOnce(&SeaPenFetcherImpl::OnFetchWallpaperTimeout,
                       fetch_thumbnails_weak_ptr_factory_.GetWeakPtr(),
                       query->which()));

    manta::proto::Request request =
        CreateMantaRequest(query, thumbnail.id, /*num_outputs=*/1,
                           GetLargestDisplaySizeLandscape(), feature_name);
    snapper_provider_->Call(
        request, TrafficAnnotationForFeature(feature_name),
        base::BindOnce(&SeaPenFetcherImpl::OnFetchWallpaperDone,
                       fetch_wallpaper_weak_ptr_factory_.GetWeakPtr(),
                       base::TimeTicks::Now(), query->which()));
  }

 private:
  void OnFetchThumbnailsDone(
      const base::TimeTicks start_time,
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      std::unique_ptr<manta::proto::Response> response,
      manta::MantaStatus status) {
    DCHECK(pending_fetch_thumbnails_callback_);
    DCHECK(fetch_thumbnails_timer_.IsRunning());

    fetch_thumbnails_timer_.Stop();

    ash::personalization_app::mojom::SeaPenQuery::Tag query_tag =
        query->which();
    RecordSeaPenMantaStatusCode(query_tag, status.status_code,
                                SeaPenApiType::kThumbnails);

    if (status.status_code != manta::MantaStatusCode::kOk || !response) {
      LOG(WARNING) << "Failed to fetch manta response: " << status.message;
      std::move(pending_fetch_thumbnails_callback_)
          .Run(std::nullopt, status.status_code);
      return;
    }

    RecordSeaPenLatency(query_tag, base::TimeTicks::Now() - start_time,
                        SeaPenApiType::kThumbnails);
    RecordSeaPenTimeout(query_tag, /*hit_timeout=*/false,
                        SeaPenApiType::kThumbnails);

    std::unique_ptr<data_decoder::DataDecoder> data_decoder =
        std::make_unique<data_decoder::DataDecoder>();
    auto* data_decoder_pointer = data_decoder.get();

    const auto barrier_callback =
        base::BarrierCallback<std::optional<ash::SeaPenImage>>(
            response->output_data_size(),
            base::BindOnce(&SeaPenFetcherImpl::OnThumbnailsSanitized,
                           fetch_thumbnails_weak_ptr_factory_.GetWeakPtr(),
                           std::move(data_decoder), query_tag,
                           std::move(response->filtered_data())));

    for (auto& data : *response->mutable_output_data()) {
      SanitizeJpgBytes(data, data_decoder_pointer, barrier_callback);
    }
  }

  void OnThumbnailsSanitized(
      std::unique_ptr<data_decoder::DataDecoder> data_decoder,
      ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
      const ::google::protobuf::RepeatedPtrField<::manta::proto::FilteredData>&
          filtered_data,
      const std::vector<std::optional<ash::SeaPenImage>>& optional_images) {
    std::vector<ash::SeaPenImage> filtered_images =
        TakeValidImages(optional_images);

    RecordSeaPenThumbnailsCount(query_tag, filtered_images.size());

    if (filtered_images.empty()) {
      LOG(WARNING) << "Got empty images from thumbnails request";
      manta::MantaStatusCode status_code =
          GetMantaStatusCodeForEmptyImageResponse(query_tag, filtered_data);
      std::move(pending_fetch_thumbnails_callback_)
          .Run(std::nullopt, status_code);
      return;
    }

    std::move(pending_fetch_thumbnails_callback_)
        .Run(std::move(filtered_images), manta::MantaStatusCode::kOk);
  }

  void OnFetchThumbnailsTimeout(
      ash::personalization_app::mojom::SeaPenQuery::Tag query_tag) {
    DCHECK(pending_fetch_thumbnails_callback_);
    fetch_thumbnails_weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(pending_fetch_thumbnails_callback_)
        .Run(std::nullopt, manta::MantaStatusCode::kGenericError);
    RecordSeaPenTimeout(query_tag, /*hit_timeout=*/true,
                        SeaPenApiType::kThumbnails);
  }

  void OnFetchWallpaperDone(
      const base::TimeTicks start_time,
      ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
      std::unique_ptr<manta::proto::Response> response,
      manta::MantaStatus status) {
    DCHECK(pending_fetch_wallpaper_callback_);
    DCHECK(fetch_wallpaper_timer_.IsRunning());

    fetch_wallpaper_timer_.Stop();

    RecordSeaPenMantaStatusCode(query_tag, status.status_code,
                                SeaPenApiType::kWallpaper);

    if (status.status_code != manta::MantaStatusCode::kOk || !response) {
      LOG(WARNING) << "Failed to fetch manta response: " << status.message;
      std::move(pending_fetch_wallpaper_callback_).Run(std::nullopt);
      return;
    }

    RecordSeaPenLatency(query_tag, base::TimeTicks::Now() - start_time,
                        SeaPenApiType::kWallpaper);
    RecordSeaPenTimeout(query_tag, /*hit_timeout=*/false,
                        SeaPenApiType::kWallpaper);

    std::unique_ptr<data_decoder::DataDecoder> data_decoder =
        std::make_unique<data_decoder::DataDecoder>();
    auto* data_decoder_pointer = data_decoder.get();

    const auto barrier_callback =
        base::BarrierCallback<std::optional<ash::SeaPenImage>>(
            response->output_data_size(),
            base::BindOnce(&SeaPenFetcherImpl::OnWallpaperSanitized,
                           fetch_wallpaper_weak_ptr_factory_.GetWeakPtr(),
                           std::move(data_decoder), query_tag));

    for (auto& data : *response->mutable_output_data()) {
      SanitizeJpgBytes(data, data_decoder_pointer, barrier_callback);
    }
  }

  void OnWallpaperSanitized(
      std::unique_ptr<data_decoder::DataDecoder> data_decoder,
      ash::personalization_app::mojom::SeaPenQuery::Tag query_tag,
      const std::vector<std::optional<ash::SeaPenImage>>& optional_images) {
    std::vector<ash::SeaPenImage> images = TakeValidImages(optional_images);

    RecordSeaPenWallpaperHasImage(query_tag, !images.empty());

    if (images.empty()) {
      LOG(WARNING) << "Got empty images from upscale request";
      std::move(pending_fetch_wallpaper_callback_).Run(std::nullopt);
      return;
    }

    if (images.size() > 1) {
      LOG(WARNING) << "Got more than 1 output image";
    }

    std::move(pending_fetch_wallpaper_callback_).Run(std::move(images.at(0)));
  }

  void OnFetchWallpaperTimeout(
      ash::personalization_app::mojom::SeaPenQuery::Tag query_tag) {
    DCHECK(pending_fetch_wallpaper_callback_);
    fetch_wallpaper_weak_ptr_factory_.InvalidateWeakPtrs();
    RecordSeaPenTimeout(query_tag, /*hit_timeout=*/true,
                        SeaPenApiType::kWallpaper);
    std::move(pending_fetch_wallpaper_callback_).Run(std::nullopt);
  }

  OnFetchThumbnailsComplete pending_fetch_thumbnails_callback_;
  OnFetchWallpaperComplete pending_fetch_wallpaper_callback_;
  std::unique_ptr<manta::SnapperProvider> snapper_provider_;
  base::OneShotTimer fetch_thumbnails_timer_;
  base::OneShotTimer fetch_wallpaper_timer_;
  base::WeakPtrFactory<SeaPenFetcherImpl> fetch_thumbnails_weak_ptr_factory_{
      this};
  base::WeakPtrFactory<SeaPenFetcherImpl> fetch_wallpaper_weak_ptr_factory_{
      this};
};

}  // namespace

SeaPenFetcher::SeaPenFetcher() = default;

SeaPenFetcher::~SeaPenFetcher() = default;

std::unique_ptr<SeaPenFetcher> SeaPenFetcher::MakeSeaPenFetcher(
    std::unique_ptr<manta::SnapperProvider> snapper_provider) {
  return std::make_unique<SeaPenFetcherImpl>(std::move(snapper_provider));
}

}  // namespace wallpaper_handlers
