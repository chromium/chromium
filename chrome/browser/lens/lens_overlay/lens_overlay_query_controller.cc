// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_query_controller.h"

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_image_helper.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_proto_converter.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_url_builder.h"
#include "chrome/common/channel_info.h"
#include "components/lens/lens_features.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "components/version_info/channel.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/lens_server_proto/lens_overlay_platform.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_surface.pb.h"
#include "ui/gfx/geometry/rect.h"

namespace lens {

namespace {

// The name string for the header for variations information.
constexpr char kClientDataHeader[] = "X-Client-Data";
constexpr char kHttpMethod[] = "POST";
constexpr char kContentType[] = "application/x-protobuf";
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
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
          last_reviewed: "2024-04-11"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature is opt-in by flag only for now, so there "
            "is no setting to disable the feature."
          policy_exception_justification: "Policy not yet implemented."
        }
      )");

lens::CoordinateType ConvertToServerCoordinateType(
    lens::mojom::CenterRotatedBox_CoordinateType type) {
  switch (type) {
    case lens::mojom::CenterRotatedBox_CoordinateType::kNormalized:
      return lens::CoordinateType::NORMALIZED;
    case lens::mojom::CenterRotatedBox_CoordinateType::kImage:
      return lens::CoordinateType::IMAGE;
    case lens::mojom::CenterRotatedBox_CoordinateType::kUnspecified:
      return lens::CoordinateType::COORDINATE_TYPE_UNSPECIFIED;
  }
}

lens::CenterRotatedBox ConvertToServerCenterRotatedBox(
    lens::mojom::CenterRotatedBoxPtr box) {
  lens::CenterRotatedBox out_box;
  out_box.set_center_x(box->box.x());
  out_box.set_center_y(box->box.y());
  out_box.set_width(box->box.width());
  out_box.set_height(box->box.height());
  out_box.set_coordinate_type(
      ConvertToServerCoordinateType(box->coordinate_type));
  return out_box;
}

}  // namespace

LensOverlayQueryController::LensOverlayQueryController(
    LensOverlayFullImageResponseCallback full_image_callback,
    LensOverlayUrlResponseCallback url_callback,
    LensOverlayInteractionResponseCallback interaction_data_callback,
    LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager)
    : full_image_callback_(std::move(full_image_callback)),
      interaction_data_callback_(std::move(interaction_data_callback)),
      thumbnail_created_callback_(std::move(thumbnail_created_callback)),
      request_id_generator_(
          std::make_unique<lens::LensOverlayRequestIdGenerator>()),
      url_callback_(std::move(url_callback)),
      variations_client_(variations_client),
      identity_manager_{identity_manager} {}

LensOverlayQueryController::~LensOverlayQueryController() = default;

void LensOverlayQueryController::StartQueryFlow(const SkBitmap& screenshot) {
  DCHECK_EQ(query_controller_state_, QueryControllerState::kOff);
  query_controller_state_ = QueryControllerState::kAwaitingFullImageResponse;
  original_screenshot_ = screenshot;

  base::ThreadPool::PostTask(
      base::BindOnce(&DownscaleAndEncodeBitmap, screenshot)
          .Then(base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindOnce(&LensOverlayQueryController::FetchFullImageRequest,
                             weak_ptr_factory_.GetWeakPtr(),
                             request_id_generator_->GetNextRequestId()))));
}

lens::LensOverlayClientContext
LensOverlayQueryController::CreateClientContext() {
  lens::LensOverlayClientContext context;
  context.set_surface(lens::SURFACE_CHROMIUM);
  context.set_platform(lens::WEB);
  context.mutable_rendering_context()->set_rendering_environment(
      lens::RENDERING_ENV_LENS_OVERLAY);
  return context;
}

