// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_model_fetcher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace optimization_guide {

PredictionModelFetcher::PredictionModelFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL optimization_guide_service_get_models_url)
    : optimization_guide_service_get_models_url_(
          net::AppendOrReplaceQueryParameter(
              optimization_guide_service_get_models_url,
              "key",
              optimization_guide::features::
                  GetOptimizationGuideServiceAPIKey())) {
  url_loader_factory_ = std::move(url_loader_factory);
  CHECK(optimization_guide_service_get_models_url_.SchemeIs(url::kHttpsScheme));
}

PredictionModelFetcher::~PredictionModelFetcher() = default;

bool PredictionModelFetcher::FetchOptimizationGuideServiceModels(
    const std::vector<optimization_guide::proto::ModelInfo>&
        models_request_info,
    const std::vector<std::string>& hosts,
    optimization_guide::proto::RequestContext request_context,
    ModelsFetchedCallback models_fetched_callback) {
  SEQUENCE_CHECKER(sequence_checker_);

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    std::move(models_fetched_callback).Run(base::nullopt);
    return false;
  }

  if (url_loader_)
    return false;

  // If there are no hosts or models to request, do not make a GetModelsRequest.
  if (hosts.size() == 0 && models_request_info.size() == 0) {
    std::move(models_fetched_callback).Run(base::nullopt);
    return false;
  }

  pending_models_request_ =
      std::make_unique<optimization_guide::proto::GetModelsRequest>();

  pending_models_request_->set_request_context(request_context);

  for (const auto& host : hosts)
    pending_models_request_->add_hosts(host);

  for (const auto& model_request_info : models_request_info)
    *pending_models_request_->add_requested_models() = model_request_info;

  std::string serialized_request;
  pending_models_request_->SerializeToString(&serialized_request);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("optimization_guide_model",
                                          R"(
        semantics {
          sender: "Optimization Guide"
          description:
            "Requests PredictionModels from the Optimization Guide Service for "
            "use in providing data saving and pageload optimizations for "
            "Chrome."
          trigger:
            "Requested daily if Lite mode is enabled and the browser "
            "has models provided by the Optimization Guide that are older than "
            "a threshold set by the server."
          data: "A list of models supported by the client and a list of the "
            "user's websites."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Lite mode on Android via 'Lite mode' setting. "
            "Lite mode is not available on iOS."
          policy_exception_justification: "Not implemented."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = optimization_guide_service_get_models_url_;

  // POST request for providing the GetModelsRequest proto to the remote
  // Optimization Guide Service.
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->AttachStringForUpload(serialized_request,
                                     "application/x-protobuf");

  UMA_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.PredictionModelFetcher."
      "GetModelsRequest.HostCount",
      hosts.size());

  // |url_loader_| should not retry on 5xx errors since the server may already
  // be overloaded.  |url_loader_| should retry on network changes since the
  // network stack may receive the connection change event later than |this|.
  static const int kMaxRetries = 1;
  url_loader_->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PredictionModelFetcher::OnURLLoadComplete,
                     base::Unretained(this)));

  models_fetched_callback_ = std::move(models_fetched_callback);
  return true;
}

void PredictionModelFetcher::HandleResponse(
    const std::string& get_models_response_data,
    int net_status,
    int response_code) {
  std::unique_ptr<optimization_guide::proto::GetModelsResponse>
      get_models_response =
          std::make_unique<optimization_guide::proto::GetModelsResponse>();

  UMA_HISTOGRAM_ENUMERATION(
      "OptimizationGuide.PredictionModelFetcher."
      "GetModelsResponse.Status",
      static_cast<net::HttpStatusCode>(response_code),
      net::HTTP_VERSION_NOT_SUPPORTED);
  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.PredictionModelFetcher."
      "GetModelsResponse.NetErrorCode",
      -net_status);

  if (net_status == net::OK && response_code == net::HTTP_OK &&
      get_models_response->ParseFromString(get_models_response_data)) {
    UMA_HISTOGRAM_COUNTS_100(
        "OptimizationGuide.PredictionModelFetcher."
        "GetModelsResponse.HostModelFeatureCount",
        get_models_response->host_model_features_size());
    std::move(models_fetched_callback_).Run(std::move(get_models_response));
  } else {
    std::move(models_fetched_callback_).Run(base::nullopt);
  }
}

void PredictionModelFetcher::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  HandleResponse(response_body ? *response_body : "", url_loader_->NetError(),
                 response_code);
  url_loader_.reset();
}

}  // namespace optimization_guide
