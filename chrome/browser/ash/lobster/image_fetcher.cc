// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/image_fetcher.h"

#include <string>

#include "ash/public/cpp/lobster/lobster_result.h"
#include "base/barrier_callback.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "components/manta/snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr gfx::Size kPreviewImageSize = gfx::Size(512, 512);
constexpr gfx::Size kFullImageSize = gfx::Size(1024, 1024);

manta::proto::Request CreateMantaRequest(std::string_view query,
                                         std::optional<uint32_t> seed,
                                         const gfx::Size& image_size,
                                         int num_outputs) {
  manta::proto::Request request;
  manta::proto::RequestConfig& request_config =
      *request.mutable_request_config();
  manta::proto::ImageDimensions& image_dimensions =
      *request_config.mutable_image_dimensions();
  manta::proto::InputData& input_data = *request.add_input_data();

  request_config.set_num_outputs(num_outputs);
  request.set_feature_name(manta::proto::FeatureName::CHROMEOS_LOBSTER);
  image_dimensions.set_width(image_size.width());
  image_dimensions.set_height(image_size.height());
  input_data.set_text(query.data(), query.size());

  if (seed.has_value()) {
    request_config.set_generation_seed(seed.value());
  }

  return request;
}

ash::LobsterErrorCode MantaToLobsterStatusCode(
    manta::MantaStatusCode manta_status_code) {
  switch (manta_status_code) {
    case manta::MantaStatusCode::kGenericError:
    case manta::MantaStatusCode::kMalformedResponse:
    case manta::MantaStatusCode::kNoIdentityManager:
      return ash::LobsterErrorCode::kUnknown;
    case manta::MantaStatusCode::kInvalidInput:
      return ash::LobsterErrorCode::kInvalidArgument;
    case manta::MantaStatusCode::kResourceExhausted:
    case manta::MantaStatusCode::kPerUserQuotaExceeded:
      return ash::LobsterErrorCode::kResourceExhausted;
    case manta::MantaStatusCode::kBackendFailure:
      return ash::LobsterErrorCode::kBackendFailure;
    case manta::MantaStatusCode::kNoInternetConnection:
      return ash::LobsterErrorCode::kNoInternetConnection;
    case manta::MantaStatusCode::kUnsupportedLanguage:
      return ash::LobsterErrorCode::kUnsupportedLanguage;
    case manta::MantaStatusCode::kBlockedOutputs:
      return ash::LobsterErrorCode::kBlockedOutputs;
    case manta::MantaStatusCode::kRestrictedCountry:
      return ash::LobsterErrorCode::kRestrictedRegion;
    case manta::MantaStatusCode::kOk:
      NOTREACHED_NORETURN();
  }
}

std::optional<ash::LobsterImageCandidate> ToLobsterImageCandidate(
    uint32_t id,
    uint32_t seed,
    const std::string& query,
    const SkBitmap& decoded_bitmap) {
  base::AssertLongCPUWorkAllowed();
  std::vector<unsigned char> data;

  if (!gfx::JPEGCodec::Encode(decoded_bitmap, /*quality=*/100, &data)) {
    return std::nullopt;
  }

  return ash::LobsterImageCandidate(/*id=*/id, /*image_bytes=*/
                                    std::string(data.begin(), data.end()),
                                    /*seed=*/seed,
                                    /*query=*/query.data());
}

void EncodeBitmap(
    uint32_t id,
    uint32_t seed,
    const std::string& query,
    base::OnceCallback<void(std::optional<ash::LobsterImageCandidate>)>
        callback,
    const SkBitmap& decoded_bitmap) {
  if (decoded_bitmap.empty()) {
    LOG(ERROR) << "Failed to decode jpg bytes";
    std::move(callback).Run(std::nullopt);
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ToLobsterImageCandidate, id, seed, query, decoded_bitmap),
      std::move(callback));
}

