// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_SUMMARIZER_H_
#define CHROME_BROWSER_AI_AI_SUMMARIZER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_on_device_session.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/summarize.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

// TODO(crbug.com/402442890): Refactor Writing Assistance APIs to reduce
// duplicated code.
class AISummarizer : public AIContextBoundObject,
                     public blink::mojom::AISummarizer {
 public:
  AISummarizer(AIContextBoundObjectSet& context_bound_object_set,
               std::unique_ptr<
                   optimization_guide::OptimizationGuideModelExecutor::Session>
                   summarize_session,
               blink::mojom::AISummarizerCreateOptionsPtr options,
               mojo::PendingReceiver<blink::mojom::AISummarizer> receiver);

  // `blink::mojom::AISummarizer` implementation.
  void Summarize(const std::string& input,
                 const std::string& context,
                 mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                     pending_responder) override;
  void MeasureUsage(const std::string& input,
                    const std::string& context,
                    MeasureUsageCallback callback) override;

  // AIContextBoundObject:
  void SetPriority(on_device_model::mojom::Priority priority) override;

  ~AISummarizer() override;

  static std::unique_ptr<optimization_guide::proto::SummarizeOptions>
  ToProtoOptions(const blink::mojom::AISummarizerCreateOptionsPtr& options);

  // Joins `shared` and `input` contexts with a space and newline as needed.
  static std::string CombineContexts(std::string_view shared,
                                     std::string_view input);

 private:
  friend class AITestUtils;

  void DidGetExecutionInputSizeForSummarize(
      mojo::RemoteSetElementId responder_id,
      const optimization_guide::proto::SummarizeRequest& request,
      std::optional<uint32_t> result);

  void DidGetExecutionInputSizeInTokensForMeasure(
      MeasureUsageCallback callback,
      std::optional<uint32_t> result);

  void ModelExecutionCallback(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  optimization_guide::proto::SummarizeRequest BuildRequest(
      const std::string& input,
      const std::string& context);

  AIOnDeviceSession session_wrapper_;

  mojo::Remote<blink::mojom::AISummarizer> remote_;

  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Summarize()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  mojo::Receiver<blink::mojom::AISummarizer> receiver_;

  const blink::mojom::AISummarizerCreateOptionsPtr options_;

  base::WeakPtrFactory<AISummarizer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_SUMMARIZER_H_
