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

namespace {

optimization_guide::proto::SummarizerOutputType ToProtoOutputType(
    blink::mojom::AISummarizerType type) {
  switch (type) {
    case blink::mojom::AISummarizerType::kTLDR:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_TYPE_TL_DR;
    case blink::mojom::AISummarizerType::kKeyPoints:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_TYPE_KEYPOINTS;
    case blink::mojom::AISummarizerType::kTeaser:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_TYPE_TEASER;
    case blink::mojom::AISummarizerType::kHeadline:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_TYPE_HEADLINES;
  }
}

optimization_guide::proto::SummarizerOutputFormat ToProtoOutputFormat(
    blink::mojom::AISummarizerFormat format) {
  switch (format) {
    case blink::mojom::AISummarizerFormat::kPlainText:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_FORMAT_PLAIN_TEXT;
    case blink::mojom::AISummarizerFormat::kMarkDown:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_FORMAT_MARKDOWN;
  }
}

optimization_guide::proto::SummarizerOutputLength ToProtoOutputLength(
    blink::mojom::AISummarizerLength length) {
  switch (length) {
    case blink::mojom::AISummarizerLength::kShort:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_LENGTH_SHORT;
    case blink::mojom::AISummarizerLength::kMedium:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_LENGTH_MEDIUM;
    case blink::mojom::AISummarizerLength::kLong:
      return optimization_guide::proto::SUMMARIZER_OUTPUT_LENGTH_LONG;
  }
}

}  // namespace

AISummarizer::AISummarizer(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        summarize_session,
    blink::mojom::AISummarizerCreateOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::AISummarizer> receiver)
    : summarize_session_(std::move(summarize_session)),
      receiver_(this, std::move(receiver)),
      options_(std::move(options)) {}

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
    const std::string& context,
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
  request.mutable_options()->set_output_type(ToProtoOutputType(options_->type));
  request.mutable_options()->set_output_format(
      ToProtoOutputFormat(options_->format));
  request.mutable_options()->set_output_length(
      ToProtoOutputLength(options_->length));
  std::string final_context = options_->shared_context.value_or("");
  if (!context.empty()) {
    if (!final_context.empty()) {
      final_context = final_context + " " + context;
    } else {
      final_context = context;
    }
  }
  if (!final_context.empty()) {
    final_context += "\n";
  }
  request.set_context(final_context);
  summarize_session_->ExecuteModel(
      request,
      base::BindRepeating(&AISummarizer::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), input, responder_id));
}
