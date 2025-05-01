// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_language_model.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

using optimization_guide::MultimodalMessage;
using optimization_guide::MultimodalMessageReadView;
using optimization_guide::proto::PromptApiMetadata;
using optimization_guide::proto::PromptApiPrompt;
using optimization_guide::proto::PromptApiRequest;
using optimization_guide::proto::PromptApiRole;

PromptApiRole ConvertRole(blink::mojom::AILanguageModelPromptRole role) {
  switch (role) {
    case blink::mojom::AILanguageModelPromptRole::kSystem:
      return PromptApiRole::PROMPT_API_ROLE_SYSTEM;
    case blink::mojom::AILanguageModelPromptRole::kUser:
      return PromptApiRole::PROMPT_API_ROLE_USER;
    case blink::mojom::AILanguageModelPromptRole::kAssistant:
      return PromptApiRole::PROMPT_API_ROLE_ASSISTANT;
  }
}

blink::mojom::AILanguageModelPromptPtr MakeTextPrompt(
    blink::mojom::AILanguageModelPromptRole role,
    const std::string& text) {
  return blink::mojom::AILanguageModelPrompt::New(
      role, blink::mojom::AILanguageModelPromptContent::NewText(text));
}

// Construct an empty multimodal PromptApiRequest message.
MultimodalMessage EmptyMessage() {
  return MultimodalMessage((PromptApiRequest()));
}

void AddPromptToField(
    const blink::mojom::AILanguageModelPrompt& prompt,
    optimization_guide::RepeatedMultimodalMessageEditView view,
    const on_device_model::Capabilities& capabilities) {
  PromptApiPrompt prompt_proto;
  prompt_proto.set_role(ConvertRole(prompt.role));
  auto prompt_view = view.Add(prompt_proto);
  if (prompt.content->is_text()) {
    prompt_view.Set(PromptApiPrompt::kTextFieldNumber,
                    prompt.content->get_text());
  } else if (prompt.content->is_bitmap()) {
    if (!capabilities.Has(on_device_model::CapabilityFlags::kImageInput)) {
      mojo::ReportBadMessage("Image input is not supported.");
      return;
    }
    prompt_view.Set(PromptApiPrompt::kMediaFieldNumber,
                    prompt.content->get_bitmap());
  } else if (prompt.content->is_audio()) {
    if (!capabilities.Has(on_device_model::CapabilityFlags::kAudioInput)) {
      mojo::ReportBadMessage("Audio input is not supported.");
      return;
    }
    // TODO: Export services/on_device_model/ml/chrome_ml_types_traits.cc.
    const on_device_model::mojom::AudioDataPtr& audio_data =
        prompt.content->get_audio();
    ml::AudioBuffer audio_buffer;
    audio_buffer.sample_rate_hz = audio_data->sample_rate;
    audio_buffer.num_channels = audio_data->channel_count;
    audio_buffer.num_frames = audio_data->frame_count;
    audio_buffer.data = audio_data->data;
    prompt_view.Set(PromptApiPrompt::kMediaFieldNumber,
                    std::move(audio_buffer));
  } else {
    NOTREACHED();
  }
}

// Fill the 'view'ed Repeated<PromptApiPrompt> field with the prompts of 'item'.
void AddPrompts(optimization_guide::RepeatedMultimodalMessageEditView view,
                const AILanguageModel::Context::ContextItem& item,
                const on_device_model::Capabilities& capabilities) {
  for (const auto& prompt : item.prompts) {
    AddPromptToField(*prompt, view, capabilities);
  }
}

// Construct an multimodal PromptApiRequest with initial prompts from 'item'.
MultimodalMessage MakeInitialPrompt(
    const AILanguageModel::Context::ContextItem& item,
    const on_device_model::Capabilities& capabilities) {
  MultimodalMessage request = EmptyMessage();
  AddPrompts(request.edit().MutableRepeatedField(
                 PromptApiRequest::kInitialPromptsFieldNumber),
             item, capabilities);
  return request;
}

// Add the prompts from 'item' to the current_prompts field of 'request'.
void AddCurrentRequest(MultimodalMessage& request,
                       const AILanguageModel::Context::ContextItem& item,
                       const on_device_model::Capabilities& capabilities) {
  AddPrompts(request.edit().MutableRepeatedField(
                 PromptApiRequest::kCurrentPromptsFieldNumber),
             item, capabilities);
}

}  // namespace

