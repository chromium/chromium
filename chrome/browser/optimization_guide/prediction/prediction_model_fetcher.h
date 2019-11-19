// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_FETCHER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace optimization_guide {

// Callback to inform the caller that the remote hints have been fetched and
// to pass back the fetched hints response from the remote Optimization Guide
// Service.
using ModelsFetchedCallback = base::OnceCallback<void(
    base::Optional<
        std::unique_ptr<optimization_guide::proto::GetModelsResponse>>)>;

// A class to handle requests for prediction models (and prediction data) from
// a remote Optimization Guide Service.
//
// This class fetches new models from the remote Optimization Guide Service.
class PredictionModelFetcher {
 public:
  PredictionModelFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_get_models_url);
  virtual ~PredictionModelFetcher();

  // Requests PredictionModels and HostModelFeatures from the Optimization Guide
  // Service if a request for them is not already in progress. Returns whether a
  // new request was issued. |models_fetched_callback| is called when the
  // request is complete providing the GetModelsResponse object if successful or
  // nullopt if the fetch failed or no fetch is needed. Virtualized for testing.
  virtual bool FetchOptimizationGuideServiceModels(
      const std::vector<optimization_guide::proto::ModelInfo>&
          models_request_info,
      const std::vector<std::string>& hosts,
      optimization_guide::proto::RequestContext request_context,
      ModelsFetchedCallback models_fetched_callback);

 private:
  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Handles the response from the remote Optimization Guide Service.
  // |response| is the response body, |status| is the
  // |net::Error| of the response, and response_code is the HTTP
  // response code (if available).
  void HandleResponse(const std::string& response,
                      int status,
                      int response_code);

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  ModelsFetchedCallback models_fetched_callback_;

  // The URL for the remote Optimization Guide Service that serves models and
  // host features.
  const GURL optimization_guide_service_get_models_url_;

  // Used to hold the GetModelsRequest being constructed and sent as a remote
  // request.
  std::unique_ptr<optimization_guide::proto::GetModelsRequest>
      pending_models_request_;

  // Holds the URLLoader for an active hints request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Used for creating a |url_loader_| when needed for request hints.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PredictionModelFetcher);
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_FETCHER_H_
