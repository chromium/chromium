// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
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
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

using optimization_guide::proto::PromptApiMetadata;
using optimization_guide::proto::PromptApiPrompt;
using optimization_guide::proto::PromptApiRequest;
using optimization_guide::proto::PromptApiRole;

PromptApiRole ConvertRole(blink::mojom::AIAssistantInitialPromptRole role) {
  switch (role) {
    case blink::mojom::AIAssistantInitialPromptRole::kSystem:
      return PromptApiRole::PROMPT_API_ROLE_SYSTEM;
    case blink::mojom::AIAssistantInitialPromptRole::kUser:
      return PromptApiRole::PROMPT_API_ROLE_USER;
    case blink::mojom::AIAssistantInitialPromptRole::kAssistant:
      return PromptApiRole::PROMPT_API_ROLE_ASSISTANT;
  }
}

PromptApiPrompt MakePrompt(PromptApiRole role, const std::string& content) {
  PromptApiPrompt prompt;
  prompt.set_role(role);
  prompt.set_content(content);
  return prompt;
}

const char* FormatPromptRole(PromptApiRole role) {
  switch (role) {
    case PromptApiRole::PROMPT_API_ROLE_SYSTEM:
      return "";  // No prefix for system prompt.
    case PromptApiRole::PROMPT_API_ROLE_USER:
      return "User: ";
    case PromptApiRole::PROMPT_API_ROLE_ASSISTANT:
      return "Model: ";
    default:
      NOTREACHED();
  }
}

PromptApiMetadata ParseMetadata(const optimization_guide::proto::Any& any) {
  PromptApiMetadata metadata;
  if (any.type_url() == "type.googleapis.com/" + metadata.GetTypeName()) {
    metadata.ParseFromString(any.value());
  }
  return metadata;
}

std::unique_ptr<optimization_guide::proto::StringValue> ToStringValue(
    const PromptApiRequest& request) {
  std::ostringstream oss;
  auto FormatPrompts =
      [](std::ostringstream& oss,
         const google::protobuf::RepeatedPtrField<PromptApiPrompt> prompts) {
        for (const auto& prompt : prompts) {
          oss << FormatPromptRole(prompt.role()) << prompt.content() << "\n";
        }
      };
  FormatPrompts(oss, request.initial_prompts());
  FormatPrompts(oss, request.prompt_history());
  FormatPrompts(oss, request.current_prompts());
  if (request.current_prompts_size() > 0) {
    oss << FormatPromptRole(PromptApiRole::PROMPT_API_ROLE_ASSISTANT);
  }
  auto value = std::make_unique<optimization_guide::proto::StringValue>();
  value->set_value(oss.str());
  return value;
}

}  // namespace

AITextSession::Context::ContextItem::ContextItem() = default;
AITextSession::Context::ContextItem::ContextItem(const ContextItem&) = default;
AITextSession::Context::ContextItem::ContextItem(ContextItem&&) = default;
AITextSession::Context::ContextItem::~ContextItem() = default;

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

AITextSession::Context::Context(uint32_t max_tokens,
                                ContextItem initial_prompts,
                                bool use_prompt_api_proto)
    : max_tokens_(max_tokens),
      initial_prompts_(std::move(initial_prompts)),
      use_prompt_api_proto_(use_prompt_api_proto) {
  CHECK_GE(max_tokens_, initial_prompts_.tokens)
      << "the caller shouldn't create an AITextSession with the initial "
         "prompts containing more tokens than the limit.";
  current_tokens_ += initial_prompts.tokens;
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

std::unique_ptr<google::protobuf::MessageLite>
AITextSession::Context::MaybeFormatRequest(PromptApiRequest request) {
  if (use_prompt_api_proto_) {
    return std::make_unique<PromptApiRequest>(std::move(request));
  }
  return ToStringValue(request);
}

std::unique_ptr<google::protobuf::MessageLite>
AITextSession::Context::MakeRequest() {
  PromptApiRequest request;
  request.mutable_initial_prompts()->MergeFrom(initial_prompts_.prompts);
  for (auto& context_item : context_items_) {
    request.mutable_prompt_history()->MergeFrom((context_item.prompts));
  }
  return MaybeFormatRequest(std::move(request));
}

bool AITextSession::Context::HasContextItem() {
  return current_tokens_;
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
      session_->GetTokenLimits().max_context_tokens, Context::ContextItem(),
      ParseMetadata(session_->GetOnDeviceFeatureMetadata()).version() >= 1);
}

