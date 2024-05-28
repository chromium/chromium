// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
#define CHROME_BROWSER_AI_AI_TEXT_SESSION_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"

// The implementation of `blink::mojom::ModelGenericSession`, which exposes the
// single stream-based `Execute()` API for model execution.
class AITextSession : public blink::mojom::AITextSession {
 public:
  explicit AITextSession(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session);
  AITextSession(const AITextSession&) = delete;
  AITextSession& operator=(const AITextSession&) = delete;

  ~AITextSession() override;

  // `blink::mojom::ModelGenericSession` implementation.
  void Prompt(const std::string& input,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Destroy() override;

 private:
  void ModelExecutionCallback(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::WeakPtrFactory<AITextSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
