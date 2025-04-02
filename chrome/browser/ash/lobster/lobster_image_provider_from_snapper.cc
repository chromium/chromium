// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_image_provider_from_snapper.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/lobster/lobster_result.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/barrier_callback.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
namespace {

constexpr gfx::Size kPreviewImageSize = gfx::Size(512, 512);
constexpr gfx::Size kFullImageSize = gfx::Size(1024, 1024);
constexpr char kLobsterUseQueryRewriterFlag[] = "use_query_rewrite";
constexpr char kLobsterI18nFlag[] = "use_i18n";
constexpr auto kLobsterTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chromeos_inline_image_request", R"(
      semantics {
        sender: "ChromeOS Inline Image"
        description:
          "Requests inline images from Google's servers. Google returns "
          "suggested images which users may choose to insert into selected "
          "text field, or download into Downloads folder."
        trigger:
          "User right clicks in an editable text field or triggers "
          "Quick Insert and select Inline Image option."
        internal {
          contacts {
            email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: USER_CONTENT
        }
        data:
          "A free-form user query. Query metadata includes a flag to indicate "
          "if user wants to get their query rewritten, and is also sent."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2025-03-14"
      }
      policy {
        cookies_allowed: NO
        setting:
          "No setting. Users must take explicit action to trigger the feature."
        policy_exception_justification:
          "Not implemented, not considered useful. This request is part of a "
          "flow which is user-initiated."
      }
    )");

manta::proto::Request CreateMantaRequest(std::string_view query,
                                         std::optional<uint32_t> seed,
                                         const gfx::Size& image_size,
                                         int num_outputs) {
  manta::proto::Request request;
  manta::proto::RequestConfig& request_config =
      *request.mutable_request_config();
  manta::proto::ImageDimensions& image_dimensions =
      *request_config.mutable_image_dimensions();
  manta::proto::InputData& query_input_data = *request.add_input_data();

  request_config.set_num_outputs(num_outputs);
  request.set_feature_name(manta::proto::FeatureName::CHROMEOS_LOBSTER);
  image_dimensions.set_width(image_size.width());
  image_dimensions.set_height(image_size.height());
  query_input_data.set_text(query.data(), query.size());

  if (seed.has_value()) {
    request_config.set_generation_seed(seed.value());
  }

  manta::proto::InputData& query_rewritter_input_data =
      *request.add_input_data();
  query_rewritter_input_data.set_tag(kLobsterUseQueryRewriterFlag);
  query_rewritter_input_data.set_text(
      ash::features::IsLobsterUseRewrittenQuery() ? "true" : "false");

  manta::proto::InputData& i18n_flag_input_data = *request.add_input_data();
  i18n_flag_input_data.set_tag(kLobsterI18nFlag);
  i18n_flag_input_data.set_text(
      ash::features::IsLobsterI18nEnabled() ? "true" : "false");

  return request;
}

ash::LobsterErrorCode MantaToLobsterStatusCode(
    manta::MantaStatusCode manta_status_code) {
  switch (manta_status_code) {
    case manta::MantaStatusCode::kGenericError:
    case manta::MantaStatusCode::kMalformedResponse:
    case manta::MantaStatusCode::kNoIdentityManager:
      return ash::LobsterErrorCode::kUnknown;
    case manta::MantaStatusCode::kImageHasPerson:
      return ash::LobsterErrorCode::kContainsPeople;
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
      NOTREACHED();
  }
}

std::string GetErrorMessage(ash::LobsterErrorCode lobster_error_code) {
  switch (lobster_error_code) {
    case ash::LobsterErrorCode::kNoInternetConnection:
      return l10n_util::GetStringUTF8(IDS_LOBSTER_NO_INTERNET_ERROR_MESSAGE);
    case ash::LobsterErrorCode::kBlockedOutputs:
      return l10n_util::GetStringUTF8(
          IDS_LOBSTER_CONTROVERSIAL_RESPONSE_ERROR_MESSAGE);
    case ash::LobsterErrorCode::kUnknown:
      return l10n_util::GetStringUTF8(
          IDS_LOBSTER_NO_SERVER_RESPONSE_ERROR_MESSAGE);
    case ash::LobsterErrorCode::kResourceExhausted:
      return l10n_util::GetStringUTF8(
          IDS_LOBSTER_OUT_OF_RESOURCE_ERROR_MESSAGE);
    case ash::LobsterErrorCode::kInvalidArgument:
      return l10n_util::GetStringUTF8(
          IDS_LOBSTER_NO_SERVER_RESPONSE_ERROR_MESSAGE);
    case ash::LobsterErrorCode::kBackendFailure:
      return l10n_util::GetStringUTF8(
          IDS_LOBSTER_NO_SERVER_RESPONSE_ERROR_MESSAGE);
    case ash::LobsterErrorCode::kUnsupportedLanguage:
      return l10n_util::GetStringUTF8(
          IDS_LOBSTER_UNSUPPORTED_LANGUAGE_ERROR_MESSAGE);
    case ash::LobsterErrorCode::kRestrictedRegion:
      return l10n_util::GetStringUTF8(
          IDS_LOBSTER_NO_SERVER_RESPONSE_ERROR_MESSAGE);
    case ash::LobsterErrorCode::kContainsPeople:
      return l10n_util::GetStringUTF8(
          IDS_LOBSTER_CONTAINS_PERSON_ERROR_MESSAGE);
  }
}

std::optional<ash::LobsterImageCandidate> ToLobsterImageCandidate(
    uint32_t id,
    uint32_t seed,
    const std::string& user_query,
    const std::string& rewritten_query,
    const SkBitmap& decoded_bitmap) {
  base::AssertLongCPUWorkAllowed();

  std::optional<std::vector<uint8_t>> data =
      gfx::JPEGCodec::Encode(decoded_bitmap, /*quality=*/100);
  if (!data) {
    return std::nullopt;
  }

  return ash::LobsterImageCandidate(
      /*id=*/id, /*image_bytes=*/
      std::string(base::as_string_view(data.value())),
      /*seed=*/seed,
      /*user_query=*/user_query,
      /*rewritten_query=*/rewritten_query);
}

void EncodeBitmap(
    uint32_t id,
    uint32_t seed,
    const std::string& user_query,
    const std::string& rewritten_query,
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
      base::BindOnce(&ToLobsterImageCandidate, id, seed, user_query,
                     rewritten_query, decoded_bitmap),
      std::move(callback));
}

void SanitizePreviewJpgBytes(
    const manta::proto::OutputData& output_data,
    data_decoder::DataDecoder* data_decoder,
    uint32_t id,
    const std::string& user_query,
    base::OnceCallback<void(std::optional<ash::LobsterImageCandidate>)>
        callback) {
  data_decoder::DecodeImage(
      data_decoder, base::as_byte_span(output_data.image().serialized_bytes()),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&EncodeBitmap, id, output_data.generation_seed(),
                     user_query,
                     // if the `generative_prompt`is populated with the
                     // rewritten query if query rewritter is enabled, and is
                     // populated with the original query otherwise.
                     output_data.generative_prompt(), std::move(callback)));
}

}  // namespace

