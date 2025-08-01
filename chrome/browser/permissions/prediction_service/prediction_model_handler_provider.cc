// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/permissions/prediction_service/permissions_aiv1_handler.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/prediction_service/permissions_aiv4_handler.h"
#include "components/permissions/request_type.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/permissions/prediction_service/prediction_model_handler.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace permissions {

PredictionModelHandlerProvider::PredictionModelHandlerProvider(
    OptimizationGuideKeyedService* optimization_guide) {
  VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider ctor";
  // We set up model handlers if necessary in order of preference:
  // Aiv4, Aiv3, Aiv1
  // CPSSv1 is defined always as backup if further requirements for AivX are not
  // fulfilled (like the MSBB bit that we don't check here at the moment).
  // TODO(crbug.com/414527270) Only create models when its really necessary (see
  // PredictionBasedPermissionUiSelector::GetPredictionTypeToUse).
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

  if (IsAiv4ModelAvailable()) {
    VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider init AIv4";
    notification_aiv4_handler_ = std::make_unique<PermissionsAiv4Handler>(
        optimization_guide,
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_PERMISSIONS_AIV4_NOTIFICATIONS_DESKTOP,
        RequestType::kNotifications);
    geolocation_aiv4_handler_ = std::make_unique<PermissionsAiv4Handler>(
        optimization_guide,
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_PERMISSIONS_AIV4_GEOLOCATION_DESKTOP,
        RequestType::kGeolocation);
    return;
  }
  if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv3)) {
    VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider init AIv3";
    notification_aiv3_handler_ = std::make_unique<PermissionsAiv3Handler>(
        optimization_guide,
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_NOTIFICATION_IMAGE_PERMISSION_RELEVANCE,
        RequestType::kNotifications);
    geolocation_aiv3_handler_ = std::make_unique<PermissionsAiv3Handler>(
        optimization_guide,
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_GEOLOCATION_IMAGE_PERMISSION_RELEVANCE,
        RequestType::kGeolocation);
    return;
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

PermissionsAiv4Handler*
PredictionModelHandlerProvider::GetPermissionsAiv4Handler(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kNotifications:
      return notification_aiv4_handler_.get();
    case RequestType::kGeolocation:
      return geolocation_aiv4_handler_.get();
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

void PredictionModelHandlerProvider::set_permissions_aiv4_handler_for_testing(
    RequestType request_type,
    std::unique_ptr<PermissionsAiv4Handler> aiv4_handler) {
  CHECK_IS_TEST();
  switch (request_type) {
    case RequestType::kNotifications:
      notification_aiv4_handler_ = std::move(aiv4_handler);
      break;
    case RequestType::kGeolocation:
      geolocation_aiv4_handler_ = std::move(aiv4_handler);
      break;
    default:
      NOTREACHED();
  }
}

void PredictionModelHandlerProvider::set_passage_embedder_for_testing(
    passage_embeddings::Embedder* passage_embedder) {
  passage_embedder_for_testing = passage_embedder;
}

bool PredictionModelHandlerProvider::IsAiv4ModelAvailable() {
  return base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv4);
  // TODO(crbug.com/382447738) Add check for language as the text embeddings
  // model required for preparing the text input of AIv4 only works on english
  // text for now.
}
passage_embeddings::Embedder*
PredictionModelHandlerProvider::GetPassageEmbedder() {
  if (passage_embedder_for_testing.has_value()) {
    CHECK_IS_TEST();
    return passage_embedder_for_testing.value();
  }
  if (auto* passage_embeddings_service_controller =
          passage_embeddings::ChromePassageEmbeddingsServiceController::Get()) {
    return passage_embeddings_service_controller->GetEmbedder();
  }
  return nullptr;
}

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}  // namespace permissions