void LensOverlayQueryController::FetchFullImageRequest(
    std::unique_ptr<lens::LensOverlayRequestId> request_id,
    lens::ImageData image_data) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);
  // Create the request.
  lens::LensOverlayServerRequest request;
  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(*request_id.get());
  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  request.mutable_objects_request()->mutable_request_context()->CopyFrom(
      request_context);
  request.mutable_objects_request()->mutable_image_data()->CopyFrom(image_data);

  // Fetch the request.
  full_image_endpoint_fetcher_ = CreateAndFetchEndpointFetcher(
      request,
      base::BindOnce(&LensOverlayQueryController::FullImageFetchResponseHandler,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LensOverlayQueryController::FullImageFetchResponseHandler(
    std::unique_ptr<EndpointResponse> response) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kAwaitingFullImageResponse);

  CHECK(full_image_endpoint_fetcher_);
  full_image_endpoint_fetcher_.reset();
  query_controller_state_ = QueryControllerState::kReceivedFullImageResponse;

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    RunFullImageCallbackForError();
    return;
  }

  lens::LensOverlayServerResponse server_response;
  const std::string response_string = response->response;
  bool parse_successful = server_response.ParseFromArray(
      response_string.data(), response_string.size());
  if (!parse_successful) {
    RunFullImageCallbackForError();
    return;
  }

  if (!server_response.has_objects_response() ||
      !server_response.objects_response().has_cluster_info()) {
    RunFullImageCallbackForError();
    return;
  }

  cluster_info_ = std::make_optional<lens::LensOverlayClusterInfo>();
  cluster_info_->CopyFrom(server_response.objects_response().cluster_info());

  if (!cluster_info_received_callback_.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cluster_info_received_callback_),
                                  *cluster_info_));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          full_image_callback_,
          lens::CreateObjectsMojomArrayFromServerResponse(server_response),
          lens::CreateTextMojomFromServerResponse(server_response)));
}

void LensOverlayQueryController::RunFullImageCallbackForError() {
  ResetRequestFlowState();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(full_image_callback_,
                     std::vector<lens::mojom::OverlayObjectPtr>(), nullptr));
}

void LensOverlayQueryController::EndQuery() {
  full_image_endpoint_fetcher_.reset();
  interaction_endpoint_fetcher_.reset();
  cluster_info_received_callback_.Reset();
  cluster_info_ = std::nullopt;
  query_controller_state_ = QueryControllerState::kOff;
}

void LensOverlayQueryController::SendRegionSearch(
    lens::mojom::CenterRotatedBoxPtr region,
    std::map<std::string, std::string> additional_search_query_params) {
  SendInteraction(/*region=*/std::move(region), /*query_text=*/std::nullopt,
                  /*object_id=*/std::nullopt, additional_search_query_params);
}

void LensOverlayQueryController::SendObjectSelection(
    const std::string& object_id,
    std::map<std::string, std::string> additional_search_query_params) {
  SendInteraction(/*region=*/lens::mojom::CenterRotatedBoxPtr(),
                  /*query_text=*/std::nullopt,
                  /*object_id=*/std::make_optional<std::string>(object_id),
                  additional_search_query_params);
}

void LensOverlayQueryController::SendMultimodalRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& query_text,
    std::map<std::string, std::string> additional_search_query_params) {
  if (base::TrimWhitespaceASCII(query_text, base::TRIM_ALL).empty()) {
    return;
  }

  SendInteraction(/*region=*/std::move(region),
                  /*query_text=*/std::make_optional<std::string>(query_text),
                  /*object_id=*/std::nullopt, additional_search_query_params);
}

void LensOverlayQueryController::SendTextOnlyQuery(
    const std::string& query_text,
    std::map<std::string, std::string> additional_search_query_params) {
  // Increment the request counter to cancel previously issued fetches.
  request_counter_++;
  lens::proto::LensOverlayUrlResponse lens_overlay_url_response;
  lens_overlay_url_response.set_url(
      lens::BuildTextOnlySearchURL(query_text, additional_search_query_params)
          .spec());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(url_callback_, lens_overlay_url_response));
}

void LensOverlayQueryController::SendInteraction(
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<std::string> query_text,
    std::optional<std::string> object_id,
    std::map<std::string, std::string> additional_search_query_params) {
  request_counter_++;
  int request_index = request_counter_;

  // Trigger asynchronous image cropping, then attempt to send the request.
  base::ThreadPool::PostTask(
      base::BindOnce(&DownscaleAndEncodeBitmapRegionIfNeeded,
                     original_screenshot_, region.Clone())
          .Then(base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindOnce(
                  &LensOverlayQueryController::
                      FetchInteractionRequestAndGenerateUrlIfClusterInfoReady,
                  weak_ptr_factory_.GetWeakPtr(), request_index, region.Clone(),
                  query_text, object_id, additional_search_query_params))));
}

