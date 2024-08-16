// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_WRITER_H_
#define CHROME_BROWSER_AI_AI_WRITER_H_

#include <optional>
#include <string>

#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

// The implementation of `blink::mojom::AIWriter`, which exposes the single
// stream-based `Write()` API.
class AIWriter : public blink::mojom::AIWriter {
 public:
  AIWriter(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session,
      const std::optional<std::string>& shared_context);
  AIWriter(const AIWriter&) = delete;
  AIWriter& operator=(const AIWriter&) = delete;

  ~AIWriter() override;

  // `blink::mojom::ModelTextSession` implementation.
  void Write(const std::string& input,
             const std::optional<std::string>& context,
             mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                 pending_responder) override;

 private:
  void ModelExecutionCallback(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  const std::optional<std::string> shared_context_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::WeakPtrFactory<AIWriter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_WRITER_H_
