// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_summarizer.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/summarize.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

AISummarizer::AISummarizer(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        summarize_session,
    mojo::PendingReceiver<blink::mojom::AISummarizer> receiver)
    : summarize_session_(std::move(summarize_session)),
      receiver_(this, std::move(receiver)) {}

AISummarizer::~AISummarizer() {
  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        std::nullopt, std::nullopt);
  }
}

void AISummarizer::SetDeletionCallback(base::OnceClosure deletion_callback) {
  receiver_.set_disconnect_handler(std::move(deletion_callback));
}

void AISummarizer::ModelExecutionCallback(
    const std::string& input,
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
        std::nullopt, std::nullopt);
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  if (response->has_value()) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                          response->value(), std::nullopt);
  }
  if (result.response->is_complete) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                          std::nullopt, std::nullopt);
  }
}

void AISummarizer::Summarize(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (!summarize_session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        std::nullopt, std::nullopt);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  optimization_guide::proto::SummarizeRequest request;
  request.set_article(input);
  summarize_session_->ExecuteModel(
      request,
      base::BindRepeating(&AISummarizer::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), input, responder_id));
}
