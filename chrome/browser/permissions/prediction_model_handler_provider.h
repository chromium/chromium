// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_PROVIDER_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_PROVIDER_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/permissions/request_type.h"

class OptimizationGuideKeyedService;

namespace permissions {

class PredictionModelHandler;
class PermissionsAiv1Handler;
class PermissionsAiv3Handler;

class PredictionModelHandlerProvider : public KeyedService {
 public:
  explicit PredictionModelHandlerProvider(
      OptimizationGuideKeyedService* optimization_guide);
  ~PredictionModelHandlerProvider() override;
  PredictionModelHandlerProvider(const PredictionModelHandlerProvider&) =
      delete;
  PredictionModelHandlerProvider& operator=(
      const PredictionModelHandlerProvider&) = delete;

  PermissionsAiv1Handler* GetPermissionsAiv1Handler();

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  PredictionModelHandler* GetPredictionModelHandler(RequestType request_type);
  PermissionsAiv3Handler* GetPermissionsAiv3Handler(RequestType request_type);
  void set_permissions_aiv3_handler_for_testing(
      RequestType request_type,
      std::unique_ptr<PermissionsAiv3Handler> handler);
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

 private:
  std::unique_ptr<PermissionsAiv1Handler> permissions_aiv1_handler_;
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::unique_ptr<PredictionModelHandler>
      notification_prediction_model_handler_;
  std::unique_ptr<PredictionModelHandler> geolocation_prediction_model_handler_;
  std::unique_ptr<PermissionsAiv3Handler> notification_aiv3_handler_;
  std::unique_ptr<PermissionsAiv3Handler> geolocation_aiv3_handler_;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
};
}  // namespace permissions
#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_PROVIDER_H_
