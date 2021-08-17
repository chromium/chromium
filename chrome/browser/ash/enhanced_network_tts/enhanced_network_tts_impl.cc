// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_impl.h"

#include <iterator>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_constants.h"
#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_utils.h"
#include "components/google/core/common/google_util.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace ash {
namespace enhanced_network_tts {

constexpr base::Feature EnhancedNetworkTtsImpl::kOverrideParams;
constexpr base::FeatureParam<std::string> EnhancedNetworkTtsImpl::kApiKey;

EnhancedNetworkTtsImpl::ServerRequest::ServerRequest(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    int start_index,
    bool is_last_request)
    : url_loader(std::move(url_loader)),
      start_index(start_index),
      is_last_request(is_last_request) {}

EnhancedNetworkTtsImpl::ServerRequest::~ServerRequest() = default;

EnhancedNetworkTtsImpl& EnhancedNetworkTtsImpl::GetInstance() {
  static base::NoDestructor<EnhancedNetworkTtsImpl> tts_impl;
  return *tts_impl;
}

EnhancedNetworkTtsImpl::EnhancedNetworkTtsImpl()
    : api_key_(kApiKey.Get().empty() ? google_apis::GetReadAloudAPIKey()
                                     : kApiKey.Get()) {}
EnhancedNetworkTtsImpl::~EnhancedNetworkTtsImpl() = default;

void EnhancedNetworkTtsImpl::BindReceiverAndURLFactory(
    mojo::PendingReceiver<mojom::EnhancedNetworkTts> receiver,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Reset the receiver in case of rebinding (e.g., after the extension crash).
  receiver_.reset();
  receiver_.Bind(std::move(receiver));

  url_loader_factory_ = url_loader_factory;
}

void EnhancedNetworkTtsImpl::GetAudioData(mojom::TtsRequestPtr request,
                                          GetAudioDataCallback callback) {
  // Reset if we have bound observer from the prior TtsRequest.
  if (on_data_received_observer_.is_bound()) {
    ResetAndSendErrorResponse(mojom::TtsRequestError::kRequestOverride);
    DVLOG(1) << "Multiple requests for Enhance Network TTS, override the "
                "prior one.";
  }

  auto pending_receiver =
      on_data_received_observer_.BindNewPipeAndPassReceiver();
  std::move(callback).Run(std::move(pending_receiver));
  // If the message pipe is disconnected, then the caller is no longer
  // interested in receiving the result of processing `request`, so reset the
  // internal state.
  on_data_received_observer_.set_disconnect_handler(
      base::BindOnce(&EnhancedNetworkTtsImpl::ResetServerRequestsAndObserver,
                     weak_factory_.GetWeakPtr()));

  // Return early if the utterance is empty.
  if (request->utterance.empty()) {
    ResetAndSendErrorResponse(mojom::TtsRequestError::kEmptyUtterance);
    return;
  }

  // TODO(crbug.com/1240445): Chop the utterance into text pieces, and queue
  // them into the |server_requests_|. Currently we send the entire utterance
  // as a single text piece.
  std::unique_ptr<network::SimpleURLLoader> url_loader = MakeRequestLoader();
  url_loader->AttachStringForUpload(FormatJsonRequest(std::move(request)),
                                    kNetworkRequestUploadType);
  server_requests_.emplace_back(std::move(url_loader),
                                0 /* text_piece_start_index */,
                                true /* is_last_request */);

  // Kick off the server requests.
  ProcessNextServerRequest();
}

data_decoder::mojom::JsonParser* EnhancedNetworkTtsImpl::GetJsonParser() {
  // TODO(crbug.com/1217301): Sets an explicit disconnect handler.
  if (!json_parser_) {
    data_decoder_.GetService()->BindJsonParser(
        json_parser_.BindNewPipeAndPassReceiver());
    json_parser_.reset_on_disconnect();
  }

  return json_parser_.get();
}

std::unique_ptr<network::SimpleURLLoader>
EnhancedNetworkTtsImpl::MakeRequestLoader() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  const GURL server_url = GURL(kReadAloudServerUrl);
  resource_request->url = server_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Put API key in request's header if a key exists, and the endpoint is
  // trusted by Google.
  if (!api_key_.empty() && server_url.SchemeIs(url::kHttpsScheme) &&
      google_util::IsGoogleAssociatedDomainUrl(server_url)) {
    resource_request->headers.SetHeader(kGoogApiKeyHeader, api_key_);
  }

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          MISSING_TRAFFIC_ANNOTATION);
}