AILanguageModel::Context::ContextItem::ContextItem() = default;
AILanguageModel::Context::ContextItem::ContextItem(const ContextItem& other) {
  tokens = other.tokens;
  for (const auto& prompt : other.prompts) {
    prompts.emplace_back(prompt.Clone());
  }
}
AILanguageModel::Context::ContextItem::ContextItem(ContextItem&&) = default;
AILanguageModel::Context::ContextItem::~ContextItem() = default;

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

AILanguageModel::Context::Context(uint32_t max_tokens,
                                  ContextItem initial_prompts)
    : max_tokens_(max_tokens), initial_prompts_(std::move(initial_prompts)) {
  CHECK_GE(max_tokens_, initial_prompts_.tokens)
      << "the caller shouldn't create an AILanguageModel with the initial "
         "prompts containing more tokens than the limit.";
  current_tokens_ += initial_prompts.tokens;
}

AILanguageModel::Context::Context(const Context& context) = default;

AILanguageModel::Context::~Context() = default;

AILanguageModel::Context::SpaceReservationResult
AILanguageModel::Context::ReserveSpace(uint32_t num_tokens) {
  // If there is no enough space to hold the `initial_prompts_` as well as the
  // newly requested `num_tokens`,  return `kInsufficientSpace`.
  if (num_tokens + initial_prompts_.tokens > max_tokens_) {
    return AILanguageModel::Context::SpaceReservationResult::kInsufficientSpace;
  }

  if (current_tokens_ + num_tokens <= max_tokens_) {
    return AILanguageModel::Context::SpaceReservationResult::kSufficientSpace;
  }

  CHECK(!context_items_.empty());
  do {
    current_tokens_ -= context_items_.begin()->tokens;
    context_items_.pop_front();
  } while (current_tokens_ + num_tokens > max_tokens_);

  return AILanguageModel::Context::SpaceReservationResult::kSpaceMadeAvailable;
}

AILanguageModel::Context::SpaceReservationResult
AILanguageModel::Context::AddContextItem(ContextItem context_item) {
  auto result = ReserveSpace(context_item.tokens);
  if (result != SpaceReservationResult::kInsufficientSpace) {
    context_items_.emplace_back(context_item);
    current_tokens_ += context_item.tokens;
  }

  return result;
}

MultimodalMessage AILanguageModel::Context::MakeRequest(
    const on_device_model::Capabilities& capabilities) {
  MultimodalMessage request = MakeInitialPrompt(initial_prompts_, capabilities);
  auto history_field = request.edit().MutableRepeatedField(
      PromptApiRequest::kPromptHistoryFieldNumber);
  for (auto& context_item : context_items_) {
    AddPrompts(history_field, context_item, capabilities);
  }
  return request;
}

bool AILanguageModel::Context::HasContextItem() {
  return current_tokens_;
}

AILanguageModel::AILanguageModel(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session,
    base::WeakPtr<content::BrowserContext> browser_context,
    mojo::PendingRemote<blink::mojom::AILanguageModel> pending_remote,
    AIContextBoundObjectSet& context_bound_object_set,
    AIManager& ai_manager,
    const std::optional<const Context>& context)
    : AIContextBoundObject(context_bound_object_set),
      session_(std::move(session)),
      browser_context_(browser_context),
      context_bound_object_set_(context_bound_object_set),
      ai_manager_(ai_manager),
      pending_remote_(std::move(pending_remote)),
      receiver_(this, pending_remote_.InitWithNewPipeAndPassReceiver()) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));

  if (context.has_value()) {
    // If the context is provided, it will be used in this session.
    context_ = std::make_unique<Context>(context.value());
    return;
  }

  // If the context is not provided, initialize a new context
  // with the default configuration.
  context_ = std::make_unique<Context>(
      session_->GetTokenLimits().max_context_tokens, Context::ContextItem());
}

AILanguageModel::~AILanguageModel() = default;

// static
PromptApiMetadata AILanguageModel::ParseMetadata(
    const optimization_guide::proto::Any& any) {
  PromptApiMetadata metadata;
  if (any.type_url() ==
      base::StrCat({"type.googleapis.com/", metadata.GetTypeName()})) {
    metadata.ParseFromString(any.value());
  }
  return metadata;
}

