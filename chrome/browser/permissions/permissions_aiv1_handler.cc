// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permissions_aiv1_handler.h"

#include "base/check_is_test.h"
#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
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
using EligibilityReason = ::optimization_guide::OnDeviceModelEligibilityReason;

constexpr ModelBasedCapabilityKey kFeatureKey =
    ModelBasedCapabilityKey::kPermissionsAi;

constexpr SessionConfigParams kSessionConfigParams = SessionConfigParams{
    .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly,
};

// Max delay for permissions AI model execution; inquiries that need more time
// get cancelled.
constexpr base::TimeDelta kMaxModelExecutionDuration = base::Seconds(5);

void LogOnDeviceModelPreviousSessionFinishedInTime(bool success) {
  base::UmaHistogramBoolean("Permissions.AIv1.PreviousSessionFinishedInTime",
                            success);
}

void LogOnDeviceModelSessionCreationSuccess(bool session_created) {
  base::UmaHistogramBoolean("Permissions.AIv1.SessionCreationSuccess",
                            session_created);
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

void LogOnDeviceModelAvailabilityAtInquiryTime(bool model_available) {
  base::UmaHistogramBoolean("Permissions.AIv1.AvailableAtInquiryTime",
                            model_available);
}

void LogOnDeviceModelExecutionTimedOut(bool timed_out) {
  base::UmaHistogramBoolean("Permissions.AIv1.ExecutionTimedOut", timed_out);
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

bool IsOnDeviceModelAvailable(EligibilityReason reason) {
  return reason == EligibilityReason::kSuccess;
}
}  // namespace

// Manages sessions and related data. This will gift us more flexibility, i.e.
// it can allow us to have multiple sessions at some point in the future at the
// same time and also to easily cancel session without fearing asynchronous
// calls to OnModelExecutionComplete meddling with a potentially new permission
// request.
class PermissionsAiv1Handler::EvaluationTask {
 public:
  explicit EvaluationTask(OptimizationGuideKeyedService* optimization_guide) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    session_ =
        optimization_guide->StartSession(kFeatureKey, kSessionConfigParams);
  }

  ~EvaluationTask() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Explicitly invalidate weak pointers to prevent callbacks that may be
    // triggered by the destructor logic.
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (inquire_on_device_model_callback_) {
      std::move(inquire_on_device_model_callback_).Run(std::nullopt);
    }
  }

  bool IsActive() { return session_ != nullptr; }

  void ExecuteModel(
      std::string rendered_text,
      permissions::RequestType request_type,
      base::OnceCallback<void(std::optional<PermissionsAiResponse>)> callback,
      base::OneShotTimer* execution_timer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!session_) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    inquire_on_device_model_callback_ = std::move(callback);

    PermissionsAiRequest request;
    request.set_rendered_text(std::move(rendered_text));
    request.set_permission_type(GetPermissionType(request_type));

    session_execution_start_time_ = base::TimeTicks::Now();
    session_->ExecuteModel(
        request,
        base::BindRepeating(
            &PermissionsAiv1Handler::EvaluationTask::OnModelExecutionComplete,
            weak_ptr_factory_.GetWeakPtr(), execution_timer));
  }

 private:
  void OnModelExecutionComplete(
      base::OneShotTimer* execution_timer,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // This is a non-error response, but since the result is not fully available
    // yet, we defer callback execution until we get a complete response.
    if (result.response.has_value() && !result.response->is_complete) {
      return;
    }

    if (execution_timer) {
      execution_timer->Stop();
      LogOnDeviceModelExecutionTimedOut(/*timed_out=*/false);
    }
    // Since we do not want to reuse a once used session, lets make sure that we
    // destroy it when the model execution has finished.
    session_.reset();

    // If there is no callback available anymore, we do not need to do any work
    // here.
    if (!inquire_on_device_model_callback_) {
      return;
    }

    LogOnDeviceModelExecutionSuccessAndTime(/*success=*/
                                            result.response.has_value(),
                                            session_execution_start_time_);
    if (!result.response.has_value()) {
      VLOG(1)
          << "[PermissionsAIv1] OnModelExecutionComplete failed with error: "
          << static_cast<int>(result.response.error().error());
      std::move(inquire_on_device_model_callback_).Run(std::nullopt);
      return;
    }

    std::optional<PermissionsAiResponse> permissions_ai_response =
        optimization_guide::ParsedAnyMetadata<PermissionsAiResponse>(
            result.response->response);
    LogOnDeviceModelExecutionParse(
        /*success=*/permissions_ai_response.has_value());

    if (!permissions_ai_response.has_value()) {
      VLOG(1) << "[PermissionsAIv1] OnModelExecutionComplete failed while "
                 "parsing the response proto.";
      std::move(inquire_on_device_model_callback_).Run(std::nullopt);
      return;
    }

    std::move(inquire_on_device_model_callback_).Run(permissions_ai_response);
  }

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  base::TimeTicks session_execution_start_time_;
  base::OnceCallback<void(
      std::optional<optimization_guide::proto::PermissionsAiResponse>)>
      inquire_on_device_model_callback_;
  base::WeakPtrFactory<PermissionsAiv1Handler::EvaluationTask>
      weak_ptr_factory_{this};
};

