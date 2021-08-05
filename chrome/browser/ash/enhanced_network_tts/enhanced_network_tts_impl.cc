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
  // Return early if the utterance is empty.
  if (request->utterance.empty()) {
    std::move(callback).Run(
        GetResultOnError(mojom::TtsRequestError::kEmptyUtterance));
    return;
  }

  // If the prior request is not finished, we override it and process any
  // unfinished audio callback.
  if (ongoing_server_request_)
    ongoing_server_request_.reset();

  if (ProcessOngoingAudioCallback(
          GetResultOnError(mojom::TtsRequestError::kRequestOverride)))
    DVLOG(1) << "Multiple HTTP requests for Enhance Network TTS, override the "
                "prior one.";

  ongoing_audio_callback_ = std::move(callback);
  ongoing_server_request_ = MakeRequestLoader();
  ongoing_server_request_->AttachStringForUpload(
      FormatJsonRequest(std::move(request)), kNetworkRequestUploadType);
  network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback =
      base::BindOnce(&EnhancedNetworkTtsImpl::OnServerResponseReceived,
                     weak_factory_.GetWeakPtr());
  ongoing_server_request_->DownloadToString(url_loader_factory_.get(),
                                            std::move(body_as_string_callback),
                                            kEnhancedNetworkTtsMaxResponseSize);
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

void EnhancedNetworkTtsImpl::OnServerResponseReceived(
    const std::unique_ptr<std::string> json_response) {
  // If the server request has been overridden, we process the audio callback
  // if necessary.
  if (!ongoing_server_request_) {
    ProcessOngoingAudioCallback(
        GetResultOnError(mojom::TtsRequestError::kRequestOverride));
    DVLOG(1) << "Multiple HTTP requests for Enhance Network TTS, override the "
                "prior one.";
  }

  ongoing_server_request_.reset();

  if (!json_response) {
    DVLOG(1) << "HTTP request for Enhance Network TTS failed.";
    ProcessOngoingAudioCallback(
        GetResultOnError(mojom::TtsRequestError::kServerError));
    return;
  }

  // Send the JSON string to a dedicated service for safe parsing.
  GetJsonParser()->Parse(
      *json_response,
      base::BindOnce(&EnhancedNetworkTtsImpl::OnResponseJsonParsed,
                     weak_factory_.GetWeakPtr()));
}

void EnhancedNetworkTtsImpl::OnResponseJsonParsed(
    const absl::optional<base::Value> json_data,
    const absl::optional<std::string>& error) {
  const bool success = json_data.has_value() && !error.has_value();

  // Extract results for the request.
  if (success) {
    ProcessOngoingAudioCallback(UnpackJsonResponse(*json_data));
  } else {
    DVLOG(1) << "Parsing server response JSON failed with error: "
             << error.value_or("No reason reported.");
    ProcessOngoingAudioCallback(
        GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData));
  }
}

bool EnhancedNetworkTtsImpl::ProcessOngoingAudioCallback(
    mojom::TtsResponsePtr response) {
  if (!ongoing_audio_callback_.is_null()) {
    std::move(ongoing_audio_callback_).Run(std::move(response));
    return true;
  }
  return false;
}

}  // namespace enhanced_network_tts
}  // namespace ash
