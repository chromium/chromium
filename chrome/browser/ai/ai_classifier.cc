// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_classifier.h"

#include "base/functional/bind.h"
#include "chrome/browser/ai/ai_on_device_session.h"
#include "components/on_device_ai/ai_utils.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/classify_api.pb.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

AIClassifier::AIClassifier(
    AIContextBoundObjectSet& context_bound_object_set,
    std::unique_ptr<optimization_guide::OnDeviceSession> classifier_session,
    blink::mojom::AIClassifierCreateOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::AIClassifier> receiver)
    : AIContextBoundObject(context_bound_object_set),
      session_wrapper_(std::move(classifier_session)),
      receiver_(this, std::move(receiver)),
      options_(std::move(options)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
}

AIClassifier::~AIClassifier() {
  for (auto& responder : responder_set_) {
    on_device_ai::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
  }
}

optimization_guide::proto::ClassifyApiRequest AIClassifier::BuildRequest(
    const std::string& input) {
  optimization_guide::proto::ClassifyApiRequest request;
  request.set_text(input);
  return request;
}

// TODO(crbug.com/485366700): Implement functionality for context.
void AIClassifier::Classify(
    const std::string& input,
    const std::string& /*context*/,
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
  auto request = BuildRequest(input);
  session->GetExecutionInputSizeInTokens(
      optimization_guide::MultimodalMessageReadView(request),
      base::BindOnce(&AIClassifier::DidGetExecutionInputSizeForClassify,
                     weak_ptr_factory_.GetWeakPtr(), responder_id, request));
}

void AIClassifier::DidGetExecutionInputSizeForClassify(
    mojo::RemoteSetElementId responder_id,
    const optimization_guide::proto::ClassifyApiRequest& request,
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

  session_wrapper_.ExecuteModelOrQueue(
      optimization_guide::MultimodalMessage(request),
      base::BindRepeating(&AIClassifier::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), responder_id));
}

void AIClassifier::SetPriority(on_device_model::mojom::Priority priority) {
  if (session_wrapper_.session()) {
    session_wrapper_.session()->SetPriority(priority);
  }
}

void AIClassifier::ModelExecutionCallback(
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
      optimization_guide::proto::ClassifyApiResponse>(
      result.response->response);
  if (response) {
    responder->OnStreaming(response->output());
  }

  if (result.response->is_complete) {
    responder->OnCompletion(/*context_info=*/nullptr);
  }
}
