// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_proofreader.h"

#include "base/containers/fixed_flat_set.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/on_device_ai/ai_utils.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/proofreader_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

// static
std::optional<base::flat_set<std::string>>
AIProofreader::GetEnabledLanguageBaseCodes() {
  const base::FeatureParam<std::string> kAIProofreaderLanguagesEnabled{
      &blink::features::kAIProofreadingAPI, "langs", "en"};
  return on_device_ai::GetEnabledLanguagesForFeature(
      GetDefaultSupportedLanguageBaseCodes(), kAIProofreaderLanguagesEnabled);
}

// static
base::flat_set<std::string>
AIProofreader::GetDefaultSupportedLanguageBaseCodes() {
  auto kSupportedBaseLanguages =
      base::MakeFixedFlatSet<std::string_view>({"en"});
  return base::flat_set<std::string>(kSupportedBaseLanguages.begin(),
                                     kSupportedBaseLanguages.end());
}

on_device_model::mojom::ResponseConstraintPtr GetConstraint(
    const optimization_guide::OnDeviceSession* session) {
  if (!session) {
    return nullptr;
  }
  const auto& metadata = session->GetOnDeviceFeatureMetadata();
  auto proofreader_metadata = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ProofreaderApiMetadata>(metadata);
  if (!proofreader_metadata || !proofreader_metadata->has_constraints()) {
    return nullptr;
  }
  const auto& constraints = proofreader_metadata->constraints();
  if (constraints.has_label_mode_constraint()) {
    const auto& constraint = constraints.label_mode_constraint();
    return ai::ToMojomResponseConstraint(constraint);
  }
  return nullptr;
}

AIProofreader::AIProofreader(
    AIContextBoundObjectSet& context_bound_object_set,
    std::unique_ptr<optimization_guide::OnDeviceSession> session,
    blink::mojom::AIProofreaderCreateOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::AIProofreader> receiver)
    : AIContextBoundObject(context_bound_object_set),
      session_wrapper_(std::move(session)),
      receiver_(this, std::move(receiver)),
      options_(std::move(options)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
}

AIProofreader::~AIProofreader() {
  for (auto& responder : responder_set_) {
    on_device_ai::SendStreamingStatus(
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
  StartExecution(input, /*serialized_corrections=*/std::string(),
                 /*is_label_mode=*/false, std::move(pending_responder));
}

void AIProofreader::GetCorrectionsTypes(
    const std::string& correction_instructions,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  StartExecution(/*input=*/std::string(), correction_instructions,
                 /*is_label_mode=*/true, std::move(pending_responder));
}

void AIProofreader::SetPriority(on_device_model::mojom::Priority priority) {
  auto* session = session_wrapper_.session();
  if (session) {
    session->SetPriority(priority);
  }
}

void AIProofreader::StartExecution(
    const std::string& input,
    const std::string& serialized_corrections,
    bool is_label_mode,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  auto* session = session_wrapper_.session();
  if (!session) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    on_device_ai::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  auto request = BuildRequest(input, serialized_corrections);

  session->GetExecutionInputSizeInTokens(
      optimization_guide::MultimodalMessageReadView(request),
      base::BindOnce(&AIProofreader::DidGetExecutionInputSizeForProofread,
                     weak_ptr_factory_.GetWeakPtr(), responder_id, request,
                     is_label_mode));
}

void AIProofreader::DidGetExecutionInputSizeForProofread(
    mojo::RemoteSetElementId responder_id,
    optimization_guide::proto::ProofreaderApiRequest request,
    bool is_label_mode,
    std::optional<uint32_t> result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    // It might be possible for the responder mojo connection to be closed
    // before this callback is invoked, in this case, we can't do anything.
    return;
  }

  // TODO(crbug.com/494980521): Catch real crash disconnects to surface errors.
  if (!session_wrapper_.session()) {
    on_device_ai::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  if (!result.has_value()) {
    on_device_ai::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorFailedToCountTokens);
    return;
  }

  uint32_t quota = blink::mojom::kWritingAssistanceMaxInputTokenSize;
  if (result.value() > quota) {
    on_device_ai::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(result.value(), quota));
    return;
  }

  on_device_model::mojom::ResponseConstraintPtr constraint =
      is_label_mode ? GetConstraint(session_wrapper_.session()) : nullptr;

  session_wrapper_.ExecuteModelOrQueue(
      optimization_guide::MultimodalMessage(request),
      base::BindRepeating(&AIProofreader::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), responder_id),
      std::move(constraint));
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
    on_device_ai::SendStreamingStatus(
        responder, on_device_ai::ConvertOnDeviceError(result.response.error()));
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
    const std::string& serialized_corrections) {
  optimization_guide::proto::ProofreaderApiRequest request;
  request.set_text(input);
  request.set_correction(serialized_corrections);
  request.set_allocated_options(
      AIProofreader::ToProtoOptions(options_).release());
  return request;
}
