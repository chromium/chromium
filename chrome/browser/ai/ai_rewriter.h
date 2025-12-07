// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_REWRITER_H_
#define CHROME_BROWSER_AI_AI_REWRITER_H_

#include <optional>
#include <string>

#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_on_device_session.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/proto/features/writing_assistance_api.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

// The implementation of `blink::mojom::AIRewriter`, which exposes the single
// stream-based `Rewrite()` API.
// TODO(crbug.com/402442890): Refactor Writing Assistance APIs to reduce
// duplicated code.
class AIRewriter : public AIContextBoundObject,
                   public blink::mojom::AIRewriter {
 public:
  AIRewriter(AIContextBoundObjectSet& context_bound_object_set,
             std::unique_ptr<optimization_guide::OnDeviceSession> session,
             blink::mojom::AIRewriterCreateOptionsPtr options,
             mojo::PendingReceiver<blink::mojom::AIRewriter> receiver);
  AIRewriter(const AIRewriter&) = delete;
  AIRewriter& operator=(const AIRewriter&) = delete;
  ~AIRewriter() override;

  static std::unique_ptr<optimization_guide::proto::WritingAssistanceApiOptions>
  ToProtoOptions(const blink::mojom::AIRewriterCreateOptionsPtr& options);

  // Returns a set of BCP 47 base language codes that are supported and enabled.
  static base::flat_set<std::string_view> GetSupportedLanguageBaseCodes();

  // `blink::mojom::AIRewriter` implementation.
  void Rewrite(const std::string& input,
               const std::optional<std::string>& context,
               mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                   pending_responder) override;
  void MeasureUsage(const std::string& input,
                    const std::string& context,
                    MeasureUsageCallback callback) override;

  // AIContextBoundObject:
  void SetPriority(on_device_model::mojom::Priority priority) override;

 private:
  void DidGetExecutionInputSizeForRewrite(
      mojo::RemoteSetElementId responder_id,
      const optimization_guide::proto::WritingAssistanceApiRequest& request,
      std::optional<uint32_t> result);

  void DidGetExecutionInputSizeInTokensForMeasure(
      MeasureUsageCallback callback,
      std::optional<uint32_t> result);

  void ModelExecutionCallback(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  optimization_guide::proto::WritingAssistanceApiRequest BuildRequest(
      const std::string& input,
      const std::string& context);

  AIOnDeviceSession session_wrapper_;

  const blink::mojom::AIRewriterCreateOptionsPtr options_;

  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  mojo::Receiver<blink::mojom::AIRewriter> receiver_;

  base::WeakPtrFactory<AIRewriter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_REWRITER_H_