void LensOverlayQueryController::
    FetchInteractionRequestAndGenerateUrlIfClusterInfoReady(
        int request_index,
        lens::mojom::CenterRotatedBoxPtr region,
        std::optional<std::string> query_text,
        std::optional<std::string> object_id,
        std::map<std::string, std::string> additional_search_query_params,
        std::optional<lens::ImageCrop> image_crop) {
  if (cluster_info_.has_value()) {
    FetchInteractionRequestAndGenerateLensSearchUrl(
        request_index, std::move(region), query_text, object_id,
        additional_search_query_params, image_crop, *cluster_info_);
  } else {
    cluster_info_received_callback_ = base::BindOnce(
        &LensOverlayQueryController::
            FetchInteractionRequestAndGenerateLensSearchUrl,
        weak_ptr_factory_.GetWeakPtr(), request_index, std::move(region),
        query_text, object_id, additional_search_query_params, image_crop);
  }

  if (image_crop.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(thumbnail_created_callback_,
                                  image_crop->image().image_content()));
  }
}

lens::LensOverlayServerRequest
LensOverlayQueryController::CreateInteractionRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<std::string> query_text,
    std::optional<std::string> object_id,
    std::optional<lens::ImageCrop> image_crop,
    std::unique_ptr<lens::LensOverlayRequestId> request_id) {
  lens::LensOverlayServerRequest server_request;
  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(*request_id.get());
  request_context.mutable_client_context()->CopyFrom(CreateClientContext());
  server_request.mutable_interaction_request()
      ->mutable_request_context()
      ->CopyFrom(request_context);

  lens::LensOverlayInteractionRequestMetadata interaction_request_metadata;
  if (!region.is_null() && image_crop.has_value()) {
    // Add the region for region search and multimodal requests.
    server_request.mutable_interaction_request()
        ->mutable_image_crop()
        ->CopyFrom(*image_crop);
    interaction_request_metadata.set_type(
        lens::LensOverlayInteractionRequestMetadata::REGION);
    interaction_request_metadata.mutable_selection_metadata()
        ->mutable_region()
        ->mutable_region()
        ->CopyFrom(ConvertToServerCenterRotatedBox(region.Clone()));

    // Add the text, for multimodal requests.
    if (query_text.has_value()) {
      interaction_request_metadata.mutable_query_metadata()
          ->mutable_text_query()
          ->set_query(*query_text);
    }
  } else if (object_id.has_value()) {
    // Add object request details.
    interaction_request_metadata.set_type(
        lens::LensOverlayInteractionRequestMetadata::TAP);
    interaction_request_metadata.mutable_selection_metadata()
        ->mutable_object()
        ->set_object_id(*object_id);
  } else {
    // There should be a region or an object id in the request.
    NOTREACHED();
  }

  server_request.mutable_interaction_request()
      ->mutable_interaction_request_metadata()
      ->CopyFrom(interaction_request_metadata);
  return server_request;
}

void LensOverlayQueryController::
    FetchInteractionRequestAndGenerateLensSearchUrl(
        int request_index,
        lens::mojom::CenterRotatedBoxPtr region,
        std::optional<std::string> query_text,
        std::optional<std::string> object_id,
        std::map<std::string, std::string> additional_search_query_params,
        std::optional<lens::ImageCrop> image_crop,
        lens::LensOverlayClusterInfo cluster_info) {
  if (request_index != request_counter_) {
    // Early exit if this is an old request.
    return;
  }
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kReceivedFullImageResponse);

  // Fetch the interaction request.
  lens::LensOverlayServerRequest server_request = CreateInteractionRequest(
      std::move(region), query_text, object_id, image_crop,
      request_id_generator_->GetNextRequestId());
  interaction_endpoint_fetcher_ = CreateAndFetchEndpointFetcher(
      server_request,
      base::BindOnce(
          &LensOverlayQueryController::InteractionFetchResponseHandler,
          weak_ptr_factory_.GetWeakPtr()));

  // Generate and send the Lens search url.
  lens::proto::LensOverlayUrlResponse lens_overlay_url_response;
  lens_overlay_url_response.set_url(
      lens::BuildLensSearchURL(query_text,
                               request_id_generator_->GetNextRequestId(),
                               cluster_info, additional_search_query_params)
          .spec());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(url_callback_, lens_overlay_url_response));
}