LobsterImageProviderFromSnapper::LobsterImageProviderFromSnapper(
    manta::SnapperProvider* provider,
    LobsterCandidateIdGenerator* id_generator)
    : provider_(provider), id_generator_(id_generator) {}

LobsterImageProviderFromSnapper::~LobsterImageProviderFromSnapper() = default;

void LobsterImageProviderFromSnapper::RequestMultipleCandidates(
    const std::string& query,
    int num_candidates,
    ash::RequestCandidatesCallback callback) {
  if (provider_ == nullptr) {
    LOG(ERROR) << "Provider is not available";
    ash::LobsterErrorCode status_code =
        MantaToLobsterStatusCode(manta::MantaStatusCode::kGenericError);
    std::move(callback).Run(base::unexpected(ash::LobsterError(
        /*status_code=*/status_code,
        /*message=*/GetErrorMessage(status_code))));
    return;
  }

  auto request = CreateMantaRequest(/*query=*/query, /*seed=*/std::nullopt,
                                    /*image_size=*/kPreviewImageSize,
                                    /*num_outputs=*/num_candidates);
  provider_->Call(
      request, kLobsterTrafficAnnotation,
      base::BindOnce(&LobsterImageProviderFromSnapper::OnCandidatesRequested,
                     weak_ptr_factory_.GetWeakPtr(), query,
                     std::move(callback)));
}

