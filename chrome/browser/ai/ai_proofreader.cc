// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_proofreader.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/proofreader_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

AIProofreader::AIProofreader(
    AIContextBoundObjectSet& context_bound_object_set,
    std::unique_ptr<optimization_guide::OnDeviceSession> session,
    blink::mojom::AIProofreaderCreateOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::AIProofreader> receiver)
    : AIContextBoundObject(context_bound_object_set),
      session_(std::move(session)),
      receiver_(this, std::move(receiver)),
      options_(std::move(options)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
}

AIProofreader::~AIProofreader() {
  for (auto& responder : responder_set_) {
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
  }
}

// static
std::unique_ptr<optimization_guide::proto::ProofreadOptions>
AIProofreader::ToProtoOptions(
    const blink::mojom::AIProofreaderCreateOptionsPtr& options) {
  auto proto_options =
      std::make_unique<optimization_guide::proto::ProofreadOptions>();
  proto_options->set_include_correction_types(
      options->include_correction_types);
  proto_options->set_include_correction_explanation(
      options->include_correction_explanations);
  return proto_options;
}

void AIProofreader::Proofread(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  StartExecution(input, /*corrected_input=*/"", /*correction_instruction=*/"",
                 std::move(pending_responder));
}

void AIProofreader::GetCorrectionType(
    const std::string& input,
    const std::string& corrected_input,
    const std::string& correction_instruction,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  StartExecution(input, corrected_input, correction_instruction,
                 std::move(pending_responder));
}

void AIProofreader::SetPriority(on_device_model::mojom::Priority priority) {
  if (session_) {
    session_->SetPriority(priority);
  }
}

void AIProofreader::StartExecution(
    const std::string& input,
    const std::string& corrected_input,
    const std::string& correction_instruction,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (!session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  auto request = BuildRequest(input, corrected_input, correction_instruction);

  session_->GetExecutionInputSizeInTokens(
      optimization_guide::MultimodalMessageReadView(request),
      base::BindOnce(&AIProofreader::DidGetExecutionInputSizeForProofread,
                     weak_ptr_factory_.GetWeakPtr(), responder_id, request));
}

void AIProofreader::DidGetExecutionInputSizeForProofread(
    mojo::RemoteSetElementId responder_id,
    optimization_guide::proto::ProofreaderApiRequest request,
    std::optional<uint32_t> result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    // It might be possible for the responder mojo connection to be closed
    // before this callback is invoked, in this case, we can't do anything.
    return;
  }

  if (!session_) {
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  if (!result.has_value()) {
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);
    return;
  }

  uint32_t quota = blink::mojom::kWritingAssistanceMaxInputTokenSize;
  if (result.value() > quota) {
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(result.value(), quota));
    return;
  }

  session_->ExecuteModel(
      request,
      base::BindRepeating(&AIProofreader::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), responder_id));
}

void AIProofreader::ModelExecutionCallback(
    mojo::RemoteSetElementId responder_id,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  if (!result.response.has_value()) {
    AIUtils::SendStreamingStatus(
        responder,
        AIUtils::ConvertModelExecutionError(result.response.error().error()));
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ProofreaderApiResponse>(
      result.response->response);
  if (response) {
    responder->OnStreaming(response->output());
  }
  if (result.response->is_complete) {
    responder->OnCompletion(/*context_info=*/nullptr);
    responder_set_.Remove(responder_id);
  }
}

optimization_guide::proto::ProofreaderApiRequest AIProofreader::BuildRequest(
    const std::string& input,
    const std::string& corrected_input,
    const std::string& correction_instruction) {
  optimization_guide::proto::ProofreaderApiRequest request;
  request.set_text(input);
  request.set_corrected_text(corrected_input);
  request.set_correction(correction_instruction);
  request.set_allocated_options(
      AIProofreader::ToProtoOptions(options_).release());
  return request;
}
