// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_PROOFREADER_H_
#define CHROME_BROWSER_AI_AI_PROOFREADER_H_

#include <optional>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_on_device_session.h"
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
  void GetCorrectionsTypes(
      const std::string& correction_instructions,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder) override;

  // AIContextBoundObject:
  void SetPriority(on_device_model::mojom::Priority priority) override;

  ~AIProofreader() override;

  static std::unique_ptr<optimization_guide::proto::ProofreadOptions>
  ToProtoOptions(const blink::mojom::AIProofreaderCreateOptionsPtr& options);

  // Returns a set of BCP 47 base language codes that are supported and enabled,
  // or nullopt if all languages are enabled (e.g. via local flags).
  static std::optional<base::flat_set<std::string>>
  GetEnabledLanguageBaseCodes();
  static base::flat_set<std::string> GetDefaultSupportedLanguageBaseCodes();

 private:
  friend class AITestUtils;

  void StartExecution(const std::string& input,
                      const std::string& serialized_corrections,
                      bool is_label_mode,
                      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                          pending_responder);

  void DidGetExecutionInputSizeForProofread(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::proto::ProofreaderApiRequest request,
      bool is_label_mode,
      std::optional<uint32_t> result);

  void ModelExecutionCallback(
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // Builds a request for the model with two primary modes:
  //
  // - To explain a list of corrections: When
  //   `serialized_corrections` are provided in the format
  //   "["Correcting `error_0` to `correction_0`", ...]", the request asks the
  //   model to return the types of the list of corrections.
  //
  // - To proofread text: Otherwise, the request asks the model to return the
  //   fully corrected text of the input.
  optimization_guide::proto::ProofreaderApiRequest BuildRequest(
      const std::string& input,
      const std::string& serialized_corrections);

  // The underlying session provided by optimization guide component.
  AIOnDeviceSession session_wrapper_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Proofread()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  mojo::Receiver<blink::mojom::AIProofreader> receiver_;

  const blink::mojom::AIProofreaderCreateOptionsPtr options_;

  base::WeakPtrFactory<AIProofreader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_PROOFREADER_H_
