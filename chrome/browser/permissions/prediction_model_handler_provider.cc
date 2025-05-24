// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_model_handler_provider.h"

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/permissions/permissions_aiv1_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/request_type.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/permissions/prediction_service/prediction_model_handler.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace permissions {

PredictionModelHandlerProvider::PredictionModelHandlerProvider(
    OptimizationGuideKeyedService* optimization_guide) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  notification_prediction_model_handler_ =
      std::make_unique<PredictionModelHandler>(
          optimization_guide,
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS);

  geolocation_prediction_model_handler_ =
      std::make_unique<PredictionModelHandler>(
          optimization_guide,
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS);

  if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv3)) {
    notification_aiv3_handler_ = std::make_unique<PermissionsAiv3Handler>(
        optimization_guide,
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_NOTIFICATION_IMAGE_PERMISSION_RELEVANCE,
        RequestType::kNotifications);
  }
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionsAIv3Geolocation)) {
    geolocation_aiv3_handler_ = std::make_unique<PermissionsAiv3Handler>(
        optimization_guide,
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_GEOLOCATION_IMAGE_PERMISSION_RELEVANCE,
        RequestType::kGeolocation);
  }

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv1)) {
    permissions_aiv1_handler_ =
        std::make_unique<PermissionsAiv1Handler>(optimization_guide);
  }
}

PredictionModelHandlerProvider::~PredictionModelHandlerProvider() = default;

PermissionsAiv1Handler*
PredictionModelHandlerProvider::GetPermissionsAiv1Handler() {
  return permissions_aiv1_handler_.get();
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
PredictionModelHandler*
PredictionModelHandlerProvider::GetPredictionModelHandler(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kNotifications:
      return notification_prediction_model_handler_.get();
    case RequestType::kGeolocation:
      return geolocation_prediction_model_handler_.get();
    default:
      NOTREACHED();
  }
}

PermissionsAiv3Handler*
PredictionModelHandlerProvider::GetPermissionsAiv3Handler(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kNotifications:
      return notification_aiv3_handler_.get();
    case RequestType::kGeolocation:
      return geolocation_aiv3_handler_.get();
    default:
      NOTREACHED();
  }
}

void PredictionModelHandlerProvider::set_permissions_aiv3_handler_for_testing(
    RequestType request_type,
    std::unique_ptr<PermissionsAiv3Handler> aiv3_handler) {
  CHECK_IS_TEST();
  switch (request_type) {
    case RequestType::kNotifications:
      notification_aiv3_handler_ = std::move(aiv3_handler);
      break;
    case RequestType::kGeolocation:
      geolocation_aiv3_handler_ = std::move(aiv3_handler);
      break;
    default:
      NOTREACHED();
  }
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}  // namespace permissions
