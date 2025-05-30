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
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

using ::optimization_guide::proto::PromptApiMetadata;

ml::Token ConvertToToken(blink::mojom::AILanguageModelPromptRole role) {
  switch (role) {
    case blink::mojom::AILanguageModelPromptRole::kSystem:
      return ml::Token::kSystem;
    case blink::mojom::AILanguageModelPromptRole::kUser:
      return ml::Token::kUser;
    case blink::mojom::AILanguageModelPromptRole::kAssistant:
      return ml::Token::kModel;
  }
}

on_device_model::mojom::InputPtr ConvertToInput(
    const std::vector<blink::mojom::AILanguageModelPromptPtr>& prompts,
    const on_device_model::Capabilities& capabilities) {
  auto input = on_device_model::mojom::Input::New();
  for (const auto& prompt : prompts) {
    input->pieces.push_back(ConvertToToken(prompt->role));
    for (const auto& content : prompt->content) {
      switch (content->which()) {
        case blink::mojom::AILanguageModelPromptContent::Tag::kText:
          input->pieces.push_back(content->get_text());
          break;
        case blink::mojom::AILanguageModelPromptContent::Tag::kBitmap:
          if (!capabilities.Has(
                  on_device_model::CapabilityFlags::kImageInput)) {
            return nullptr;
          }
          input->pieces.push_back(content->get_bitmap());
          break;
        case blink::mojom::AILanguageModelPromptContent::Tag::kAudio:
          if (!capabilities.Has(
                  on_device_model::CapabilityFlags::kAudioInput)) {
            return nullptr;
          }
          // TODO: Export services/on_device_model/ml/chrome_ml_types_traits.cc.
          const on_device_model::mojom::AudioDataPtr& audio_data =
              content->get_audio();
          ml::AudioBuffer audio_buffer;
          audio_buffer.sample_rate_hz = audio_data->sample_rate;
          audio_buffer.num_channels = audio_data->channel_count;
          audio_buffer.num_frames = audio_data->frame_count;
          audio_buffer.data = audio_data->data;
          input->pieces.push_back(std::move(audio_buffer));
          break;
      }
    }
    input->pieces.push_back(ml::Token::kEnd);
  }
  return input;
}

on_device_model::mojom::InputPtr ConvertToInputForExecute(
    const std::vector<blink::mojom::AILanguageModelPromptPtr>& prompts,
    const on_device_model::Capabilities& capabilities) {
  auto input = ConvertToInput(prompts, capabilities);
  if (!input) {
    return nullptr;
  }
  input->pieces.push_back(ml::Token::kModel);
  return input;
}

on_device_model::mojom::AppendOptionsPtr MakeAppendOptions(
    on_device_model::mojom::InputPtr input) {
  auto append_options = on_device_model::mojom::AppendOptions::New();
  append_options->input = std::move(input);
  return append_options;
}

optimization_guide::MultimodalMessage CreateStringMessage(
    const on_device_model::mojom::Input& input) {
  optimization_guide::proto::StringValue value;
  value.set_value(optimization_guide::OnDeviceInputToString(input));
  return optimization_guide::MultimodalMessage(value);
}

}  // namespace