void SanitizePreviewJpgBytes(
    const manta::proto::OutputData& output_data,
    data_decoder::DataDecoder* data_decoder,
    uint32_t id,
    const std::string& query,
    base::OnceCallback<void(std::optional<ash::LobsterImageCandidate>)>
        callback) {
  data_decoder::DecodeImage(
      data_decoder, base::as_byte_span(output_data.image().serialized_bytes()),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&EncodeBitmap, id, output_data.generation_seed(), query,
                     std::move(callback)));
}

}  // namespace

ImageFetcher::ImageFetcher(manta::SnapperProvider* provider,
                           LobsterCandidateIdGenerator* id_generator)
    : provider_(provider), id_generator_(id_generator) {}

ImageFetcher::~ImageFetcher() = default;

void ImageFetcher::RequestCandidates(const std::string& query,
                                     int num_candidates,
                                     ash::RequestCandidatesCallback callback) {
  if (provider_ == nullptr) {
    LOG(ERROR) << "Provider is not available";
    std::move(callback).Run(base::unexpected(ash::LobsterError(
        /*status_code=*/MantaToLobsterStatusCode(
            manta::MantaStatusCode::kGenericError),
        /*message=*/"Provider is not available")));
    return;
  }

  auto request = CreateMantaRequest(/*query=*/query, /*seed=*/std::nullopt,
                                    /*image_size=*/kPreviewImageSize,
                                    /*num_outputs=*/num_candidates);
  // TODO(b:354620949): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  provider_->Call(request, MISSING_TRAFFIC_ANNOTATION,
                  base::BindOnce(&ImageFetcher::OnCandidatesRequested,
                                 weak_ptr_factory_.GetWeakPtr(), query,
                                 std::move(callback)));
}

void ImageFetcher::RequestFullSizeCandidate(
    const std::string& query,
    uint32_t seed,
    ash::RequestCandidatesCallback callback) {
  if (provider_ == nullptr) {
    LOG(ERROR) << "Provider is not available";
    std::move(callback).Run(base::unexpected(ash::LobsterError(
        /*status_code=*/MantaToLobsterStatusCode(
            manta::MantaStatusCode::kGenericError),
        /*message=*/"Provider is not available")));
    return;
  }

  auto request =
      CreateMantaRequest(/*query=*/query, /*seed=*/seed,
                         /*image_size=*/kFullImageSize, /*num_outputs=*/1);

  // TODO(b:354620949): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  provider_->Call(request, MISSING_TRAFFIC_ANNOTATION,
                  base::BindOnce(&ImageFetcher::OnCandidatesRequested,
                                 weak_ptr_factory_.GetWeakPtr(), query,
                                 std::move(callback)));
}

void ImageFetcher::OnCandidatesRequested(
    const std::string& query,
    ash::RequestCandidatesCallback callback,
    std::unique_ptr<manta::proto::Response> response,
    manta::MantaStatus status) {
  if (status.status_code != manta::MantaStatusCode::kOk) {
    std::move(callback).Run(base::unexpected(ash::LobsterError(
        /*status_code=*/MantaToLobsterStatusCode(status.status_code),
        /*message=*/status.message)));
    return;
  }

  std::unique_ptr<data_decoder::DataDecoder> data_decoder =
      std::make_unique<data_decoder::DataDecoder>();
  const auto barrier_callback =
      base::BarrierCallback<std::optional<ash::LobsterImageCandidate>>(
          response->output_data_size(),
          base::BindOnce(&ImageFetcher::OnImagesSanitized,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  for (auto& data : *response->mutable_output_data()) {
    SanitizePreviewJpgBytes(data, data_decoder.get(),
                            id_generator_->GenerateNextId(), query,
                            barrier_callback);
  }
}

void ImageFetcher::OnImagesSanitized(
    ash::RequestCandidatesCallback callback,
    const std::vector<std::optional<ash::LobsterImageCandidate>>&
        sanitized_image_candidates) {
  std::vector<ash::LobsterImageCandidate> image_candidates;

  for (auto& candidate : sanitized_image_candidates) {
    if (candidate.has_value()) {
      image_candidates.push_back(candidate.value());
    }
  }
  std::move(callback).Run(std::move(image_candidates));
}
