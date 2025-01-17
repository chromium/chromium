// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_rewriter.h"

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

optimization_guide::proto::WritingAssistanceApiOutputTone ToProtoTone(
    blink::mojom::AIRewriterTone type) {
  switch (type) {
    case blink::mojom::AIRewriterTone::kAsIs:
      // Rewriter config handles neutral tone semantically like "as-is".
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_TONE_NEUTRAL;
    case blink::mojom::AIRewriterTone::kMoreFormal:
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_TONE_FORMAL;
    case blink::mojom::AIRewriterTone::kMoreCasual:
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_TONE_CASUAL;
  }
}

optimization_guide::proto::WritingAssistanceApiOutputFormat ToProtoFormat(
    blink::mojom::AIRewriterFormat format) {
  switch (format) {
    case blink::mojom::AIRewriterFormat::kAsIs:
      // Rewriter config handles unspecified format by omitting instructions.
      NOTIMPLEMENTED() << "TODO: Improve AIRewriterFormat::kAsIs support";
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_FORMAT_NOT_SPECIFIED;
    case blink::mojom::AIRewriterFormat::kPlainText:
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_FORMAT_PLAIN_TEXT;
    case blink::mojom::AIRewriterFormat::kMarkdown:
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_FORMAT_MARKDOWN;
  }
}

optimization_guide::proto::WritingAssistanceApiOutputLength ToProtoLength(
    blink::mojom::AIRewriterLength length) {
  switch (length) {
    case blink::mojom::AIRewriterLength::kAsIs:
      // Rewriter config handles medium length semantically like "as-is".
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_LENGTH_MEDIUM;
    case blink::mojom::AIRewriterLength::kShorter:
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_LENGTH_SHORT;
    case blink::mojom::AIRewriterLength::kLonger:
      return optimization_guide::proto::
          WRITING_ASSISTANCE_API_OUTPUT_LENGTH_LONG;
  }
}

}  // namespace

AIRewriter::AIRewriter(
    AIContextBoundObjectSet& context_bound_object_set,
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session,
    blink::mojom::AIRewriterCreateOptionsPtr options,
    mojo::PendingReceiver<blink::mojom::AIRewriter> receiver)
    : AIContextBoundObject(context_bound_object_set),
      session_(std::move(session)),
      options_(std::move(options)),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
}

AIRewriter::~AIRewriter() {
  for (auto& responder : responder_set_) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
  }
}

// static
std::unique_ptr<optimization_guide::proto::WritingAssistanceApiOptions>
AIRewriter::ToProtoOptions(
    const blink::mojom::AIRewriterCreateOptionsPtr& options) {
  auto proto_options = std::make_unique<
      optimization_guide::proto::WritingAssistanceApiOptions>();
  proto_options->set_output_tone(ToProtoTone(options->tone));
  proto_options->set_output_format(ToProtoFormat(options->format));
  proto_options->set_output_length(ToProtoLength(options->length));
  return proto_options;
}

void AIRewriter::Rewrite(
    const std::string& input,
    const std::optional<std::string>& context,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  optimization_guide::proto::WritingAssistanceApiRequest request;
  request.set_context(context.value_or(std::string()));
  request.set_allocated_options(ToProtoOptions(options_).release());
  request.set_rewrite_text(input);
  // TODO(crbug.com/390006887): Pass shared context with session creation.
  request.set_shared_context(options_->shared_context.value_or(std::string()));
  session_->ExecuteModel(
      request,
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
    responder->OnError(
        AIUtils::ConvertModelExecutionError(result.response.error().error()));
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::WritingAssistanceApiResponse>(
      result.response->response);
  if (response) {
    responder->OnStreaming(
        response->output(),
        blink::mojom::ModelStreamingResponderAction::kReplace);
  }
  if (result.response->is_complete) {
    responder->OnCompletion(/*context_info=*/nullptr);
    responder_set_.Remove(responder_id);
  }
}