// Contains state for a currently active prompt call. Makes sure everything is
// properly cancelled if needed.
class AILanguageModel::PromptState
    : public on_device_model::mojom::StreamingResponder,
      public on_device_model::mojom::ContextClient {
 public:
  enum class Mode {
    // Only input will be added, no output will be generated. The completion
    // callback will be called when ContextClient has signaled completion.
    kAppendOnly,
    // Input will be appended and then output will be generated. The completion
    // callback will be called when StreamingResponder has signaled completion
    // and the output has been checked for safety.
    kAppendAndGenerate,
  };
  PromptState(
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder> responder,
      on_device_model::mojom::InputPtr input,
      on_device_model::mojom::ResponseConstraintPtr constraint,
      optimization_guide::SafetyChecker& safety_checker,
      base::WeakPtr<OptimizationGuideLogger> logger,
      Mode mode)
      : responder_(std::move(responder)),
        input_(std::move(input)),
        constraint_(std::move(constraint)),
        safety_checker_(safety_checker),
        logger_(std::move(logger)),
        mode_(mode) {
    responder_.set_disconnect_handler(
        base::BindOnce(&PromptState::OnDisconnect, base::Unretained(this)));
  }

  ~PromptState() override {
    OnError(blink::mojom::ModelStreamingResponseStatus::kErrorCancelled);
  }

  // Appends input and generates a response on `session`. `callback` will be
  // called on completion or error, with the full response and number of
  // input+output tokens. `callback` may delete this object.
  void AppendAndGenerate(
      mojo::PendingRemote<on_device_model::mojom::Session> session,
      base::OnceClosure callback) {
    start_ = base::TimeTicks::Now();
    callback_ = std::move(callback);
    safety_checker_->RunRequestChecks(
        CreateStringMessage(*input_),
        base::BindOnce(&PromptState::RequestSafetyChecksComplete,
                       weak_factory_.GetWeakPtr(), std::move(session)));
  }

  void OnError(blink::mojom::ModelStreamingResponseStatus error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
    if (responder_) {
      AIUtils::SendStreamingStatus(responder_, error,
                                   std::move(quota_error_info));
    }
    session_.reset();
    responder_.reset();
    context_receiver_.reset();
    response_receiver_.reset();
    if (callback_) {
      std::move(callback_).Run();
      // `this` may be deleted.
    }
  }

  void OnQuotaOverflow() {
    if (responder_) {
      responder_->OnQuotaOverflow();
    }
  }

  void SetPriority(on_device_model::mojom::Priority priority) {
    if (session_) {
      session_->SetPriority(priority);
    }
  }

  bool IsValid() const { return !!responder_; }

  mojo::Remote<on_device_model::mojom::Session> TakeSession() {
    return std::move(session_);
  }

  mojo::Remote<blink::mojom::ModelStreamingResponder> TakeResponder() {
    return std::move(responder_);
  }

  on_device_model::mojom::InputPtr TakeInput() { return std::move(input_); }
  const std::string& response() const { return full_response_; }
  // The total token count for this request including input and output tokens.
  uint32_t token_count() const { return token_count_; }
  Mode mode() const { return mode_; }

 private:
  void OnDisconnect() {
    OnError(blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);
  }

  // on_device_model::mojom::ContextClient:
  void OnComplete(uint32_t tokens_processed) override {
    base::UmaHistogramCounts10000("AI.Session.LanguageModel.ContextTokens",
                                  tokens_processed);
    base::UmaHistogramMediumTimes("AI.Session.LanguageModel.ContextTime",
                                  base::TimeTicks::Now() - start_);
    if (logger_ && logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
          logger_.get())
          << "Executing model with input context of "
          << base::NumberToString(tokens_processed) << " tokens:\n"
          << optimization_guide::OnDeviceInputToString(*input_);
    }
    generate_start_ = base::TimeTicks::Now();
    context_receiver_.reset();
    token_count_ = tokens_processed;
    if (mode_ == Mode::kAppendOnly) {
      std::move(callback_).Run();
      // `this` may be deleted.
    }
  }

  // on_device_model::mojom::StreamingResponder:
  void OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) override {
    if (full_response_.empty()) {
      base::UmaHistogramMediumTimes(
          "AI.Session.LanguageModel.FirstResponseTime",
          base::TimeTicks::Now() - start_);
    }
    output_tokens_++;
    full_response_ += chunk->text;

    unchecked_output_tokens_++;
    unchecked_response_ += chunk->text;

    if (!safety_checker_->safety_cfg().CanCheckPartialOutput(
            output_tokens_, unchecked_output_tokens_)) {
      return;
    }
    safety_checker_->RunRawOutputCheck(
        full_response_, optimization_guide::ResponseCompleteness::kPartial,
        base::BindOnce(&PromptState::OnPartialResponseCheckComplete,
                       weak_factory_.GetWeakPtr(),
                       std::move(unchecked_response_)));
    unchecked_output_tokens_ = 0;
    unchecked_response_ = "";
  }

  void OnComplete(on_device_model::mojom::ResponseSummaryPtr summary) override {
    // The `OnComplete()` method on `responder_` will be called in
    // `AILanguageModel::OnPromptOutputComplete()` after adding the response to
    // the session and handling overflow.
    response_receiver_.reset();
    safety_checker_->RunRawOutputCheck(
        full_response_, optimization_guide::ResponseCompleteness::kComplete,
        base::BindOnce(&PromptState::OnFullResponseCheckComplete,
                       weak_factory_.GetWeakPtr(), std::move(summary)));
  }

  void RequestSafetyChecksComplete(
      mojo::PendingRemote<on_device_model::mojom::Session> session,
      optimization_guide::SafetyChecker::Result safety_result) {
    if (HandleSafetyError(std::move(safety_result))) {
      return;
    }
    session_.Bind(std::move(session));
    session_.set_disconnect_handler(
        base::BindOnce(&PromptState::OnDisconnect, base::Unretained(this)));

    session_->Append(MakeAppendOptions(input_.Clone()),
                     context_receiver_.BindNewPipeAndPassRemote());
    context_receiver_.set_disconnect_handler(
        base::BindOnce(&PromptState::OnDisconnect, base::Unretained(this)));

    if (mode_ == Mode::kAppendAndGenerate) {
      auto generate_options = on_device_model::mojom::GenerateOptions::New();
      generate_options->constraint = std::move(constraint_);
      session_->Generate(std::move(generate_options),
                         response_receiver_.BindNewPipeAndPassRemote());
      response_receiver_.set_disconnect_handler(
          base::BindOnce(&PromptState::OnDisconnect, base::Unretained(this)));
    }
  }

  void OnPartialResponseCheckComplete(
      const std::string& response,
      optimization_guide::SafetyChecker::Result safety_result) {
    if (HandleSafetyError(std::move(safety_result))) {
      return;
    }
    responder_->OnStreaming(response);
  }

  void OnFullResponseCheckComplete(
      on_device_model::mojom::ResponseSummaryPtr summary,
      optimization_guide::SafetyChecker::Result safety_result) {
    if (HandleSafetyError(std::move(safety_result))) {
      return;
    }
    token_count_ += summary->output_token_count;
    base::UmaHistogramMediumTimes(
        "AI.Session.LanguageModel.ResponseCompleteTime",
        base::TimeTicks::Now() - generate_start_);
    base::UmaHistogramCounts10000("AI.Session.LanguageModel.ResponseTokens",
                                  summary->output_token_count);

    if (logger_ && logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
          logger_.get())
          << "Model generates raw response with PromptApi:\n"
          << full_response_;
    }
    std::move(callback_).Run();
    // `this` may be deleted.
  }

  // Returns true if there was a safety error and the response was stopped.
  bool HandleSafetyError(
      optimization_guide::SafetyChecker::Result safety_result) {
    if (safety_result.failed_to_run) {
      OnError(blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);
      return true;
    }
    if (safety_result.is_unsafe) {
      OnError(blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
      return true;
    }
    if (safety_result.is_unsupported_language) {
      OnError(blink::mojom::ModelStreamingResponseStatus::
                  kErrorUnsupportedLanguage);
      return true;
    }
    return false;
  }

  mojo::Remote<blink::mojom::ModelStreamingResponder> responder_;

  mojo::Remote<on_device_model::mojom::Session> session_;
  mojo::Receiver<on_device_model::mojom::ContextClient> context_receiver_{this};
  mojo::Receiver<on_device_model::mojom::StreamingResponder> response_receiver_{
      this};
  on_device_model::mojom::InputPtr input_;
  on_device_model::mojom::ResponseConstraintPtr constraint_;

  // Called when the full operation has completed or an error has occurred.
  base::OnceClosure callback_;
  base::raw_ref<optimization_guide::SafetyChecker> safety_checker_;

  // Total number of tokens in input and output.
  uint32_t token_count_ = 0;
  // The full response so far.
  std::string full_response_;
  // Number of tokens in the response.
  uint32_t output_tokens_ = 0;
  // The response since safety check was last run.
  std::string unchecked_response_;
  // Number of tokens since safety check was last run.
  uint32_t unchecked_output_tokens_ = 0;

  base::WeakPtr<OptimizationGuideLogger> logger_;
  Mode mode_;

  base::TimeTicks start_;
  base::TimeTicks generate_start_;

  base::WeakPtrFactory<PromptState> weak_factory_{this};
};

