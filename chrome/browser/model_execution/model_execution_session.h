// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MODEL_EXECUTION_MODEL_EXECUTION_SESSION_H_
#define CHROME_BROWSER_MODEL_EXECUTION_MODEL_EXECUTION_SESSION_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom.h"

// The implementation of `blink::mojom::ModelGenericSession`, which exposes the
// single stream-based `Execute()` API for model execution.
class ModelExecutionSession : public blink::mojom::ModelGenericSession {
 public:
  explicit ModelExecutionSession(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session);
  ModelExecutionSession(const ModelExecutionSession&) = delete;
  ModelExecutionSession& operator=(const ModelExecutionSession&) = delete;

  ~ModelExecutionSession() override;

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::ModelGenericSession> receiver);

  // `blink::mojom::ModelGenericSession` implementation.
  void Execute(const std::string& input,
               mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                   responder) override;

 private:
  void ModelExecutionCallback(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  mojo::Receiver<blink::mojom::ModelGenericSession> receiver_{this};
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::WeakPtrFactory<ModelExecutionSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MODEL_EXECUTION_MODEL_EXECUTION_SESSION_H_
