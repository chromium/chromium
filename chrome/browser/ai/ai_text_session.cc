// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-shared.h"

namespace {

// The format for the prompt and the context. The prompt structure helps the
// model distinguish the roles in the previous conversation.
const char kPromptFormat[] = "User: %s\nModel: ";
const char kContextFormat[] = "%s\n%s\n";

}  // namespace

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

AITextSession::Context::Context(uint32_t max_tokens)
    : max_tokens_(max_tokens) {}

AITextSession::Context::Context(const Context& context) = default;

AITextSession::Context::~Context() = default;

void AITextSession::Context::AddContextItem(const std::string& text,
                                            uint32_t size) {
  context_item_.emplace_back(text, size);
  current_tokens_ += size;
  while (current_tokens_ > max_tokens_) {
    current_tokens_ -= context_item_.begin()->tokens;
    context_item_.pop_front();
  }
}

std::string AITextSession::Context::GetContextString() {
  std::string context;
  for (auto& context_item : context_item_) {
    context.append(context_item.text);
  }
  return context;
}

bool AITextSession::Context::HasContextItem() {
  return !context_item_.empty();
}

AITextSession::AITextSession(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session,
    std::optional<optimization_guide::SamplingParams> sampling_params)
    : session_(std::move(session)),
      sampling_params_(sampling_params),
      context_(
          optimization_guide::features::GetOnDeviceModelMaxTokensForContext()) {
}

AITextSession::AITextSession(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session,
    std::optional<optimization_guide::SamplingParams> sampling_params,
    Context context)
    : session_(std::move(session)),
      sampling_params_(sampling_params),
      context_(context) {}

AITextSession::~AITextSession() = default;

blink::mojom::ModelStreamingResponseStatus ConvertModelExecutionError(
    ModelExecutionError error) {
  switch (error) {
    case ModelExecutionError::kUnknown:
      return blink::mojom::ModelStreamingResponseStatus::kErrorUnknown;
    case ModelExecutionError::kInvalidRequest:
      return blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest;
    case ModelExecutionError::kRequestThrottled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorRequestThrottled;
    case ModelExecutionError::kPermissionDenied:
      return blink::mojom::ModelStreamingResponseStatus::kErrorPermissionDenied;
    case ModelExecutionError::kGenericFailure:
      return blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure;
    case ModelExecutionError::kRetryableError:
      return blink::mojom::ModelStreamingResponseStatus::kErrorRetryableError;
    case ModelExecutionError::kNonRetryableError:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorNonRetryableError;
    case ModelExecutionError::kUnsupportedLanguage:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorUnsupportedLanguage;
    case ModelExecutionError::kFiltered:
      return blink::mojom::ModelStreamingResponseStatus::kErrorFiltered;
    case ModelExecutionError::kDisabled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorDisabled;
    case ModelExecutionError::kCancelled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorCancelled;
  }
}

void AITextSession::GetContextSizeInTokensCallback(const std::string& text,
                                                   uint32_t size) {
  // If the on device model service fails to get the size, it will be 0.
  // TODO(crbug.com/351935691): make sure the error is explicitly returned and
  // handled accordingly.
  if (size) {
    context_.AddContextItem(text, size);
  }
}

void AITextSession::AppendContext(std::string& text) {
  // TODO(crbug.com/351935390): instead of calculating this from the
  // AITextSession, it should be returned by the model since the token should be
  // calculated during the execution.
  session_->GetSizeInTokens(
      text, base::BindOnce(&AITextSession::GetContextSizeInTokensCallback,
                           weak_ptr_factory_.GetWeakPtr(), text));
}

void AITextSession::ModelExecutionCallback(
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
        ConvertModelExecutionError(result.response.error().error()),
        std::nullopt);
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  if (response->has_value()) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                          response->value());
  }
  if (result.response->is_complete) {
    std::string new_context = base::StringPrintf(kContextFormat, input.c_str(),
                                                 response->value().c_str());
    AppendContext(new_context);
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                          std::nullopt);
  }
}

void AITextSession::Prompt(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (!session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        std::nullopt);
    return;
  }

  // TODO(crbug.com/342069219): un-comment this when the model proto is updated,
  // and add the logic to determine if context handling is supported in the
  // current version of the model.
  // if (context_.HasContextItem()) {
  //   optimization_guide::proto::StringValue context;
  //   context.set_value(context_.GetContextString());
  //   session_->AddContext(context);
  // }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  optimization_guide::proto::StringValue request;
  const std::string formatted_input =
      base::StringPrintf(kPromptFormat, input.c_str());
  request.set_value(formatted_input);
  session_->ExecuteModel(
      request, base::BindRepeating(&AITextSession::ModelExecutionCallback,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   formatted_input, responder_id));
}

void AITextSession::Destroy() {
  if (session_) {
    session_.reset();
  }

  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        std::nullopt);
  }

  responder_set_.Clear();
}