void AILanguageModel::SetInitialPrompts(
    const std::optional<std::string> system_prompt,
    std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
    CreateLanguageModelCallback callback) {
  Context::ContextItem item;
  if (system_prompt) {
    item.prompts.emplace_back(MakeTextPrompt(
        blink::mojom::AILanguageModelPromptRole::kSystem, *system_prompt));
  }
  for (auto& prompt : initial_prompts) {
    item.prompts.emplace_back(std::move(prompt));
  }
  MultimodalMessage request =
      MakeInitialPrompt(item, session_->GetCapabilities());
  session_->GetContextSizeInTokens(
      request.read(),
      base::BindOnce(&AILanguageModel::InitializeContextWithInitialPrompts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(item),
                     std::move(callback)));
}

void AILanguageModel::InitializeContextWithInitialPrompts(
    Context::ContextItem initial_prompts,
    CreateLanguageModelCallback callback,
    std::optional<uint32_t> result) {
  if (!result.has_value()) {
    std::move(callback).Run(
        base::unexpected(blink::mojom::AIManagerCreateClientError::
                             kUnableToCalculateTokenSize),
        /*info=*/nullptr);
    return;
  }

  uint32_t size = result.value();
  uint32_t max_token = context_->max_tokens();
  if (size > max_token) {
    // The session cannot be created if the system prompt contains more tokens
    // than the limit.
    std::move(callback).Run(
        base::unexpected(
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge),
        /*info=*/nullptr);
    return;
  }

  initial_prompts.tokens = size;
  context_ = std::make_unique<Context>(max_token, std::move(initial_prompts));

  // Begin processing the initial prompts immediately.
  session_->SetInput(context_->MakeRequest(session_->GetCapabilities()), {});

  std::move(callback).Run(TakePendingRemote(), GetLanguageModelInstanceInfo());
}

void AILanguageModel::ModelExecutionCallback(
    const Context::ContextItem& item,
    mojo::RemoteSetElementId responder_id,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    // It might be possible for the responder mojo connection to be closed
    // before this callback is invoked, in this case, we can't do anything.
    return;
  }

  if (!result.response.has_value()) {
    responder->OnError(
        AIUtils::ConvertModelExecutionError(result.response.error().error()));
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  if (response->has_value()) {
    std::string chunk = response->value();
    current_response_ += chunk;
    responder->OnStreaming(chunk);
  }

  if (result.response->is_complete) {
    uint32_t token_count = result.response->input_token_count +
                           result.response->output_token_count;
    // If the on device model service fails to calculate the size, it will be 0.
    // TODO(crbug.com/351935691): make sure the error is explicitly returned
    // and handled accordingly.
    if (token_count) {
      Context::ContextItem copy = item;
      copy.tokens = token_count;
      copy.prompts.emplace_back(
          MakeTextPrompt(blink::mojom::AILanguageModelPromptRole::kAssistant,
                         current_response_));
      if (context_->AddContextItem(std::move(copy)) ==
          Context::SpaceReservationResult::kSpaceMadeAvailable) {
        responder->OnQuotaOverflow();
      }
    }
    responder->OnCompletion(blink::mojom::ModelExecutionContextInfo::New(
        context_->current_tokens()));
  }
}

void AILanguageModel::PromptGetInputSizeCompletion(
    mojo::RemoteSetElementId responder_id,
    Context::ContextItem current_item,
    on_device_model::mojom::ResponseConstraintPtr constraint,
    std::optional<uint32_t> result) {
  if (!session_) {
    // If the session is destroyed before this callback is invoked, we should
    // not do anything further.
    return;
  }

  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    // It might be possible for the responder mojo connection to be closed
    // before this callback is invoked, in this case, we can't do anything.
    return;
  }

  if (!result.has_value()) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);
    return;
  }

  uint32_t number_of_tokens = result.value();
  auto space_reserved = context_->ReserveSpace(number_of_tokens);
  if (space_reserved == Context::SpaceReservationResult::kInsufficientSpace) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
    return;
  }

  if (space_reserved == Context::SpaceReservationResult::kSpaceMadeAvailable) {
    responder->OnQuotaOverflow();
  }
  current_item.tokens = number_of_tokens;

  const on_device_model::Capabilities& capabilities =
      session_->GetCapabilities();
  MultimodalMessage request = context_->MakeRequest(capabilities);
  AddCurrentRequest(request, current_item, capabilities);
  session_->SetInput(std::move(request), {});
  session_->ExecuteModelWithResponseConstraint(
      PromptApiRequest(), std::move(constraint),
      base::BindRepeating(&AILanguageModel::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(current_item), responder_id));
}

