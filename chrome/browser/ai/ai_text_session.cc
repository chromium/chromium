// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session.h"

#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

// The format for the prompt and the context. The prompt structure helps the
// model distinguish the roles in the previous conversation.
const char kPromptFormat[] = "User: %s\nModel: ";
const char kContextFormat[] = "%s\n%s\n";
const char kSystemPromptFormat[] = "%s\n";

}  // namespace

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

AITextSession::Context::Context(uint32_t max_tokens,
                                std::optional<ContextItem> system_prompt)
    : max_tokens_(max_tokens), system_prompt_(system_prompt) {
  if (system_prompt.has_value()) {
    CHECK_GE(max_tokens_, system_prompt->tokens)
        << "the caller shouldn't create an AITextSession with the system "
           "prompt containing more tokens than the limit.";
    current_tokens_ += system_prompt->tokens;
  }
}

AITextSession::Context::Context(const Context& context) = default;

AITextSession::Context::~Context() = default;

void AITextSession::Context::AddContextItem(ContextItem context_item) {
  context_items_.emplace_back(context_item);
  current_tokens_ += context_item.tokens;
  while (current_tokens_ > max_tokens_) {
    current_tokens_ -= context_items_.begin()->tokens;
    context_items_.pop_front();
  }
}

std::string AITextSession::Context::GetContextString() {
  std::string context;
  if (system_prompt_.has_value()) {
    context =
        base::StringPrintf(kSystemPromptFormat, system_prompt_->text.c_str());
  }
  for (auto& context_item : context_items_) {
    context.append(context_item.text);
  }
  return context;
}

bool AITextSession::Context::HasContextItem() {
  return system_prompt_.has_value() || !context_items_.empty();
}

AITextSession::AITextSession(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session,
    base::WeakPtr<content::BrowserContext> browser_context,
    mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
    AIContextBoundObjectSet* context_bound_object_set,
    const std::optional<const Context>& context)
    : session_(std::move(session)),
      browser_context_(browser_context),
      context_bound_object_set_(context_bound_object_set),
      receiver_(this, std::move(receiver)) {
  if (context.has_value()) {
    // If the context is provided, it will be used in this session.
    context_ = std::make_unique<Context>(context.value());
    return;
  }

  // If the context is not provided, initialize a new context with the default
  // configuration.
  context_ = std::make_unique<Context>(
      session_->GetTokenLimits().max_context_tokens, std::nullopt);
}

AITextSession::~AITextSession() = default;

void AITextSession::SetSystemPrompt(std::string system_prompt,
                                    CreateTextSessionCallback callback) {
  session_->GetSizeInTokens(
      system_prompt,
      base::BindOnce(&AITextSession::InitializeContextWithSystemPrompt,
                     weak_ptr_factory_.GetWeakPtr(), system_prompt,
                     std::move(callback)));
}

void AITextSession::SetDeletionCallback(base::OnceClosure deletion_callback) {
  receiver_.set_disconnect_handler(std::move(deletion_callback));
}

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

void AITextSession::InitializeContextWithSystemPrompt(
    const std::string& text,
    CreateTextSessionCallback callback,
    uint32_t size) {
  // If the on device model service fails to get the size, it will be 0.
  // TODO(crbug.com/351935691): make sure the error is explicitly returned and
  // handled accordingly.
  if (!size) {
    std::move(callback).Run(/*info=*/nullptr);
    return;
  }

  uint32_t max_token = session_->GetTokenLimits().max_context_tokens;
  if (size > max_token) {
    // The session cannot be created if the system prompt contains more tokens
    // than the limit.
    std::move(callback).Run(/*info=*/nullptr);
    return;
  }

  context_ =
      std::make_unique<Context>(max_token, Context::ContextItem{text, size});
  std::move(callback).Run(GetTextSessionInfo());
}

void AITextSession::OnGetSizeInTokensComplete(
    const std::string& text,
    blink::mojom::ModelStreamingResponder* responder,
    uint32_t size) {
  // If the on device model service fails to get the size, it will be 0.
  // TODO(crbug.com/351935691): make sure the error is explicitly returned and
  // handled accordingly.
  if (size) {
    context_->AddContextItem({text, size});
  }
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                        std::nullopt, context_->current_tokens());
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
        /*text=*/std::nullopt, /*current_tokens=*/std::nullopt);
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  if (response->has_value()) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                          response->value(), /*current_tokens=*/std::nullopt);
  }
  if (result.response->is_complete) {
    std::string new_context = base::StringPrintf(kContextFormat, input.c_str(),
                                                 response->value().c_str());
    // TODO(crbug.com/351935390): instead of calculating this from the
    // AITextSession, it should be returned by the model since the token should
    // be calculated during the execution.
    session_->GetSizeInTokens(
        new_context,
        base::BindOnce(&AITextSession::OnGetSizeInTokensComplete,
                       weak_ptr_factory_.GetWeakPtr(), new_context, responder));
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
        /*text=*/std::nullopt, /*current_tokens=*/std::nullopt);
    return;
  }

  if (context_->HasContextItem()) {
    optimization_guide::proto::StringValue context;
    context.set_value(context_->GetContextString());
    session_->AddContext(context);
  }

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

void AITextSession::Fork(
    mojo::PendingReceiver<blink::mojom::AITextSession> session,
    ForkCallback callback) {
  if (!browser_context_) {
    // The `browser_context_` is already destroyed before the renderer owner is
    // gone.
    std::move(callback).Run(nullptr);
    return;
  }

  AIManagerKeyedService* service =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
          browser_context_.get());

  const optimization_guide::SamplingParams sampling_param =
      session_->GetSamplingParams();

  service->CreateTextSessionForCloning(
      base::PassKey<AITextSession>(), std::move(session),
      blink::mojom::AITextSessionSamplingParams::New(
          sampling_param.top_k, sampling_param.temperature),
      context_bound_object_set_, *context_, std::move(callback));
}

void AITextSession::Destroy() {
  if (session_) {
    session_.reset();
  }

  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*text=*/std::nullopt, /*current_tokens=*/std::nullopt);
  }

  responder_set_.Clear();
}

blink::mojom::AITextSessionInfoPtr AITextSession::GetTextSessionInfo() {
  const optimization_guide::SamplingParams session_sampling_params =
      session_->GetSamplingParams();
  return blink::mojom::AITextSessionInfo::New(
      context_->max_tokens(),
      blink::mojom::AITextSessionSamplingParams::New(
          session_sampling_params.top_k, session_sampling_params.temperature));
}
