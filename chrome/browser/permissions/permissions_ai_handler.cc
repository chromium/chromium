// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permissions_ai_handler.h"

#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/proto/features/permissions_ai.pb.h"
#include "components/permissions/request_type.h"

namespace permissions {

namespace {
using ::optimization_guide::ModelBasedCapabilityKey;
using ::optimization_guide::SessionConfigParams;
using ::optimization_guide::proto::PermissionsAiRequest;
using ::optimization_guide::proto::PermissionsAiResponse;
using ::optimization_guide::proto::PermissionType;

constexpr ModelBasedCapabilityKey kFeatureKey =
    ModelBasedCapabilityKey::kPermissionsAi;
constexpr SessionConfigParams kSessionConfigParams = SessionConfigParams{
    .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
};

// Currently, the following errors, which are used when a model may have been
// installed but not yet loaded, are treated as waitable.
static constexpr auto kWaitableReasons =
    base::MakeFixedFlatSet<optimization_guide::OnDeviceModelEligibilityReason>({
        optimization_guide::OnDeviceModelEligibilityReason::
            kConfigNotAvailableForFeature,
        optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
        optimization_guide::OnDeviceModelEligibilityReason::
            kSafetyModelNotAvailable,
        optimization_guide::OnDeviceModelEligibilityReason::
            kLanguageDetectionModelNotAvailable,
    });

void LogOnDeviceModelDownloadSuccessAndTime(
    bool success,
    base::TimeTicks model_download_start_time) {
  base::UmaHistogramBoolean("Permissions.AIv1.DownloadSuccess", success);
  if (success) {
    base::UmaHistogramMediumTimes(
        "Permissions.AIv1.DownloadTime",
        base::TimeTicks::Now() - model_download_start_time);
  }
}

void LogOnDeviceModelPreviousSessionFinishedInTime(bool success) {
  base::UmaHistogramBoolean("Permissions.AIv1.PreviousSessionFinishedInTime",
                            success);
}

void LogOnDeviceModelSessionCreationSuccessAndTime(
    bool success,
    base::TimeTicks session_creation_start_time) {
  base::UmaHistogramBoolean("Permissions.AIv1.SessionCreationSuccess", success);
  if (success) {
    base::UmaHistogramMediumTimes(
        "Permissions.AIv1.SessionCreationTime",
        base::TimeTicks::Now() - session_creation_start_time);
  }
}

void LogOnDeviceModelExecutionSuccessAndTime(
    bool success,
    base::TimeTicks session_execution_start_time) {
  base::UmaHistogramBoolean("Permissions.AIv1.ExecutionSuccess", success);
  if (success) {
    base::UmaHistogramMediumTimes(
        "Permissions.AIv1.ExecutionDuration",
        base::TimeTicks::Now() - session_execution_start_time);
  }
}

void LogOnDeviceModelExecutionParse(bool success) {
  base::UmaHistogramBoolean("Permissions.AIv1.ResponseParseSuccess", success);
}

void LogOnDeviceModelAvailabilityAtInquiryTime(bool success) {
  base::UmaHistogramBoolean("Permissions.AIv1.AvailableAtInquiryTime", success);
}

PermissionType GetPermissionType(permissions::RequestType request_type) {
  switch (request_type) {
    case permissions::RequestType::kNotifications:
      return PermissionType::PERMISSION_TYPE_NOTIFICATIONS;
    case permissions::RequestType::kGeolocation:
      return PermissionType::PERMISSION_TYPE_GEOLOCATION;
    default:
      return PermissionType::PERMISSION_TYPE_NOT_SPECIFIED;
  }
}

}  // namespace

PermissionsAiHandler::PermissionsAiHandler(
    OptimizationGuideKeyedService* optimization_guide)
    : optimization_guide_(optimization_guide) {}

PermissionsAiHandler::~PermissionsAiHandler() {
  StopListeningToOnDeviceModelUpdate();
}

void PermissionsAiHandler::StartListeningToOnDeviceModelUpdate() {
  if (observing_on_device_model_availability_) {
    return;
  }
  if (!optimization_guide_) {
    LogOnDeviceModelDownloadSuccessAndTime(/*success=*/false,
                                           on_device_download_start_time_);
    return;
  }

  observing_on_device_model_availability_ = true;
  optimization_guide_->AddOnDeviceModelAvailabilityChangeObserver(kFeatureKey,
                                                                  this);

  session_ =
      optimization_guide_->StartSession(kFeatureKey, kSessionConfigParams);
  if (session_) {
    SetOnDeviceModelAvailable();
  } else {
    on_device_download_start_time_ = base::TimeTicks::Now();
  }
}

void PermissionsAiHandler::StopListeningToOnDeviceModelUpdate() {
  if (!observing_on_device_model_availability_ || !optimization_guide_) {
    return;
  }

  observing_on_device_model_availability_ = false;
  optimization_guide_->RemoveOnDeviceModelAvailabilityChangeObserver(
      kFeatureKey, this);
}

void PermissionsAiHandler::SetOnDeviceModelAvailable() {
  LogOnDeviceModelDownloadSuccessAndTime(/*success=*/true,
                                         on_device_download_start_time_);
  is_on_device_model_available_ = true;
  observing_on_device_model_availability_ = false;
}

void PermissionsAiHandler::OnDeviceModelAvailabilityChanged(
    ModelBasedCapabilityKey feature,
    optimization_guide::OnDeviceModelEligibilityReason reason) {
  if (!observing_on_device_model_availability_ || feature != kFeatureKey) {
    return;
  }

  VLOG(1) << "[PermissionsAIv1] OnDeviceModelAvailability changed to state: "
          << reason;

  if (kWaitableReasons.contains(reason)) {
    return;
  }

  if (reason == optimization_guide::OnDeviceModelEligibilityReason::kSuccess) {
    SetOnDeviceModelAvailable();
  } else {
    LogOnDeviceModelDownloadSuccessAndTime(/*success=*/false,
                                           on_device_download_start_time_);
  }
}

void PermissionsAiHandler::CreateModelExecutorSession() {
  if (!optimization_guide_) {
    return;
  }
  if (is_on_device_model_available_) {
    session_ =
        optimization_guide_->StartSession(kFeatureKey, kSessionConfigParams);
  } else {
    StartListeningToOnDeviceModelUpdate();
  }
}

void PermissionsAiHandler::OnModelExecutionComplete(
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  if (!result.response.has_value()) {
    VLOG(1) << "[PermissionsAIv1] OnModelExecutionComplete failed with error: "
            << static_cast<int>(result.response.error().error());
    LogOnDeviceModelExecutionSuccessAndTime(/*success=*/false,
                                            session_execution_start_time_);
    if (inquire_on_device_model_callback_) {
      std::move(inquire_on_device_model_callback_).Run(std::nullopt);
    }
    return;
  }

  // This is a non-error response, but it's not completed, yet so we wait till
  // it's complete. We will not respond to the callback yet because of this.
  if (!result.response->is_complete) {
    return;
  }

  LogOnDeviceModelExecutionSuccessAndTime(/*success=*/true,
                                          session_execution_start_time_);

  std::optional<PermissionsAiResponse> permissions_ai_response =
      optimization_guide::ParsedAnyMetadata<PermissionsAiResponse>(
          result.response->response);

  if (!permissions_ai_response.has_value()) {
    VLOG(1) << "[PermissionsAIv1] OnModelExecutionComplete failed while "
               "parsing the response proto.";
    LogOnDeviceModelExecutionParse(/*success=*/false);
    if (inquire_on_device_model_callback_) {
      std::move(inquire_on_device_model_callback_).Run(std::nullopt);
    }
    return;
  }

  LogOnDeviceModelExecutionParse(/*success=*/true);

  if (session_) {
    session_.reset();
  }

  if (inquire_on_device_model_callback_) {
    std::move(inquire_on_device_model_callback_).Run(permissions_ai_response);
  }
}

bool PermissionsAiHandler::IsOnDeviceModelAvailable() {
  return is_on_device_model_available_;
}

void PermissionsAiHandler::InquireAiOnDeviceModel(
    std::string rendered_text,
    permissions::RequestType request_type,
    base::OnceCallback<void(std::optional<PermissionsAiResponse>)> callback) {
  // TODO(crbug.com/382447738): It can happen that a new inquiry comes before
  // the previous finishes its execution. To avoid unexpected behavior return
  // `std::nullopt` which means another type of CPSS logic will be executed.
  if (session_) {
    LogOnDeviceModelPreviousSessionFinishedInTime(/*success=*/false);
    std::move(callback).Run(std::nullopt);
    return;
  } else if (is_on_device_model_available_) {
    LogOnDeviceModelPreviousSessionFinishedInTime(/*success=*/true);
  }

  base::TimeTicks session_creation_start_time = base::TimeTicks::Now();

  CreateModelExecutorSession();
  LogOnDeviceModelAvailabilityAtInquiryTime(is_on_device_model_available_);
  LogOnDeviceModelSessionCreationSuccessAndTime(
      /*success=*/session_ != nullptr, session_creation_start_time);

  if (!session_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  PermissionsAiRequest request;
  request.set_rendered_text(std::move(rendered_text));
  request.set_permission_type(GetPermissionType(request_type));

  inquire_on_device_model_callback_ = std::move(callback);
  session_execution_start_time_ = base::TimeTicks::Now();
  session_->ExecuteModel(
      request,
      base::BindRepeating(&PermissionsAiHandler::OnModelExecutionComplete,
                          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace permissions