void LensOverlayQueryController::InteractionFetchResponseHandler(
    std::unique_ptr<EndpointResponse> response) {
  DCHECK_EQ(query_controller_state_,
            QueryControllerState::kReceivedFullImageResponse);
  // TODO(b/331501820): Add retry logic using a timeout to clear the request
  // flow state.
  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    RunInteractionCallbackForError();
    return;
  }

  lens::LensOverlayServerResponse server_response;
  const std::string response_string = response->response;
  bool parse_successful = server_response.ParseFromArray(
      response_string.data(), response_string.size());
  if (!parse_successful) {
    RunInteractionCallbackForError();
    return;
  }

  if (!server_response.has_interaction_response()) {
    RunInteractionCallbackForError();
    return;
  }

  lens::proto::LensOverlayInteractionResponse lens_overlay_interaction_response;
  lens_overlay_interaction_response.set_suggest_signals(
      server_response.interaction_response().encoded_response());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(interaction_data_callback_,
                                lens_overlay_interaction_response));
}

void LensOverlayQueryController::RunInteractionCallbackForError() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(interaction_data_callback_,
                                lens::proto::LensOverlayInteractionResponse()));
}

void LensOverlayQueryController::ResetRequestFlowState() {
  cluster_info_received_callback_.Reset();
  interaction_endpoint_fetcher_.reset();
  cluster_info_ = std::nullopt;
  request_id_generator_->ResetRequestId();
}

std::unique_ptr<EndpointFetcher>
LensOverlayQueryController::CreateAndFetchEndpointFetcher(
    lens::LensOverlayServerRequest request_data,
    EndpointFetcherCallback callback) {
  std::string request_data_string;
  CHECK(request_data.SerializeToString(&request_data_string));
  std::vector<std::string> cors_exempt_headers;

  variations::mojom::VariationsHeadersPtr headers =
      variations_client_->GetVariationsHeaders();
  if (!headers.is_null()) {
    cors_exempt_headers.push_back(kClientDataHeader);
    // The endpoint is always a Google property.
    cors_exempt_headers.push_back(headers->headers_map.at(
        variations::mojom::GoogleWebVisibility::FIRST_PARTY));
  }

  GURL fetch_url = GURL(lens::features::GetLensOverlayEndpointURL());
  if (cluster_info_.has_value()) {
    // The endpoint fetches should use the server session id from the cluster
    // info.
    fetch_url = net::AppendOrReplaceQueryParameter(
        fetch_url, kSessionIdQueryParameterKey,
        cluster_info_->server_session_id());
  }

  if (lens::features::UseOauthForLensOverlayRequests() && identity_manager_) {
    // Make a request with oauth.
    // TODO(b/335241935): Manually fetch the oauth token and use the
    // EndpointFetcher constructor that supports setting cors_exempt_headers.
    std::unique_ptr<EndpointFetcher> endpoint_fetcher =
        std::make_unique<EndpointFetcher>(
            /*url_loader_factory=*/g_browser_process
                ->shared_url_loader_factory(),
            /*oauth_consumer_name=*/"LensOverlayQueryController",
            /*url=*/fetch_url,
            /*http_method=*/kHttpMethod,
            /*content_type=*/kContentType,
            /*scopes=*/
            std::vector<std::string>{"https://www.googleapis.com/auth/lens"},
            /*timeout=*/kServerRequestTimeout,
            /*post_data=*/request_data_string,
            /*annotation_tag=*/kTrafficAnnotationTag,
            /*identity_manager=*/identity_manager_,
            /*consent_level=*/signin::ConsentLevel::kSignin);
    endpoint_fetcher.get()->Fetch(std::move(callback));
    return endpoint_fetcher;
  } else {
    // Make a request with the api key.
    std::unique_ptr<EndpointFetcher> endpoint_fetcher =
        std::make_unique<EndpointFetcher>(
            /*url_loader_factory=*/g_browser_process
                ->shared_url_loader_factory(),
            /*url=*/fetch_url,
            /*http_method=*/kHttpMethod,
            /*content_type=*/kContentType,
            /*timeout=*/kServerRequestTimeout,
            /*post_data=*/request_data_string,
            /*headers=*/std::vector<std::string>(),
            /*cors_exempt_headers=*/cors_exempt_headers,
            /*annotation_tag=*/kTrafficAnnotationTag,
            /*is_stable_channel=*/chrome::GetChannel() ==
                version_info::Channel::STABLE);
    endpoint_fetcher.get()->PerformRequest(std::move(callback),
                                           google_apis::GetAPIKey().c_str());
    return endpoint_fetcher;
  }
}

}  // namespace lens