AILanguageModel::Context::ContextItem::ContextItem() = default;
AILanguageModel::Context::ContextItem::ContextItem(const ContextItem& other) {
  tokens = other.tokens;
  input = other.input.Clone();
}
AILanguageModel::Context::ContextItem::ContextItem(ContextItem&&) = default;
AILanguageModel::Context::ContextItem::~ContextItem() = default;

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

AILanguageModel::Context::Context(uint32_t max_tokens)
    : max_tokens_(max_tokens) {}

AILanguageModel::Context::Context(const Context& context) = default;

AILanguageModel::Context::~Context() = default;

AILanguageModel::Context::SpaceReservationResult
AILanguageModel::Context::ReserveSpace(uint32_t num_tokens) {
  // If there is not enough space to hold the newly requested `num_tokens`,
  // return `kInsufficientSpace`.
  if (num_tokens > max_tokens_) {
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

on_device_model::mojom::InputPtr
AILanguageModel::Context::GetNonInitialPrompts() {
  auto input = on_device_model::mojom::Input::New();
  for (const auto& item : context_items_) {
    input->pieces.insert(input->pieces.end(), item.input->pieces.begin(),
                         item.input->pieces.end());
  }
  return input;
}

AILanguageModel::AILanguageModel(
    AIContextBoundObjectSet& context_bound_object_set,
    on_device_model::mojom::SessionParamsPtr session_params,
    base::WeakPtr<optimization_guide::ModelClient> model_client,
    mojo::PendingRemote<on_device_model::mojom::Session> session,
    base::WeakPtr<OptimizationGuideLogger> logger)
    : AIContextBoundObject(context_bound_object_set),
      initial_session_(std::move(session)),
      session_params_(std::move(session_params)),
      context_bound_object_set_(context_bound_object_set),
      model_client_(std::move(model_client)),
      logger_(std::move(logger)) {
  context_ = std::make_unique<Context>(
      model_client_->feature_adapter().GetTokenLimits().max_context_tokens);
  // TODO(crbug.com/415808003): Should we handle crashes?
  initial_session_.reset_on_disconnect();

  safety_checker_ = std::make_unique<optimization_guide::SafetyChecker>(
      weak_ptr_factory_.GetWeakPtr(),
      optimization_guide::SafetyConfig(model_client_->safety_config()));

  if (logger_ && logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        logger_.get())
        << "Starting on-device session for PromptApi";
  }
}

AILanguageModel::~AILanguageModel() {
  // If the initial session has been reset, the session crashed.
  base::UmaHistogramBoolean("AI.Session.LanguageModel.Crashed",
                            !initial_session_);
}

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

void AILanguageModel::Initialize(
    std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        create_client) {
  if (initial_prompts.empty()) {
    InitializeGetInputSizeComplete(nullptr, std::move(create_client), 0);
  } else {
    auto input = ConvertToInput(initial_prompts, session_params_->capabilities);
    if (!input) {
      mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
          client_remote(std::move(create_client));
      AIUtils::SendClientRemoteError(
          client_remote,
          blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
      return;
    }
    // This does not need to be queued because the AILanguageModel receiver has
    // not been bound yet, so mojo calls cannot be received.
    // TODO(crbug.com/415808003): May be able to avoid GetSizeInTokens() and
    // directly use the token result from ContextClient if the backend can
    // gracefully handle sending >max_tokens and giving an error.
    auto cloned_input = input.Clone();
    GetSizeInTokens(
        std::move(cloned_input),
        base::BindOnce(&AILanguageModel::InitializeGetInputSizeComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(input),
                       std::move(create_client)));
  }
}

void AILanguageModel::Prompt(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    on_device_model::mojom::ResponseConstraintPtr constraint,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  AddToQueue(base::BindOnce(
      &AILanguageModel::PromptInternal, weak_ptr_factory_.GetWeakPtr(),
      std::move(prompts), std::move(constraint), std::move(pending_responder)));
}

void AILanguageModel::Append(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  AddToQueue(base::BindOnce(&AILanguageModel::AppendInternal,
                            weak_ptr_factory_.GetWeakPtr(), std::move(prompts),
                            std::move(pending_responder)));
}

void AILanguageModel::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client) {
  AddToQueue(base::BindOnce(&AILanguageModel::ForkInternal,
                            weak_ptr_factory_.GetWeakPtr(), std::move(client)));
}

void AILanguageModel::Destroy() {
  RemoveFromSet();
}

void AILanguageModel::MeasureInputUsage(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    MeasureInputUsageCallback callback) {
  auto input = ConvertToInputForExecute(std::move(prompts),
                                        session_params_->capabilities);
  if (!input) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  GetSizeInTokens(std::move(input), std::move(callback));
}

void AILanguageModel::SetPriority(on_device_model::mojom::Priority priority) {
  if (initial_session_) {
    initial_session_->SetPriority(priority);
  }
  if (current_session_) {
    current_session_->SetPriority(priority);
  }
  if (prompt_state_) {
    prompt_state_->SetPriority(priority);
  }
}

void AILanguageModel::StartSession(
    mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> session) {
  if (model_client_) {
    model_client_->StartSession(std::move(session));
  }
}

blink::mojom::AILanguageModelInstanceInfoPtr
AILanguageModel::GetLanguageModelInstanceInfo() {
  base::flat_set<blink::mojom::AILanguageModelPromptType> input_types = {
      blink::mojom::AILanguageModelPromptType::kText  // Text always supported.
  };
  for (const auto capability : session_params_->capabilities) {
    switch (capability) {
      case on_device_model::CapabilityFlags::kImageInput:
        input_types.insert(blink::mojom::AILanguageModelPromptType::kImage);
        break;
      case on_device_model::CapabilityFlags::kAudioInput:
        input_types.insert(blink::mojom::AILanguageModelPromptType::kAudio);
        break;
    }
  }

  uint32_t max_tokens = 0;
  if (model_client_) {
    max_tokens =
        model_client_->feature_adapter().GetTokenLimits().max_context_tokens;
  }
  return blink::mojom::AILanguageModelInstanceInfo::New(
      max_tokens, max_tokens - context_->max_tokens(),
      blink::mojom::AILanguageModelSamplingParams::New(
          session_params_->top_k, session_params_->temperature),
      std::move(input_types).extract());
}

mojo::PendingRemote<blink::mojom::AILanguageModel>
AILanguageModel::BindRemote() {
  auto remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
  return remote;
}

void AILanguageModel::InitializeGetInputSizeComplete(
    on_device_model::mojom::InputPtr input,
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        create_client,
    std::optional<uint32_t> token_count) {
  if (!initial_session_ || !token_count) {
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(create_client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  uint32_t max_tokens = context_->max_tokens();
  if (*token_count > max_tokens) {
    auto quota = context_->max_tokens() - context_->current_tokens();
    mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient>
        client_remote(std::move(create_client));
    AIUtils::SendClientRemoteError(
        client_remote,
        blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(token_count.value(), quota));
    return;
  }

  // `context_` will track how many tokens are remaining after the initial
  // prompts. The initial prompts cannot be evicted.
  context_ = std::make_unique<Context>(max_tokens - *token_count);

  if (input) {
    if (logger_ && logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
          logger_.get())
          << "Adding initial context to the model of "
          << base::NumberToString(*token_count) << " tokens:\n"
          << optimization_guide::OnDeviceInputToString(*input);
    }
    auto safety_input = CreateStringMessage(*input);
    safety_checker_->RunRequestChecks(
        safety_input,
        base::BindOnce(&AILanguageModel::InitializeSafetyChecksComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(input),
                       std::move(create_client)));
  } else {
    InitializeSafetyChecksComplete(nullptr, std::move(create_client),
                                   optimization_guide::SafetyChecker::Result());
  }
}

void AILanguageModel::InitializeSafetyChecksComplete(
    on_device_model::mojom::InputPtr input,
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        create_client,
    optimization_guide::SafetyChecker::Result safety_result) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client(
      std::move(create_client));
  // TODO(crbug.com/415808003): Add more fine grained errors on safety check
  // failure.
  if (safety_result.failed_to_run || safety_result.is_unsafe ||
      safety_result.is_unsupported_language) {
    AIUtils::SendClientRemoteError(
        client,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }
  if (input) {
    // No ContextClient is passed here since this operation should never be
    // cancelled unless the session is destroyed.
    initial_session_->Append(MakeAppendOptions(std::move(input)), {});
  }
  initial_session_->Clone(current_session_.BindNewPipeAndPassReceiver());

  client->OnResult(BindRemote(), GetLanguageModelInstanceInfo());
}

void AILanguageModel::ForkInternal(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client,
    base::OnceClosure on_complete) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> remote(
      std::move(client));
  if (!initial_session_ || !model_client_) {
    AIUtils::SendClientRemoteError(
        remote,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    return;
  }

  mojo::PendingRemote<on_device_model::mojom::Session> session;
  initial_session_->Clone(session.InitWithNewPipeAndPassReceiver());
  auto clone = std::make_unique<AILanguageModel>(
      *context_bound_object_set_, session_params_.Clone(), model_client_,
      std::move(session), logger_);
  clone->context_ = std::make_unique<Context>(*context_);
  current_session_->Clone(clone->current_session_.BindNewPipeAndPassReceiver());

  remote->OnResult(clone->BindRemote(), clone->GetLanguageModelInstanceInfo());

  context_bound_object_set_->AddContextBoundObject(std::move(clone));
}

void AILanguageModel::PromptInternal(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    on_device_model::mojom::ResponseConstraintPtr constraint,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder,
    base::OnceClosure on_complete) {
  if (!initial_session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  auto input = ConvertToInputForExecute(prompts, session_params_->capabilities);
  if (!input) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
    return;
  }
  prompt_state_ = std::make_unique<PromptState>(
      std::move(pending_responder), input.Clone(), std::move(constraint),
      *safety_checker_, logger_, PromptState::Mode::kAppendAndGenerate);
  GetSizeInTokens(
      std::move(input),
      base::BindOnce(&AILanguageModel::PromptGetInputSizeComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&AILanguageModel::OnPromptOutputComplete,
                                    weak_ptr_factory_.GetWeakPtr())
                         .Then(std::move(on_complete))));
}

void AILanguageModel::PromptGetInputSizeComplete(
    base::OnceClosure on_complete,
    std::optional<uint32_t> token_count) {
  if (!prompt_state_ || !prompt_state_->IsValid()) {
    return;
  }

  if (!token_count) {
    prompt_state_->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);
    return;
  }

  auto space_reserved = context_->ReserveSpace(*token_count);
  if (space_reserved == Context::SpaceReservationResult::kInsufficientSpace) {
    auto quota = context_->max_tokens() - context_->current_tokens();
    prompt_state_->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(token_count.value(), quota));
    return;
  }

  if (space_reserved == Context::SpaceReservationResult::kSpaceMadeAvailable) {
    HandleOverflow();
    prompt_state_->OnQuotaOverflow();
  }

  // Use a cloned version of the current session so it is easy to restore to
  // the previous state if a prompt is cancelled.
  mojo::PendingRemote<on_device_model::mojom::Session> session;
  current_session_->Clone(session.InitWithNewPipeAndPassReceiver());
  prompt_state_->AppendAndGenerate(std::move(session), std::move(on_complete));
}