AITextSession::~AITextSession() = default;

void AITextSession::SetInitialPrompts(
    const std::optional<std::string> system_prompt,
    std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts,
    CreateTextSessionCallback callback) {
  PromptApiRequest request;
  if (system_prompt) {
    *request.add_initial_prompts() =
        MakePrompt(PromptApiRole::PROMPT_API_ROLE_SYSTEM, *system_prompt);
  }
  for (const auto& prompt : initial_prompts) {
    *request.add_initial_prompts() =
        MakePrompt(ConvertRole(prompt->role), prompt->content);
  }
  session_->GetContextSizeInTokens(
      *context_->MaybeFormatRequest(request),
      base::BindOnce(&AITextSession::InitializeContextWithInitialPrompts,
                     weak_ptr_factory_.GetWeakPtr(), request,
                     std::move(callback)));
}

void AITextSession::SetDeletionCallback(base::OnceClosure deletion_callback) {
  receiver_.set_disconnect_handler(std::move(deletion_callback));
}

void AITextSession::InitializeContextWithInitialPrompts(
    optimization_guide::proto::PromptApiRequest initial_request,
    CreateTextSessionCallback callback,
    uint32_t size) {
  // If the on device model service fails to get the size, it will be 0.
  // TODO(crbug.com/351935691): make sure the error is explicitly returned and
  // handled accordingly.
  if (!size) {
    std::move(callback).Run(/*info=*/nullptr);
    return;
  }

  uint32_t max_token = context_->max_tokens();
  if (size > max_token) {
    // The session cannot be created if the system prompt contains more tokens
    // than the limit.
    std::move(callback).Run(/*info=*/nullptr);
    return;
  }

  auto initial_prompts = Context::ContextItem();
  initial_prompts.tokens = size;
  initial_prompts.prompts.Swap(initial_request.mutable_initial_prompts());
  context_ = std::make_unique<Context>(max_token, std::move(initial_prompts),
                                       context_->use_prompt_api_proto());
  std::move(callback).Run(GetTextSessionInfo());
}

void AITextSession::AddPromptHistoryAndSendCompletion(
    const PromptApiRequest& history_request,
    blink::mojom::ModelStreamingResponder* responder,
    uint32_t size) {
  // If the on device model service fails to get the size, it will be 0.
  // TODO(crbug.com/351935691): make sure the error is explicitly returned and
  // handled accordingly.
  if (size) {
    auto item = Context::ContextItem();
    item.tokens = size;
    item.prompts = history_request.prompt_history();
    context_->AddContextItem(std::move(item));
  }
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                        std::nullopt, context_->current_tokens());
}

void AITextSession::ModelExecutionCallback(
    const PromptApiRequest& input,
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

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  if (response->has_value()) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                          response->value(), /*current_tokens=*/std::nullopt);
  }
  if (result.response->is_complete) {
    // TODO(crbug.com/351935390): instead of calculating this from the
    // AITextSession, it should be returned by the model since the token
    // should be calculated during the execution.
    PromptApiRequest request;
    request.mutable_prompt_history()->CopyFrom(input.current_prompts());
    *request.add_prompt_history() =
        MakePrompt(PromptApiRole::PROMPT_API_ROLE_ASSISTANT, response->value());
    session_->GetContextSizeInTokens(
        *context_->MaybeFormatRequest(request),
        base::BindOnce(&AITextSession::AddPromptHistoryAndSendCompletion,
                       weak_ptr_factory_.GetWeakPtr(), request, responder));
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
    session_->AddContext(*context_->MakeRequest());
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  PromptApiRequest request;
  *request.add_current_prompts() =
      MakePrompt(PromptApiRole::PROMPT_API_ROLE_USER, input);
  session_->ExecuteModel(
      *context_->MaybeFormatRequest(request),
      base::BindRepeating(&AITextSession::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), request,
                          responder_id));
}

void AITextSession::Fork(
    mojo::PendingReceiver<blink::mojom::AITextSession> session,
    ForkCallback callback) {
  if (!browser_context_) {
    // The `browser_context_` is already destroyed before the renderer owner
    // is gone.
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
