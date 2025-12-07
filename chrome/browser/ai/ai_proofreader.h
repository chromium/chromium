// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_PROOFREADER_H_
#define CHROME_BROWSER_AI_AI_PROOFREADER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/proto/features/proofreader_api.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_proofreader.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

class AIProofreader : public AIContextBoundObject,
                      public blink::mojom::AIProofreader {
 public:
  AIProofreader(
      AIContextBoundObjectSet& context_bound_object_set,
      std::unique_ptr<optimization_guide::OnDeviceSession> proofread_session,
      blink::mojom::AIProofreaderCreateOptionsPtr options,
      mojo::PendingReceiver<blink::mojom::AIProofreader> receiver);

  // `blink::mojom::AIProofreader` implementation.
  void Proofread(const std::string& input,
                 mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                     pending_responder) override;
  void GetCorrectionType(
      const std::string& input,
      const std::string& corrected_input,
      const std::string& correction_instruction,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder) override;

  // AIContextBoundObject:
  void SetPriority(on_device_model::mojom::Priority priority) override;

  ~AIProofreader() override;

  static std::unique_ptr<optimization_guide::proto::ProofreadOptions>
  ToProtoOptions(const blink::mojom::AIProofreaderCreateOptionsPtr& options);

 private:
  friend class AITestUtils;

  void StartExecution(const std::string& input,
                      const std::string& corrected_input,
                      const std::string& correction_instruction,
                      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                          pending_responder);

  void DidGetExecutionInputSizeForProofread(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::proto::ProofreaderApiRequest request,
      std::optional<uint32_t> result);

  void ModelExecutionCallback(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // Builds a request for the model with two primary modes:
  //
  // - To explain a correction: When `corrected_input` and
  //   `correction_instruction` are provided, the request asks the model to
  //   return the type of the correction.
  //
  // - To proofread text: Otherwise, the request asks the model to return the
  //   fully corrected text of the input.
  optimization_guide::proto::ProofreaderApiRequest BuildRequest(
      const std::string& input,
      const std::string& corrected_input,
      const std::string& correction_instruction);

  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OnDeviceSession> session_;
  mojo::Remote<blink::mojom::AIProofreader> remote_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Proofread()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  mojo::Receiver<blink::mojom::AIProofreader> receiver_;

  const blink::mojom::AIProofreaderCreateOptionsPtr options_;

  base::WeakPtrFactory<AIProofreader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_PROOFREADER_H_
