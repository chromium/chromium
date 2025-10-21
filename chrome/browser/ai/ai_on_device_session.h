// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_ON_DEVICE_SESSION_H_
#define CHROME_BROWSER_AI_AI_ON_DEVICE_SESSION_H_

#include "base/containers/queue.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"

// Execution set for Optimization Guide sessions. It handles queueing requests
// for `ExecuteModel()` since multiple executions are not supported currently.
// Not thread-safe.
//
// TODO(crbug.com/403352722): When Optimization Guide supports broker interface
// via mojo, the logic in this class can be moved to the implementation of
// each feature.
class AIOnDeviceSession {
 public:
  explicit AIOnDeviceSession(
      std::unique_ptr<optimization_guide::OnDeviceSession> session);

  ~AIOnDeviceSession();

  // Not copyable or movable.
  AIOnDeviceSession(const AIOnDeviceSession&) = delete;
  AIOnDeviceSession& operator=(const AIOnDeviceSession&) = delete;

  // Queues the request for `OnDeviceSession::ExecuteModel()`.
  void ExecuteModelOrQueue(
      optimization_guide::MultimodalMessage request,
      optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
          callback);

  optimization_guide::OnDeviceSession* session() { return session_.get(); }

 private:
  // Takes the next pending request, if there is no execution in flight.
  void MaybeRunNextExecutionRequest();

  // Callback function invoked by `OnDeviceSession::ExecuteModel()`.
  void ModelExecutionCallback(
      optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
          final_callback,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // The underlying session provided by Optimization Guide.
  std::unique_ptr<optimization_guide::OnDeviceSession> session_;

  // Queue holding execution requests.
  base::queue<
      std::pair<optimization_guide::MultimodalMessage,
                optimization_guide::
                    OptimizationGuideModelExecutionResultStreamingCallback>>
      requests_;

  bool is_execution_in_progress_ = false;

  base::WeakPtrFactory<AIOnDeviceSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_ON_DEVICE_SESSION_H_