void EnhancedNetworkTtsImpl::ProcessNextServerRequest() {
  // If there is no more request to process, resets the state variables and
  // return early.
  if (server_requests_.empty()) {
    ResetServerRequestsAndObserver();
    return;
  }

  const ServerRequestList::iterator first_request_it = server_requests_.begin();
  network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback =
      base::BindOnce(&EnhancedNetworkTtsImpl::OnServerResponseReceived,
                     weak_factory_.GetWeakPtr(), first_request_it);
  server_requests_.front().url_loader->DownloadToString(
      url_loader_factory_.get(), std::move(body_as_string_callback),
      kEnhancedNetworkTtsMaxResponseSize);
}

void EnhancedNetworkTtsImpl::OnServerResponseReceived(
    const ServerRequestList::iterator server_request_it,
    const std::unique_ptr<std::string> json_response) {
  // This callback will not be called when the url_loader and its request are
  // deleted. See simple_url_loader.h for more details.
  DCHECK(!server_requests_.empty());
  // The iterator should only point to the begin of the list.
  DCHECK(server_requests_.begin() == server_request_it);

  const int start_index = server_request_it->start_index;
  const bool is_last_request = server_request_it->is_last_request;

  // Remove the current request from the list.
  server_requests_.erase(server_request_it);

  if (!json_response) {
    DVLOG(1) << "HTTP request for Enhance Network TTS failed.";
    ResetAndSendErrorResponse(mojom::TtsRequestError::kServerError);
    return;
  }

  // Send the JSON string to a dedicated service for safe parsing.
  GetJsonParser()->Parse(
      *json_response,
      base::BindOnce(&EnhancedNetworkTtsImpl::OnResponseJsonParsed,
                     weak_factory_.GetWeakPtr(), start_index, is_last_request));
}

void EnhancedNetworkTtsImpl::OnResponseJsonParsed(
    const int start_index,
    const bool is_last_request,
    const absl::optional<base::Value> json_data,
    const absl::optional<std::string>& error) {
  const bool success = json_data.has_value() && !error.has_value();
  // Extract results for the request.
  if (success) {
    SendResponse(UnpackJsonResponse(*json_data, start_index, is_last_request));
    // Only start the next request after finishing the current one. This method
    // will also reset the internal state if there is no more request.
    ProcessNextServerRequest();
  } else {
    ResetAndSendErrorResponse(mojom::TtsRequestError::kReceivedUnexpectedData);
    DVLOG(1) << "Parsing server response JSON failed with error: "
             << error.value_or("No reason reported.");
  }
}

void EnhancedNetworkTtsImpl::SendResponse(mojom::TtsResponsePtr response) {
  if (on_data_received_observer_.is_bound()) {
    on_data_received_observer_->OnAudioDataReceived(std::move(response));
  }
}

void EnhancedNetworkTtsImpl::ResetServerRequestsAndObserver() {
  server_requests_.clear();
  on_data_received_observer_.reset();
}

void EnhancedNetworkTtsImpl::ResetAndSendErrorResponse(
    mojom::TtsRequestError error_code) {
  SendResponse(GetResultOnError(error_code));
  ResetServerRequestsAndObserver();
}

}  // namespace enhanced_network_tts
}  // namespace ash
