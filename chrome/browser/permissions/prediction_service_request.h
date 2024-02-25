// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_REQUEST_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_REQUEST_H_

#include "base/memory/weak_ptr.h"
#include "components/permissions/prediction_service/prediction_service_base.h"

namespace permissions {
class PredictionService;
struct PredictionRequestFeatures;
class GeneratePredictionsResponse;
}  // namespace permissions

// Represents a singular request to the prediction service.
class PredictionServiceRequest {
 public:
  PredictionServiceRequest(
      permissions::PredictionService* service,
      const permissions::PredictionRequestFeatures& entity,
      permissions::PredictionServiceBase::LookupResponseCallback callback);
  ~PredictionServiceRequest();

  // Disallow copy and assign.
  PredictionServiceRequest(const PredictionServiceRequest&) = delete;
  PredictionServiceRequest& operator=(const PredictionServiceRequest&) = delete;

 private:
  void LookupReponseReceived(
      bool lookup_succesful,
      bool response_from_cache,
      const std::optional<permissions::GeneratePredictionsResponse>& response);

  permissions::PredictionServiceBase::LookupResponseCallback callback_;

  base::WeakPtrFactory<PredictionServiceRequest> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_REQUEST_H_