void AILanguageModel::OnPromptOutputComplete() {
  if (!prompt_state_ || !prompt_state_->IsValid()) {
    return;
  }

  if (!initial_session_) {
    prompt_state_->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  // The prompt has completed successfully, replace the current session.
  current_session_ = prompt_state_->TakeSession();

  Context::ContextItem item;
  item.tokens = prompt_state_->token_count();
  item.input = prompt_state_->TakeInput();

  if (prompt_state_->mode() == PromptState::Mode::kAppendAndGenerate) {
    auto model_output = on_device_model::mojom::Input::New();
    model_output->pieces = {prompt_state_->response(), ml::Token::kEnd};
    item.input->pieces.insert(item.input->pieces.end(),
                              model_output->pieces.begin(),
                              model_output->pieces.end());
    // Add the output to the session since this is not added automatically from
    // the Generate() call. The previous token will be a kModel token from
    // ConvertToInputForExecute().
    current_session_->Append(MakeAppendOptions(std::move(model_output)), {});
    // One extra token for the end token on the model output.
    item.tokens++;
  }

  auto responder = prompt_state_->TakeResponder();
  // The context's session history may be modified when adding a new item. In
  // this case, the session history is replayed on the session and the output is
  // still sent to the responder.
  if (context_->AddContextItem(std::move(item)) ==
      Context::SpaceReservationResult::kSpaceMadeAvailable) {
    HandleOverflow();
    responder->OnQuotaOverflow();
  }
  responder->OnCompletion(
      blink::mojom::ModelExecutionContextInfo::New(context_->current_tokens()));
  if (model_client_) {
    model_client_->solution().ReportHealthyCompletion();
  }
}

void AILanguageModel::AppendInternal(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder,
    base::OnceClosure on_complete) {
  if (!initial_session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  auto input = ConvertToInput(prompts, session_params_->capabilities);
  if (!input) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    AIUtils::SendStreamingStatus(
        responder,
        blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
    return;
  }
  prompt_state_ = std::make_unique<PromptState>(
      std::move(pending_responder), input.Clone(), /*constraint=*/nullptr,
      *safety_checker_, logger_, PromptState::Mode::kAppendOnly);
  // The rest of the logic can be shared with Prompt() since PromptState() will
  // handle correctly calling this for append mode.
  GetSizeInTokens(
      std::move(input),
      base::BindOnce(&AILanguageModel::PromptGetInputSizeComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&AILanguageModel::OnPromptOutputComplete,
                                    weak_ptr_factory_.GetWeakPtr())
                         .Then(std::move(on_complete))));
}

void AILanguageModel::HandleOverflow() {
  // On overflow the prompt history has been modified. This happens if
  // Context::AddContextItem() returns kSpaceMadeAvailable. Create a clone of
  // the initial session, then replay the modified history on top of that.
  current_session_.reset();
  initial_session_->Clone(current_session_.BindNewPipeAndPassReceiver());

  auto input = context_->GetNonInitialPrompts();
  if (!input->pieces.empty()) {
    // No ContextClient is passed here since this operation should never be
    // cancelled unless the session is destroyed.
    current_session_->Append(MakeAppendOptions(std::move(input)), {});
  }
}

void AILanguageModel::GetSizeInTokens(
    on_device_model::mojom::InputPtr input,
    base::OnceCallback<void(std::optional<uint32_t>)> callback) {
  if (!initial_session_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  initial_session_->GetSizeInTokens(
      std::move(input),
      base::BindOnce(
          [](base::OnceCallback<void(std::optional<uint32_t>)> callback,
             uint32_t num_tokens) { std::move(callback).Run(num_tokens); },
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                      std::nullopt)));
}

void AILanguageModel::AddToQueue(QueueCallback task) {
  queue_.push(std::move(task));
  RunNext();
}

void AILanguageModel::TaskComplete() {
  task_running_ = false;
  RunNext();
}

void AILanguageModel::RunNext() {
  if (task_running_) {
    return;
  }
  prompt_state_ = nullptr;
  if (queue_.empty()) {
    return;
  }
  task_running_ = true;
  auto task = std::move(queue_.front());
  queue_.pop();
  // Wrap the completion callback in a default invoke to allow tasks to avoid
  // having to explicitly call in every error code path.
  std::move(task).Run(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(base::BindOnce(
          &AILanguageModel::TaskComplete, weak_ptr_factory_.GetWeakPtr())));
}
