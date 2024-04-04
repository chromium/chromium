// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_query_controller.h"

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "components/lens/lens_features.h"
#include "components/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"

namespace lens {

namespace {

constexpr char kHttpMethod[] = "POST";
constexpr char kContentType[] = "application/x-protobuf";
constexpr base::TimeDelta kServerRequestTimeout = base::Minutes(1);

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("lens_overlay", R"(
        semantics {
          sender: "Lens"
          description: "A request to the service handling the Lens "
            "Overlay feature in Chrome."
          trigger: "The user triggered a Lens Overlay Flow by entering "
            "the experience via the right click menu option for "
            "searching images on the page."
          data: "Image and user interaction data. Only the screenshot "
            "of the current webpage viewport (image bytes) and user "
            "interaction data (coordinates of a box within the "
            "screenshot or tapped object-id) are sent."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "hujasonx@google.com"
            }
            contacts {
              email: "lens-chrome@google.com"
            }
          }
          user_data {
            type: USER_CONTENT
            type: WEB_CONTENT
          }
          last_reviewed: "2024-04-02"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature is opt-in by flag only for now, so there "
            "is no setting to disable the feature."
          policy_exception_justification: "Policy not yet implemented."
        }
      )");
}  // namespace

LensOverlayQueryController::LensOverlayQueryController(
    base::RepeatingCallback<void(lens::proto::LensOverlayFullImageResponse)>
        full_image_callback,
    base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>
        url_callback,
    base::RepeatingCallback<void(lens::proto::LensOverlayInteractionResponse)>
        interaction_data_callback)
    : request_id_generator_{std::make_unique<
          lens::LensOverlayRequestIdGenerator>()},
      full_image_callback_{full_image_callback},
      url_callback_{url_callback},
      interaction_data_callback_{interaction_data_callback} {}

LensOverlayQueryController::~LensOverlayQueryController() = default;

void LensOverlayQueryController::StartQueryFlow(const SkBitmap& screenshot) {
  DCHECK_EQ(query_controller_state_, QueryControllerState::kOff);
  query_controller_state_ = QueryControllerState::kAwaitingFullImageResponse;
  original_screenshot_ = screenshot;

  LensOverlayQueryController::CreateFullImageData(
      base::BindOnce(&LensOverlayQueryController::FetchFullImageRequest,
                     weak_ptr_factory_.GetWeakPtr(),
                     request_id_generator_->GetNextRequestId()));
}

void LensOverlayQueryController::CreateFullImageData(
    base::OnceCallback<void()> callback) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);
  // TODO(b/328297337): Add the downscaled and encoded image data to the
  // request.
  full_image_data_.Clear();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback)));
}

void LensOverlayQueryController::FetchFullImageRequest(
    std::unique_ptr<lens::LensOverlayRequestId> request_id) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);
  // Create the request.
  lens::LensOverlayServerRequest request;
  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(*request_id.get());
  request.mutable_objects_request()->mutable_request_context()->CopyFrom(
      request_context);
  request.mutable_objects_request()->mutable_image_data()->CopyFrom(
      full_image_data_);

  // Fetch the request.
  full_image_endpoint_fetcher_ = CreateEndpointFetcher(request);
  full_image_endpoint_fetcher_.get()->PerformRequest(
      base::BindOnce(&LensOverlayQueryController::FullImageFetchResponseHandler,
                     weak_ptr_factory_.GetWeakPtr()),
      google_apis::GetAPIKey().c_str());
}

void LensOverlayQueryController::FullImageFetchResponseHandler(
    std::unique_ptr<EndpointResponse> response) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);

  query_controller_state_ = QueryControllerState::kReceivedFullImageResponse;
  if (has_pending_interaction_) {
    LensOverlayQueryController::FetchInteractionRequest(
        request_id_generator_->GetNextRequestId());
  }
  if (has_pending_url_change_) {
    LensOverlayQueryController::GenerateAndSendSearchUrl(
        request_id_generator_->GetNextRequestId());
  }

  lens::proto::LensOverlayFullImageResponse lens_overlay_full_image_response;

  // TODO(b/331488406): Set the overlay response data.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(full_image_callback_, lens_overlay_full_image_response));
}