void LobsterImageProviderFromSnapper::RequestSingleCandidateWithSeed(
    const std::string& query,
    uint32_t seed,
    ash::RequestCandidatesCallback callback) {
  if (provider_ == nullptr) {
    LOG(ERROR) << "Provider is not available";
    ash::LobsterErrorCode status_code =
        MantaToLobsterStatusCode(manta::MantaStatusCode::kGenericError);
    std::move(callback).Run(base::unexpected(ash::LobsterError(
        /*status_code=*/status_code,
        /*message=*/GetErrorMessage(status_code))));
    return;
  }

  auto request =
      CreateMantaRequest(/*query=*/query, /*seed=*/seed,
                         /*image_size=*/kFullImageSize, /*num_outputs=*/1);
  provider_->Call(
      request, kLobsterTrafficAnnotation,
      base::BindOnce(&LobsterImageProviderFromSnapper::OnCandidatesRequested,
                     weak_ptr_factory_.GetWeakPtr(), query,
                     std::move(callback)));
}

void LobsterImageProviderFromSnapper::OnCandidatesRequested(
    const std::string& query,
    ash::RequestCandidatesCallback callback,
    std::unique_ptr<manta::proto::Response> response,
    manta::MantaStatus status) {
  if (status.status_code != manta::MantaStatusCode::kOk) {
    ash::LobsterErrorCode error_code =
        MantaToLobsterStatusCode(status.status_code);
    std::move(callback).Run(base::unexpected(
        ash::LobsterError(error_code,
                          /*message=*/GetErrorMessage(error_code))));
    return;
  }

  std::unique_ptr<data_decoder::DataDecoder> data_decoder =
      std::make_unique<data_decoder::DataDecoder>();
  const auto barrier_callback =
      base::BarrierCallback<std::optional<ash::LobsterImageCandidate>>(
          response->output_data_size(),
          base::BindOnce(&LobsterImageProviderFromSnapper::OnImagesSanitized,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(response->filtered_data()),
                         std::move(callback)));

  for (auto& data : *response->mutable_output_data()) {
    SanitizePreviewJpgBytes(data, data_decoder.get(),
                            id_generator_->GenerateNextId(), query,
                            barrier_callback);
  }
}

void LobsterImageProviderFromSnapper::OnImagesSanitized(
    const ::google::protobuf::RepeatedPtrField<::manta::proto::FilteredData>&
        filtered_data,
    ash::RequestCandidatesCallback callback,
    const std::vector<std::optional<ash::LobsterImageCandidate>>&
        sanitized_image_candidates) {
  std::vector<ash::LobsterImageCandidate> image_candidates;

  for (auto& candidate : sanitized_image_candidates) {
    if (candidate.has_value()) {
      image_candidates.push_back(candidate.value());
    }
  }

  if (!image_candidates.empty()) {
    std::move(callback).Run(std::move(image_candidates));
    return;
  }

  // If there is no valid image, returns errors based on the filtered reasons.
  for (auto& filtered_datum : filtered_data) {
    if (filtered_datum.additional_reasons_size() > 0) {
      if (std::find(filtered_datum.additional_reasons().begin(),
                    filtered_datum.additional_reasons().end(),
                    manta::proto::FilteredReason::IMAGE_SAFETY_PERSON) !=
          filtered_datum.additional_reasons().end()) {
        std::move(callback).Run(base::unexpected(ash::LobsterError(
            /*status_code=*/ash::LobsterErrorCode::kContainsPeople,
            /*message=*/GetErrorMessage(
                ash::LobsterErrorCode::kContainsPeople))));
        return;
      }
    }
  }
  // All the images were filtered out due to our safety filters.
  std::move(callback).Run(base::unexpected(ash::LobsterError(
      /*status_code=*/ash::LobsterErrorCode::kBlockedOutputs,
      GetErrorMessage(ash::LobsterErrorCode::kBlockedOutputs))));
}
