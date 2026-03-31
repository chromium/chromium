// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_on_device_session.h"

AIOnDeviceSession::AIOnDeviceSession(
    std::unique_ptr<optimization_guide::OnDeviceSession> session)
    : session_(std::move(session)) {}

AIOnDeviceSession::~AIOnDeviceSession() = default;

void AIOnDeviceSession::ExecuteModelOrQueue(
    optimization_guide::MultimodalMessage request,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        callback,
    on_device_model::mojom::ResponseConstraintPtr constraint) {
  requests_.emplace(std::move(request), std::move(callback),
                    std::move(constraint));
  MaybeRunNextExecutionRequest();
}

void AIOnDeviceSession::MaybeRunNextExecutionRequest() {
  if (is_execution_in_progress_ || !session_ || requests_.empty()) {
    return;
  }

  auto request = std::move(requests_.front());
  requests_.pop();

  is_execution_in_progress_ = true;
  session_->ExecuteModelWithResponseConstraint(
      request.message.BuildProtoMessage(), std::move(request.constraint),
      base::BindRepeating(&AIOnDeviceSession::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(request.callback)));
}

void AIOnDeviceSession::ModelExecutionCallback(
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        final_callback,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  // Current execution is considered completed if there is no response value
  // (the error case), or streaming response is marked completed.
  is_execution_in_progress_ =
      result.response.has_value() && !result.response->is_complete;

  std::move(final_callback).Run(std::move(result));

  MaybeRunNextExecutionRequest();
}

AIOnDeviceSession::ExecutionRequest::ExecutionRequest(
    optimization_guide::MultimodalMessage message,
    optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
        callback,
    on_device_model::mojom::ResponseConstraintPtr constraint)
    : message(std::move(message)),
      callback(std::move(callback)),
      constraint(std::move(constraint)) {}

AIOnDeviceSession::ExecutionRequest::ExecutionRequest(ExecutionRequest&&) =
    default;

AIOnDeviceSession::ExecutionRequest::~ExecutionRequest() = default;
