// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/prediction_service/permissions_aiv4_handler.h"
#include "components/permissions/request_type.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/download/public/background_service/download_params.h"
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/permissions/prediction_service/prediction_model_handler.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace permissions {

using optimization_guide::proto::OptimizationTarget;

namespace {

inline OptimizationTarget getGeolocationAiv4OptTarget() {
#if BUILDFLAG(IS_ANDROID)
  return OptimizationTarget::
      OPTIMIZATION_TARGET_PERMISSIONS_AIV4_GEOLOCATION_ANDROID;
#else
  return OptimizationTarget::
      OPTIMIZATION_TARGET_PERMISSIONS_AIV4_GEOLOCATION_DESKTOP;
#endif
}
inline OptimizationTarget getNotificationsAiv4OptTarget() {
#if BUILDFLAG(IS_ANDROID)
  return OptimizationTarget::
      OPTIMIZATION_TARGET_PERMISSIONS_AIV4_NOTIFICATIONS_ANDROID;
#else
  return OptimizationTarget::
      OPTIMIZATION_TARGET_PERMISSIONS_AIV4_NOTIFICATIONS_DESKTOP;
#endif
}

}  // namespace

PredictionModelHandlerProvider::PredictionModelHandlerProvider(
    OptimizationGuideKeyedService* optimization_guide,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    passage_embeddings::Embedder* passage_embedder)
    : passage_embedder_(passage_embedder) {
  VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider ctor "
             "passage_embedder available: "
          << (passage_embedder ? "true" : "false");
  VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider ctor "
             "optimization_guide available: "
          << (optimization_guide ? "true" : "false");

  if (!optimization_guide) {
    VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider optimization "
               "guide is null";
    return;
  }

  // We set up model handlers if necessary in order of preference:
  // Aiv4, Aiv3
  // CPSSv1 is defined always as backup if further requirements for AivX are not
  // fulfilled (like the MSBB bit that we don't check here at the moment).
  // TODO(crbug.com/414527270) Only create models when its really necessary (see
  // PermissionsAiUiSelector::GetPredictionTypeToUse).
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (embedder_metadata_provider) {
    embedder_metadata_observation_.Observe(embedder_metadata_provider);
  }

  // This feature is enabled by default; we add the check here to fix internally
  // failing tests.
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDeviceNotificationPredictions)) {
    notification_prediction_model_handler_ =
        std::make_unique<PredictionModelHandler>(
            optimization_guide,
            OptimizationTarget::
                OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS);
  }

  // This feature is enabled by default; we add the check here to fix internally
  // failing tests.
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDeviceGeolocationPredictions)) {
    geolocation_prediction_model_handler_ =
        std::make_unique<PredictionModelHandler>(
            optimization_guide,
            OptimizationTarget::
                OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS);
  }

  if (IsAIv4FeatureEnabled()) {
    VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider init AIv4";
#if BUILDFLAG(IS_ANDROID)
    download::SchedulingParams scheduling_params;
    scheduling_params.priority = download::SchedulingParams::Priority::HIGH;
    scheduling_params.battery_requirements =
        download::SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
    scheduling_params.network_requirements =
        download::SchedulingParams::NetworkRequirements::UNMETERED;
#else
    std::optional<download::SchedulingParams> scheduling_params = std::nullopt;
#endif
    notification_aiv4_handler_ = std::make_unique<PermissionsAiv4Handler>(
        optimization_guide, getNotificationsAiv4OptTarget(),
        RequestType::kNotifications, scheduling_params);
    geolocation_aiv4_handler_ = std::make_unique<PermissionsAiv4Handler>(
        optimization_guide, getGeolocationAiv4OptTarget(),
        RequestType::kGeolocation, scheduling_params);
    return;
  }
  if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv3)) {
    VLOG(1) << "[PermissionsAI] PredictionModelHandlerProvider init AIv3";
    notification_aiv3_handler_ = std::make_unique<PermissionsAiv3Handler>(
        optimization_guide,
        OptimizationTarget::
            OPTIMIZATION_TARGET_NOTIFICATION_IMAGE_PERMISSION_RELEVANCE,
        RequestType::kNotifications);
    geolocation_aiv3_handler_ = std::make_unique<PermissionsAiv3Handler>(
        optimization_guide,
        OptimizationTarget::
            OPTIMIZATION_TARGET_GEOLOCATION_IMAGE_PERMISSION_RELEVANCE,
        RequestType::kGeolocation);
    return;
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

PredictionModelHandlerProvider::~PredictionModelHandlerProvider() = default;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PredictionModelHandlerProvider::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata metadata) {
  is_passage_embedder_ready_ = metadata.IsValid();
  VLOG(1) << "[PermissionsAI] Passage embedder readiness updated to: "
          << is_passage_embedder_ready_;
}

bool PredictionModelHandlerProvider::IsPassageEmbedderReady() const {
  return is_passage_embedder_ready_;
}

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
  CHECK_IS_TEST();
  passage_embedder_ = passage_embedder;
}

bool PredictionModelHandlerProvider::IsAIv4FeatureEnabled() {
  return base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv4);
}

passage_embeddings::Embedder*
PredictionModelHandlerProvider::GetPassageEmbedder() {
  return passage_embedder_;
}

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}  // namespace permissions
