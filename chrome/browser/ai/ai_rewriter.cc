// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_rewriter.h"

#include "base/functional/bind.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

AIRewriter::AIRewriter(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session,
    const std::optional<std::string>& shared_context,
    blink::mojom::AIRewriterTone tone,
    blink::mojom::AIRewriterLength length)
    : session_(std::move(session)),
      shared_context_(shared_context),
      tone_(tone),
      length_(length) {}

AIRewriter::~AIRewriter() {
  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*text=*/std::nullopt, /*current_tokens=*/std::nullopt);
  }
}

void AIRewriter::Rewrite(
    const std::string& input,
    const std::optional<std::string>& context,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  optimization_guide::proto::ComposePageMetadata page_metadata;
  const std::string context_string = base::JoinString(
      {shared_context_.value_or(""), context.value_or("")}, "\n");
  page_metadata.set_trimmed_page_inner_text(
      context_string.substr(0, AIUtils::kTrimmedInnerTextMaxChars));
  page_metadata.set_page_inner_text(context_string);

  optimization_guide::proto::ComposeRequest context_request;
  *context_request.mutable_page_metadata() = std::move(page_metadata);

  session_->AddContext(context_request);

  optimization_guide::proto::ComposeRequest execute_request;
  // TODO(crbug.com/358214322): We don't support the combination of tone and
  // length.
  if (tone_ == blink::mojom::AIRewriterTone::kMoreFormal) {
    execute_request.mutable_rewrite_params()->set_tone(
        optimization_guide::proto::ComposeTone::COMPOSE_FORMAL);
  } else if (tone_ == blink::mojom::AIRewriterTone::kMoreCasual) {
    execute_request.mutable_rewrite_params()->set_tone(
        optimization_guide::proto::ComposeTone::COMPOSE_INFORMAL);
  } else if (length_ == blink::mojom::AIRewriterLength::kLonger) {
    execute_request.mutable_rewrite_params()->set_length(
        optimization_guide::proto::ComposeLength::COMPOSE_LONGER);
  } else if (length_ == blink::mojom::AIRewriterLength::kShorter) {
    execute_request.mutable_rewrite_params()->set_length(
        optimization_guide::proto::ComposeLength::COMPOSE_SHORTER);
  } else {
    execute_request.mutable_rewrite_params()->set_regenerate(true);
  }
  execute_request.mutable_rewrite_params()->set_previous_response(input);

  session_->ExecuteModel(
      execute_request,
      base::BindRepeating(&AIRewriter::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(),
                          responder_set_.Add(std::move(pending_responder))));
}

void AIRewriter::ModelExecutionCallback(
    mojo::RemoteSetElementId responder_id,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }
  if (!result.response.has_value()) {
    responder->OnResponse(
        AIUtils::ConvertModelExecutionError(result.response.error().error()),
        /*text=*/std::nullopt, /*current_tokens=*/std::nullopt);
    return;
  }

  auto compose_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ComposeResponse>(result.response->response);
  if (compose_response) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                          compose_response->output(),
                          /*current_tokens=*/std::nullopt);
  }
  if (result.response->is_complete) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                          /*text=*/std::nullopt,
                          /*current_tokens=*/std::nullopt);
    responder_set_.Remove(responder_id);
  }
}
