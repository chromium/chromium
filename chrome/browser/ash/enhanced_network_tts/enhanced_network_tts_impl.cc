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

EnhancedNetworkTtsImpl& EnhancedNetworkTtsImpl::GetInstance() {
  static base::NoDestructor<EnhancedNetworkTtsImpl> tts_impl;
  return *tts_impl;
}

EnhancedNetworkTtsImpl::EnhancedNetworkTtsImpl()
    : api_key_(google_apis::GetReadAloudAPIKey()) {}
EnhancedNetworkTtsImpl::~EnhancedNetworkTtsImpl() = default;

void EnhancedNetworkTtsImpl::BindReceiverAndURLFactory(
    mojo::PendingReceiver<mojom::EnhancedNetworkTts> receiver,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Reset the receiver in case of rebinding (e.g., after the extension crash).
  receiver_.reset();
  receiver_.Bind(std::move(receiver));

  url_loader_factory_ = url_loader_factory;
}

void EnhancedNetworkTtsImpl::GetAudioData(const std::string& text,
                                          GetAudioDataCallback callback) {
  std::unique_ptr<network::SimpleURLLoader> url_loader = MakeRequestLoader();
  url_loader->AttachStringForUpload(FormatJsonRequest(text),
                                    kNetworkRequestUploadType);
  ongoing_server_requests_.push_back(std::move(url_loader));
  const UrlLoaderList::iterator last_request =
      std::prev(ongoing_server_requests_.end());
  network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback =
      base::BindOnce(&EnhancedNetworkTtsImpl::OnServerResponseReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     last_request);
  ongoing_server_requests_.back()->DownloadToString(
      url_loader_factory_.get(), std::move(body_as_string_callback),
      kEnhancedNetworkTtsMaxResponseSize);
}

data_decoder::mojom::JsonParser* EnhancedNetworkTtsImpl::GetJsonParser() {
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
    GetAudioDataCallback audio_callback,
    const UrlLoaderList::iterator server_request_it,
    const std::unique_ptr<std::string> json_response) {
  ongoing_server_requests_.erase(server_request_it);

  if (!json_response) {
    DVLOG(1) << "HTTP request for Enhance Network TTS failed.";
    std::move(audio_callback).Run(GetResultOnError());
    return;
  }

  // Send the JSON string to a dedicated service for safe parsing.
  GetJsonParser()->Parse(
      *json_response,
      base::BindOnce(&EnhancedNetworkTtsImpl::OnResponseJsonParsed,
                     weak_factory_.GetWeakPtr(), std::move(audio_callback)));
}

void EnhancedNetworkTtsImpl::OnResponseJsonParsed(
    GetAudioDataCallback audio_callback,
    const absl::optional<base::Value> json_data,
    const absl::optional<std::string>& error) {
  const bool success = json_data.has_value() && !error.has_value();

  // Extract results for the request.
  if (success) {
    std::move(audio_callback).Run(UnpackJsonResponse(*json_data));
  } else {
    DVLOG(1) << "Parsing server response JSON failed with error: "
             << error.value_or("No reason reported.");
    std::move(audio_callback).Run(GetResultOnError());
  }
}

}  // namespace enhanced_network_tts
}  // namespace ash
