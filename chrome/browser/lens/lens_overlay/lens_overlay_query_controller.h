// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_
#define CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_

#include "base/functional/callback.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_request_id_generator.h"
#include "chrome/browser/resources/lens/server/proto/lens_overlay_response.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "third_party/lens_server_proto/lens_overlay_client_context.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_interaction_request_metadata.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

class Profile;

namespace lens {

// Callback type alias for the lens overlay full image response.
using LensOverlayFullImageResponseCallback =
    base::RepeatingCallback<void(std::vector<lens::mojom::OverlayObjectPtr>,
                                 lens::mojom::TextPtr)>;
// Callback type alias for the lens overlay url response.
using LensOverlayUrlResponseCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>;
// Callback type alias for the lens overlay interaction data response.
using LensOverlayInteractionResponseCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlayInteractionResponse)>;
// Manages queries on behalf of a Lens overlay.
class LensOverlayQueryController {
 public:
  explicit LensOverlayQueryController(
      LensOverlayFullImageResponseCallback full_image_callback,
      base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>
          url_callback,
      base::RepeatingCallback<void(lens::proto::LensOverlayInteractionResponse)>
          interaction_data_callback,
      Profile* profile);
  virtual ~LensOverlayQueryController();

  // Starts a query flow by sending a request to Lens using the screenshot,
  // returning the response to the full image callback. Should be called
  // exactly once.
  void StartQueryFlow(const SkBitmap& screenshot);

  // Clears the state and resets stored values.
  void EndQuery();

  // Sends a region search interaction. Expected to be called multiple times.
  void SendRegionSearch(lens::mojom::CenterRotatedBoxPtr region);

  // Sends an object selection interaction. Expected to be called multiple
  // times.
  void SendObjectSelection(const std::string& object_id);

  // Sends a text-only interaction. Expected to be called multiple times.
  void SendTextOnlyQuery(const std::string& query_text);

  // Sends a multimodal interaction. Expected to be called multiple times.
  void SendMultimodalRequest(lens::mojom::CenterRotatedBoxPtr region,
                             const std::string& query_text);

 protected:
  // Creates an endpoint fetcher for fetching the request data.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      lens::LensOverlayServerRequest request_data);

 private:
  enum class QueryControllerState {
    // StartQueryFlow has not been called and the query controller is inactive.
    kOff = 0,
    // The full image response has not been received, or is no longer valid.
    kAwaitingFullImageResponse = 1,
    // The full image response has been received and the query controller can
    // send interaction requests.
    kReceivedFullImageResponse = 2,
  };

  // Creates a client context proto to be attached to a server request.
  lens::LensOverlayClientContext CreateClientContext();

  // Sends the interaction data, triggering async image cropping and fetching
  // the request.
  void SendInteraction(lens::mojom::CenterRotatedBoxPtr region,
                       std::optional<std::string> query_text,
                       std::optional<std::string> object_id);

  // Fetches the endpoint using the initial image data.
  void FetchFullImageRequest(
      std::unique_ptr<lens::LensOverlayRequestId> request_id,
      lens::ImageData image_data);

  // Handles the endpoint fetch response for the initial request.
  void FullImageFetchResponseHandler(
      std::unique_ptr<EndpointResponse> response);

  // Runs the full image callback with empty response data, for errors.
  void RunFullImageCallbackForError();

  // Handles the endpoint fetch response for an interaction request.
  void InteractionFetchResponseHandler(
      std::unique_ptr<EndpointResponse> response);

  // Runs the interaction callback with empty response data, for errors.
  void RunInteractionCallbackForError();

  // Helper to gate interaction fetches on whether or not the cluster
  // info has been received. If it has not been received, this function
  // sets the cluster info received callback to fetch the interaction.
  void FetchInteractionRequestAndGenerateUrlIfClusterInfoReady(
      int request_index,
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      std::optional<lens::ImageCrop> image_crop);

  // Fetches the endpoint for an interaction request and creates a Lens search
  // url if the request is the most recent request.
  void FetchInteractionRequestAndGenerateLensSearchUrl(
      int request_index,
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      std::optional<lens::ImageCrop> image_crop,
      lens::LensOverlayClusterInfo cluster_info);

  // Creates the metadata for an interaction request using the latest
  // interaction and image crop data.
  lens::LensOverlayServerRequest CreateInteractionRequest(
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      std::optional<lens::ImageCrop> image_crop,
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // Resets the request flow state.
  void ResetRequestFlowState();

  // The request id generator.
  std::unique_ptr<lens::LensOverlayRequestIdGenerator> request_id_generator_;

  // The original screenshot image.
  SkBitmap original_screenshot_;

  // The current state.
  QueryControllerState query_controller_state_ = QueryControllerState::kOff;

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  LensOverlayFullImageResponseCallback full_image_callback_;

  // Url callback for an interaction.
  LensOverlayUrlResponseCallback url_callback_;

  // Interaction data callback for an interaction.
  LensOverlayInteractionResponseCallback interaction_data_callback_;

  // The last received cluster info.
  std::optional<lens::LensOverlayClusterInfo> cluster_info_ = std::nullopt;

  // The cluster info received callback. Will be used to send a queued
  // interaction request if an interaction is received before the initial
  // request receives the cluster info.
  base::OnceCallback<void(lens::LensOverlayClusterInfo)>
      cluster_info_received_callback_;

  // The endpoint fetcher used for the full image request.
  std::unique_ptr<EndpointFetcher> full_image_endpoint_fetcher_;

  // The endpoint fetcher used for the interaction request. Only the last
  // endpoint fetcher is kept; additional fetch requests will discard
  // earlier unfinished requests.
  std::unique_ptr<EndpointFetcher> interaction_endpoint_fetcher_;

  // The profile, necessary to get the variation data to attach to the
  // Lens server request.
  raw_ptr<Profile> profile_;

  // The request counter, used to make sure requests are not sent out of order.
  int request_counter_ = 0;

  base::WeakPtrFactory<LensOverlayQueryController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_
