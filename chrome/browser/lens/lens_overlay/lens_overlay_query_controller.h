// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_
#define CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_

#include "base/functional/callback.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_request_id_generator.h"
#include "chrome/browser/resources/lens/server/proto/lens_overlay_request.pb.h"
#include "chrome/browser/resources/lens/server/proto/lens_overlay_response.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_interaction_request_metadata.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace lens {

// Manages queries on behalf of a Lens overlay.
class LensOverlayQueryController {
 public:
  explicit LensOverlayQueryController(
      base::RepeatingCallback<void(lens::proto::LensOverlayFullImageResponse)>
          full_image_callback,
      base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>
          url_callback,
      base::RepeatingCallback<void(lens::proto::LensOverlayInteractionResponse)>
          interaction_data_callback);
  virtual ~LensOverlayQueryController();

  // Starts a query flow by sending a request to Lens using the screenshot,
  // returning the response to the full image callback. Should be called
  // exactly once.
  void StartQueryFlow(const SkBitmap& screenshot);

  // Clears the state and resets stored values.
  void EndQuery();

  // Sends a request to Lens representing an interaction following the initial
  // query. Expected to be called multiple times.
  void SendInteraction(lens::proto::LensOverlayRequest request);

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

  // Creates the image data for the initial request, setting
  // full_image_data_.
  void CreateFullImageData(base::OnceCallback<void()> callback);

  // Fetches the endpoint using the initial image data.
  void FetchFullImageRequest(
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // Handles the endpoint fetch response for the initial request.
  void FullImageFetchResponseHandler(
      std::unique_ptr<EndpointResponse> response);

  // Creates the metadata for an interaction request, setting
  // cropped_image_data_.
  void CropImageForInteraction(lens::proto::LensOverlayRequest request,
                               base::OnceCallback<void()> callback);

  // Handles the endpoint fetch response for an interaction request.
  void InteractionFetchResponseHandler(
      std::unique_ptr<EndpointResponse> response);

  // Fetches the endpoint using the last interaction data if the initial
  // response was received.
  void MaybeFetchInteractionRequest(
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // Fetches the endpoint using the last interaction data.
  void FetchInteractionRequest(
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // Generates the url response and send it to the overlay controller via the
  // url callback.
  void GenerateAndSendSearchUrl(
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // The request id generator.
  std::unique_ptr<lens::LensOverlayRequestIdGenerator> request_id_generator_;

  // The original screenshot image.
  SkBitmap original_screenshot_;

  // The stored image data for the full image fetch, used for retries too.
  lens::ImageData full_image_data_;

  // The stored interaction image crop data, used for retries too.
  lens::ImageCrop cropped_image_data_;

  // The last interaction data, stored for retries.
  lens::proto::LensOverlayRequest last_interaction_data_;

  // The current state.
  QueryControllerState query_controller_state_ = QueryControllerState::kOff;

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  base::RepeatingCallback<void(lens::proto::LensOverlayFullImageResponse)>
      full_image_callback_;

  // Url callback for for an interaction.
  base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>
      url_callback_;

  // Interaction data callback for for an interaction.
  base::RepeatingCallback<void(lens::proto::LensOverlayInteractionResponse)>
      interaction_data_callback_;

  // Whether or not there is a pending interaction request to be sent.
  // This can happen if an interaction request failed (indicating the server
  // data is stale) or the user performed an interaction before the initial
  // full image response was received.
  bool has_pending_interaction_ = false;

  // Whether or not there was an interaction request whose search url
  // needs to be generated. This can happen if an interaction occurs
  // before the full image response was received.
  bool has_pending_url_change_ = false;

  // The endpoint fetcher used for the full image request.
  std::unique_ptr<EndpointFetcher> full_image_endpoint_fetcher_;

  // The endpoint fetcher used for the interaction request. Only the last
  // endpoint fetcher is kept; additional fetch requests will discard
  // earlier unfinished requests.
  std::unique_ptr<EndpointFetcher> interaction_endpoint_fetcher_;

  base::WeakPtrFactory<LensOverlayQueryController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_OVERLAY_LENS_OVERLAY_QUERY_CONTROLLER_H_
