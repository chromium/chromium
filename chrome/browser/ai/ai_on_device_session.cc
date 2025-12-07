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
        callback) {
  requests_.push({std::move(request), std::move(callback)});
  MaybeRunNextExecutionRequest();
}

void AIOnDeviceSession::MaybeRunNextExecutionRequest() {
  if (is_execution_in_progress_ || !session_ || requests_.empty()) {
    return;
  }

  auto request_pair = std::move(requests_.front());
  requests_.pop();

  is_execution_in_progress_ = true;
  session_->ExecuteModel(
      request_pair.first.BuildProtoMessage(),
      base::BindRepeating(&AIOnDeviceSession::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(request_pair.second)));
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
