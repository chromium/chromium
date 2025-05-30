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
      session_wrapper_(std::move(session)),
      options_(std::move(options)),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
}

AIRewriter::~AIRewriter() {
  for (auto& responder : responder_set_) {
    AIUtils::SendStreamingStatus(
        responder,
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
  auto request = BuildRequest(input, context.value_or(std::string()));
  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));

  session_wrapper_.session()->GetExecutionInputSizeInTokens(
      optimization_guide::MultimodalMessageReadView(request),
      base::BindOnce(&AIRewriter::DidGetExecutionInputSizeForRewrite,
                     weak_ptr_factory_.GetWeakPtr(), responder_id, request));
}

void AIRewriter::DidGetExecutionInputSizeForRewrite(
    mojo::RemoteSetElementId responder_id,
    const optimization_guide::proto::WritingAssistanceApiRequest& request,
    std::optional<uint32_t> result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    // It might be possible for the responder mojo connection to be closed
    // before this callback is invoked, in this case, we can't do anything.
    return;
  }

  if (!session_wrapper_.session()) {
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

  session_wrapper_.ExecuteModelOrQueue(
      optimization_guide::MultimodalMessage(request),
      base::BindRepeating(&AIRewriter::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), responder_id));
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
    AIUtils::SendStreamingStatus(
        responder,
        AIUtils::ConvertModelExecutionError(result.response.error().error()));
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::WritingAssistanceApiResponse>(
      result.response->response);
  if (response) {
    responder->OnStreaming(response->output());
  }
  if (result.response->is_complete) {
    responder->OnCompletion(/*context_info=*/nullptr);
    responder_set_.Remove(responder_id);
  }
}

void AIRewriter::MeasureUsage(const std::string& input,
                              const std::string& context,
                              MeasureUsageCallback callback) {
  auto* session = session_wrapper_.session();
  if (!session) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto request = BuildRequest(input, context);
  session->GetExecutionInputSizeInTokens(
      optimization_guide::MultimodalMessageReadView(request),
      base::BindOnce(&AIRewriter::DidGetExecutionInputSizeInTokensForMeasure,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AIRewriter::SetPriority(on_device_model::mojom::Priority priority) {
  auto* session = session_wrapper_.session();
  if (session) {
    session->SetPriority(priority);
  }
}

void AIRewriter::DidGetExecutionInputSizeInTokensForMeasure(
    MeasureUsageCallback callback,
    std::optional<uint32_t> result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(result.value());
}

optimization_guide::proto::WritingAssistanceApiRequest AIRewriter::BuildRequest(
    const std::string& input,
    const std::string& context) {
  optimization_guide::proto::WritingAssistanceApiRequest request;
  request.set_context(context);
  request.set_allocated_options(ToProtoOptions(options_).release());
  request.set_rewrite_text(input);
  // TODO(crbug.com/390006887): Pass shared context with session creation.
  request.set_shared_context(options_->shared_context.value_or(std::string()));
  return request;
}