void LensOverlayQueryController::EndQuery() {
  full_image_endpoint_fetcher_.reset();
  interaction_endpoint_fetcher_.reset();
  query_controller_state_ = QueryControllerState::kOff;
}

void LensOverlayQueryController::SendInteraction(
    lens::proto::LensOverlayRequest request) {
  LensOverlayQueryController::CropImageForInteraction(
      request,
      base::BindOnce(&LensOverlayQueryController::MaybeFetchInteractionRequest,
                     weak_ptr_factory_.GetWeakPtr(),
                     request_id_generator_->GetNextRequestId()));
  if (query_controller_state_ ==
      QueryControllerState::kReceivedFullImageResponse) {
    LensOverlayQueryController::GenerateAndSendSearchUrl(
        request_id_generator_->GetNextRequestId());
  } else {
    has_pending_url_change_ = true;
  }
}

void LensOverlayQueryController::CropImageForInteraction(
    lens::proto::LensOverlayRequest request,
    base::OnceCallback<void()> callback) {
  // TODO(b/328297337): Add the downscaled and encoded image data to the
  // request.
  cropped_image_data_.Clear();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback)));
}

void LensOverlayQueryController::MaybeFetchInteractionRequest(
    std::unique_ptr<lens::LensOverlayRequestId> request_id) {
  if (query_controller_state_ ==
      QueryControllerState::kAwaitingFullImageResponse) {
    has_pending_interaction_ = true;
    return;
  }
  LensOverlayQueryController::FetchInteractionRequest(std::move(request_id));
}

void LensOverlayQueryController::FetchInteractionRequest(
    std::unique_ptr<lens::LensOverlayRequestId> request_id) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kReceivedFullImageResponse);
  has_pending_interaction_ = false;

  // Create the request.
  lens::LensOverlayServerRequest request;
  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(*request_id.get());
  lens::LensOverlayInteractionRequestMetadata interaction_request_metadata;
  // TODO(b/331488406): Fill the interaction request metadata using
  // cropped_image_data_ and last_interaction_data_.

  request.mutable_interaction_request()->mutable_request_context()->CopyFrom(
      request_context);
  request.mutable_interaction_request()
      ->mutable_interaction_request_metadata()
      ->CopyFrom(interaction_request_metadata);

  // Fetch the request.
  interaction_endpoint_fetcher_ = CreateEndpointFetcher(request);
  interaction_endpoint_fetcher_.get()->PerformRequest(
      base::BindOnce(
          &LensOverlayQueryController::InteractionFetchResponseHandler,
          weak_ptr_factory_.GetWeakPtr()),
      google_apis::GetAPIKey().c_str());
}

void LensOverlayQueryController::InteractionFetchResponseHandler(
    std::unique_ptr<EndpointResponse> response) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kReceivedFullImageResponse);
  // TODO(b/331501820): Add retry logic and set query_controller_state_ back to
  // kAwaitingFullImageResponse based on the response status, as that
  // indicates that the data on the server is no longer available.

  // TODO(b/331488406): Use real data.
  lens::proto::LensOverlayInteractionResponse lens_overlay_interaction_response;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(interaction_data_callback_,
                                lens_overlay_interaction_response));
}

void LensOverlayQueryController::GenerateAndSendSearchUrl(
    std::unique_ptr<lens::LensOverlayRequestId> request_id) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kReceivedFullImageResponse);
  has_pending_url_change_ = false;
  // TODO: use the request id for generating the url.
  lens::proto::LensOverlayUrlResponse lens_overlay_url_response;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(url_callback_, lens_overlay_url_response));
}

std::unique_ptr<EndpointFetcher>
LensOverlayQueryController::CreateEndpointFetcher(
    lens::LensOverlayServerRequest request_data) {
  std::string request_data_string;
  CHECK(request_data.SerializeToString(&request_data_string));
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/g_browser_process->shared_url_loader_factory(),
      /*url=*/GURL(lens::features::GetLensOverlayEndpointURL()),
      /*http_method=*/kHttpMethod,
      /*content_type=*/kContentType,
      /*timeout=*/kServerRequestTimeout,
      /*post_data=*/request_data_string,
      /*headers=*/std::vector<std::string>(),
      /*annotation_tag=*/kTrafficAnnotationTag,
      /*is_stable_channel=*/chrome::GetChannel() ==
          version_info::Channel::STABLE);
}

}  // namespace lens