PermissionsAiv1Handler::PermissionsAiv1Handler(
    OptimizationGuideKeyedService* optimization_guide)
    : optimization_guide_(optimization_guide),
      execution_timer_(std::make_unique<base::OneShotTimer>()) {}

PermissionsAiv1Handler::~PermissionsAiv1Handler() {
  execution_timer_->Stop();
}

bool PermissionsAiv1Handler::IsModelExecutionInProgress() {
  return evaluation_task_ && evaluation_task_->IsActive();
}

void PermissionsAiv1Handler::InquireAiOnDeviceModel(
    std::string rendered_text,
    permissions::RequestType request_type,
    base::OnceCallback<void(std::optional<PermissionsAiResponse>)> callback) {
  if (!optimization_guide_) {
    // If optimization_guide_ is a nullptr then we cannot do anything here at
    // all.
    std::move(callback).Run(std::nullopt);
    return;
  }

  EligibilityReason model_state =
      optimization_guide_->GetOnDeviceModelEligibility(kFeatureKey);
  bool model_available_at_inquiry_time = IsOnDeviceModelAvailable(model_state);
  LogOnDeviceModelAvailabilityAtInquiryTime(model_available_at_inquiry_time);

  if (model_available_at_inquiry_time) {
    if (IsModelExecutionInProgress()) {
      LogOnDeviceModelPreviousSessionFinishedInTime(/*success=*/false);
      // TODO(crbug.com/382447738): It can happen that a new inquiry comes
      // before the previous finishes its execution. To avoid unexpected
      // behavior return `std::nullopt` which means another type of CPSS logic
      // will be executed.
      std::move(callback).Run(std::nullopt);
      return;
    }
    LogOnDeviceModelPreviousSessionFinishedInTime(/*success=*/true);
  }

  // We make sure by recreating the session on every inquiry that no
  // callback executions or timing data linked to the previous request gets
  // accidentally mixed up with the new request when we cancel lengthy model
  // executions.
  evaluation_task_ = std::make_unique<EvaluationTask>(optimization_guide_);

  LogOnDeviceModelSessionCreationSuccess(
      /*session_created=*/evaluation_task_->IsActive());

  if (evaluation_task_->IsActive()) {
    evaluation_task_->ExecuteModel(std::move(rendered_text), request_type,
                                   std::move(callback), execution_timer_.get());

    // We check again to make sure the callback did not immediately return.
    if (evaluation_task_->IsActive()) {
      execution_timer_->Start(
          FROM_HERE, kMaxModelExecutionDuration,
          base::BindOnce(&PermissionsAiv1Handler::CancelModelExecution,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  // We end up here if the model download has not started/ended yet or
  // there is a transient error. If we did not download the model yet, trying
  // to create the session will have started the download already so we do
  // nothing here and just return. We also return for the other two cases,
  // hoping the next inquiry will have better luck.
  std::move(callback).Run(std::nullopt);
}

void PermissionsAiv1Handler::CancelModelExecution() {
  evaluation_task_.reset();
  execution_timer_->Stop();
  LogOnDeviceModelExecutionTimedOut(/*timed_out=*/true);
}

void PermissionsAiv1Handler::set_execution_timer_for_testing(
    std::unique_ptr<base::OneShotTimer> execution_timer) {
  CHECK_IS_TEST();
  execution_timer_ = std::move(execution_timer);
}

}  // namespace permissions