void AILanguageModel::Prompt(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    on_device_model::mojom::ResponseConstraintPtr constraint,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (!session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  // Clear the response from the previous execution.
  current_response_ = "";
  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));

  Context::ContextItem item;
  item.prompts = std::move(prompts);

  MultimodalMessage request = EmptyMessage();
  AddCurrentRequest(request, item, session_->GetCapabilities());
  session_->GetExecutionInputSizeInTokens(
      request.read(),
      base::BindOnce(&AILanguageModel::PromptGetInputSizeCompletion,
                     weak_ptr_factory_.GetWeakPtr(), responder_id,
                     std::move(item), std::move(constraint)));
}

void AILanguageModel::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));
  if (!browser_context_) {
    // The `browser_context_` is already destroyed before the renderer owner
    // is gone.
    client_remote->OnError(
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  const optimization_guide::SamplingParams sampling_param =
      session_->GetSamplingParams();

  ai_manager_->CreateLanguageModelForCloning(
      base::PassKey<AILanguageModel>(),
      blink::mojom::AILanguageModelSamplingParams::New(
          sampling_param.top_k, sampling_param.temperature),
      session_->GetCapabilities(), context_bound_object_set_.get(), *context_,
      std::move(client_remote));
}

void AILanguageModel::Destroy() {
  session_.reset();
  for (auto& responder : responder_set_) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
  }
  responder_set_.Clear();
}

blink::mojom::AILanguageModelInstanceInfoPtr
AILanguageModel::GetLanguageModelInstanceInfo() {
  const optimization_guide::SamplingParams session_sampling_params =
      session_->GetSamplingParams();
  base::flat_set<blink::mojom::AILanguageModelPromptType> input_types = {
      blink::mojom::AILanguageModelPromptType::kText  // Text is always
                                                      // supported.
  };
  for (const auto capability : session_->GetCapabilities()) {
    switch (capability) {
      case on_device_model::CapabilityFlags::kImageInput:
        input_types.insert(blink::mojom::AILanguageModelPromptType::kImage);
        break;
      case on_device_model::CapabilityFlags::kAudioInput:
        input_types.insert(blink::mojom::AILanguageModelPromptType::kAudio);
        break;
    }
  }

  return blink::mojom::AILanguageModelInstanceInfo::New(
      context_->max_tokens(), context_->current_tokens(),
      blink::mojom::AILanguageModelSamplingParams::New(
          session_sampling_params.top_k, session_sampling_params.temperature),
      std::move(input_types).extract());
}

void AILanguageModel::MeasureInputUsage(
    std::vector<blink::mojom::AILanguageModelPromptPtr> input,
    mojo::PendingRemote<blink::mojom::AILanguageModelMeasureInputUsageClient>
        client) {
  Context::ContextItem item;
  item.prompts = std::move(input);

  MultimodalMessage request = EmptyMessage();
  AddCurrentRequest(request, item, session_->GetCapabilities());

  session_->GetExecutionInputSizeInTokens(
      request.read(),
      base::BindOnce(
          [](mojo::Remote<blink::mojom::AILanguageModelMeasureInputUsageClient>
                 client_remote,
             std::optional<uint32_t> result) {
            // TODO(crbug.com/351935691): Explicitly return an error. Consider
            // introducing a callback instead of remote client, as it's done
            // for Writing Assistance APIs.
            client_remote->OnResult(result.value_or(0));
          },
          mojo::Remote<blink::mojom::AILanguageModelMeasureInputUsageClient>(
              std::move(client))));
}

void AILanguageModel::SetPriority(on_device_model::mojom::Priority priority) {
  if (session_) {
    session_->SetPriority(priority);
  }
}

mojo::PendingRemote<blink::mojom::AILanguageModel>
AILanguageModel::TakePendingRemote() {
  return std::move(pending_remote_);
}
