// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_language_model_decision_model.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ai/decision_model_prompt_builder.h"
#include "chrome/browser/ai/features.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

using ::on_device_model::mojom::InputPiece;
using ::on_device_model::mojom::InputPiecePtr;

base::unexpected<blink::mojom::AIDecisionModelDecideErrorPtr> DecideError(
    blink::mojom::ModelStreamingResponseStatus status,
    blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
  return base::unexpected(blink::mojom::AIDecisionModelDecideError::New(
      status, std::move(quota_error_info)));
}

on_device_model::mojom::AppendOptionsPtr MakeAppendOptions(
    std::vector<InputPiecePtr> pieces) {
  auto options = on_device_model::mojom::AppendOptions::New();
  options->input = on_device_model::mojom::Input::New(std::move(pieces));
  options->input_source = on_device_model::mojom::InputSource::kUserInput;
  return options;
}

// Builds the `InputPiece` lists for each section of a scoring context (see the
// header comment). `InputPieces()` opens the user turn and `QuestionPieces()`
// closes it on a cloned session.
std::vector<InputPiecePtr> SystemTurn(const std::string& system_prompt) {
  std::vector<InputPiecePtr> pieces;
  pieces.push_back(InputPiece::NewToken(ml::Token::kSystem));
  pieces.push_back(InputPiece::NewText(system_prompt));
  pieces.push_back(InputPiece::NewToken(ml::Token::kEnd));
  return pieces;
}

// Opens the user turn with the input text; `QuestionPieces()` closes it.
std::vector<InputPiecePtr> InputPieces(std::string input_prompt) {
  std::vector<InputPiecePtr> pieces;
  pieces.push_back(InputPiece::NewToken(ml::Token::kUser));
  pieces.push_back(InputPiece::NewText(std::move(input_prompt)));
  return pieces;
}

// Appends the question, closes the user turn, and opens the model turn for
// option scoring.
std::vector<InputPiecePtr> QuestionPieces(const std::string& question_prompt) {
  std::vector<InputPiecePtr> pieces;
  pieces.push_back(InputPiece::NewText(question_prompt));
  pieces.push_back(InputPiece::NewToken(ml::Token::kEnd));
  pieces.push_back(InputPiece::NewToken(ml::Token::kModel));
  return pieces;
}

// Combines the system turn and a single question (without user input) to
// measure how many tokens the schema uses before reserving the rest for input.
std::vector<InputPiecePtr> PromptPieces(const std::string& system_prompt,
                                        const std::string& question_prompt) {
  std::vector<InputPiecePtr> pieces = SystemTurn(system_prompt);
  for (InputPiecePtr& piece : QuestionPieces(question_prompt)) {
    pieces.push_back(std::move(piece));
  }
  return pieces;
}

// Starts a scoring session on `model_client`'s model, or drops `session` if
// the model is unavailable.
void CreateSession(
    base::WeakPtr<optimization_guide::ModelClient> model_client,
    mojo::PendingReceiver<on_device_model::mojom::Session> session) {
  if (!model_client) {
    return;
  }
  // Scoring never samples tokens, but `CreateSession()` requires valid
  // sampling params.
  auto params = on_device_model::mojom::SessionParams::New();
  params->top_k = 1;
  params->temperature = 0.0f;
  model_client->solution().CreateSession(std::move(session), std::move(params));
}

// Resets the on-device model service's crash counter after a successful call,
// matching `AILanguageModel`.
void ReportHealthyCompletion(
    base::WeakPtr<optimization_guide::ModelClient> model_client) {
  if (model_client) {
    model_client->solution().ReportHealthyCompletion();
  }
}

// Returns the usable context window (matching `AILanguageModel`), reserving
// the output buffer so prefill padding cannot exceed the model's total token
// capacity.
uint32_t GetMaxTokens(const optimization_guide::ModelClient& model_client) {
  const optimization_guide::TokenLimits& limits = model_client.token_limits();
  const uint32_t output_buffer = static_cast<uint32_t>(std::max(
      0, features::kAILanguageModelOverrideConfigurationOutputBuffer.Get()));
  return std::min(
      limits.max_context_tokens,
      limits.max_tokens - std::min(limits.max_tokens, output_buffer));
}

}  // namespace

// Executes a single `Decide()` call (see the flow diagram in the header).
// Because `Append()` and `Score()` both mutate a session's context in place,
// any shared prefix (`input_session_`, `question_session_`) is cloned before
// appending the next part.
//
// Owned by `AILanguageModelDecisionModel`; `done_` is always the last step and
// may delete `this`.
class AILanguageModelDecisionModel::DecideRequest
    : public on_device_model::mojom::ContextClient {
 public:
  using DoneCallback = base::OnceCallback<void(DecideResult)>;

  // `input_session` is a newly created, empty session.
  DecideRequest(
      const DecisionModelSchemaCompiler::CompiledSchema& schema,
      const std::string& system_prompt,
      mojo::PendingRemote<on_device_model::mojom::Session> input_session,
      on_device_model::mojom::Priority priority,
      uint32_t input_token_limit)
      : schema_(schema),
        system_prompt_(system_prompt),
        input_token_limit_(input_token_limit),
        input_session_(std::move(input_session)),
        scores_(schema.questions.size()) {
    input_session_.set_disconnect_handler(base::BindOnce(
        &DecideRequest::OnSessionFailed, base::Unretained(this)));
    // Clones inherit this priority. Priority changes during an active
    // `Decide()` take effect on the next `Decide()`.
    input_session_->SetPriority(priority);
  }
  DecideRequest(const DecideRequest&) = delete;
  DecideRequest& operator=(const DecideRequest&) = delete;
  ~DecideRequest() override = default;

  void Start(std::string input_prompt, DoneCallback done) {
    done_ = std::move(done);
    std::vector<InputPiecePtr> pieces = InputPieces(std::move(input_prompt));
    auto input = on_device_model::mojom::Input::New(mojo::Clone(pieces));
    input_session_->GetSizeInTokens(
        std::move(input),
        base::BindOnce(&DecideRequest::OnInputSizeCounted,
                       weak_factory_.GetWeakPtr(), std::move(pieces)));
  }

 private:
  void OnInputSizeCounted(std::vector<InputPiecePtr> input_pieces,
                          uint32_t size) {
    // Non-empty input always has at least one token; 0 indicates a failure.
    if (size == 0) {
      Fail(blink::mojom::ModelStreamingResponseStatus::
               kErrorFailedToCountTokens);
      return;
    }
    if (size > input_token_limit_) {
      Fail(blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge,
           blink::mojom::QuotaErrorInfo::New(size, input_token_limit_));
      return;
    }
    // Append the system turn and input once; each question clones from this
    // state.
    std::vector<InputPiecePtr> pieces = SystemTurn(*system_prompt_);
    std::ranges::move(input_pieces, std::back_inserter(pieces));
    Append(*input_session_, std::move(pieces));
  }

  // Appends `pieces` to `session` and invokes `OnComplete()` when finished.
  void Append(on_device_model::mojom::Session& session,
              std::vector<InputPiecePtr> pieces) {
    session.Append(MakeAppendOptions(std::move(pieces)),
                   context_receiver_.BindNewPipeAndPassRemote());
    // If the append fails or is cancelled, the service disconnects the client
    // without calling `OnComplete()`.
    context_receiver_.set_disconnect_handler(base::BindOnce(
        &DecideRequest::OnSessionFailed, base::Unretained(this)));
  }

  // on_device_model::mojom::ContextClient:
  void OnComplete(uint32_t tokens_processed) override {
    context_receiver_.reset();
    if (!input_appended_) {
      input_appended_ = true;
      StartQuestion();
      return;
    }
    ScoreNextOption();
  }

  void StartQuestion() {
    if (question_index_ == schema_->questions.size()) {
      Finish();
      return;
    }
    question_session_.reset();
    input_session_->Clone(question_session_.BindNewPipeAndPassReceiver());
    question_session_.set_disconnect_handler(base::BindOnce(
        &DecideRequest::OnSessionFailed, base::Unretained(this)));
    Append(*question_session_,
           QuestionPieces(DecisionModelPromptBuilder::BuildQuestionPrompt(
               *schema_, question_index_)));
  }

  // Because `Session::Score()` appends the scored token to the session, each
  // option key is scored on a fresh clone of `question_session_`.
  void ScoreNextOption() {
    const size_t option_index = scores_[question_index_].size();
    if (option_index == schema_->questions[question_index_].options.size()) {
      ++question_index_;
      StartQuestion();
      return;
    }
    question_session_->Clone(option_session_.BindNewPipeAndPassReceiver());
    option_session_.set_disconnect_handler(base::BindOnce(
        &DecideRequest::OnSessionFailed, base::Unretained(this)));
    option_session_->Score(
        DecisionModelPromptBuilder::GetOptionKey(option_index),
        base::BindOnce(&DecideRequest::OnScore, weak_factory_.GetWeakPtr()));
  }

  void OnScore(float probability) {
    // Discard `option_session_` since `Score()` appended the option token to
    // it.
    option_session_.reset();
    scores_[question_index_].push_back(probability);
    ScoreNextOption();
  }

  void Finish() {
    auto result = DecisionModelSchemaCompiler::DecodeResult(*schema_, scores_);
    if (!result) {
      Fail(blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);
      return;
    }
    std::move(done_).Run(std::move(result));
  }

  void OnSessionFailed() {
    Fail(blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);
  }

  void Fail(blink::mojom::ModelStreamingResponseStatus status,
            blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
    if (done_) {
      std::move(done_).Run(DecideError(status, std::move(quota_error_info)));
    }
  }

  const raw_ref<const DecisionModelSchemaCompiler::CompiledSchema> schema_;
  const raw_ref<const std::string> system_prompt_;
  const uint32_t input_token_limit_;
  bool input_appended_ = false;
  // Session with the system turn and user input appended.
  mojo::Remote<on_device_model::mojom::Session> input_session_;
  // Clone of `input_session_` with question `question_index_` appended.
  mojo::Remote<on_device_model::mojom::Session> question_session_;
  // Clone of `question_session_` used to score a single option key.
  mojo::Remote<on_device_model::mojom::Session> option_session_;
  mojo::Receiver<on_device_model::mojom::ContextClient> context_receiver_{this};
  size_t question_index_ = 0;
  // Raw option probabilities per question.
  std::vector<std::vector<float>> scores_;
  DoneCallback done_;
  base::WeakPtrFactory<DecideRequest> weak_factory_{this};
};

AILanguageModelDecisionModel::AILanguageModelDecisionModel(
    AIContextBoundObjectSet& context_bound_object_set,
    DecisionModelSchemaCompiler::CompiledSchema schema,
    base::WeakPtr<optimization_guide::ModelClient> model_client,
    mojo::PendingReceiver<blink::mojom::AIDecisionModel> receiver,
    ReadyCallback on_ready)
    : AILanguageModelDecisionModel(
          context_bound_object_set,
          std::move(schema),
          base::BindRepeating(&CreateSession, model_client),
          std::move(receiver),
          std::move(on_ready),
          GetMaxTokens(*model_client),
          base::BindRepeating(&ReportHealthyCompletion, model_client)) {}

// static
std::unique_ptr<AILanguageModelDecisionModel>
AILanguageModelDecisionModel::CreateForTesting(
    AIContextBoundObjectSet& context_bound_object_set,
    DecisionModelSchemaCompiler::CompiledSchema schema,
    SessionFactory create_session,
    mojo::PendingReceiver<blink::mojom::AIDecisionModel> receiver,
    ReadyCallback on_ready,
    uint32_t max_tokens,
    base::RepeatingClosure on_decide_succeeded) {
  return base::WrapUnique(new AILanguageModelDecisionModel(
      context_bound_object_set, std::move(schema), std::move(create_session),
      std::move(receiver), std::move(on_ready), max_tokens,
      std::move(on_decide_succeeded)));
}

AILanguageModelDecisionModel::AILanguageModelDecisionModel(
    AIContextBoundObjectSet& context_bound_object_set,
    DecisionModelSchemaCompiler::CompiledSchema schema,
    SessionFactory create_session,
    mojo::PendingReceiver<blink::mojom::AIDecisionModel> receiver,
    ReadyCallback on_ready,
    uint32_t max_tokens,
    base::RepeatingClosure on_decide_succeeded)
    : AIContextBoundObject(context_bound_object_set),
      schema_(std::move(schema)),
      system_prompt_(DecisionModelPromptBuilder::BuildSystemPrompt(schema_)),
      token_limit_(max_tokens > kReservedTokens ? max_tokens - kReservedTokens
                                                : 0),
      on_ready_(std::move(on_ready)),
      create_session_(std::move(create_session)),
      on_decide_succeeded_(std::move(on_decide_succeeded)),
      receiver_(this, std::move(receiver)) {
  // `AIManager` validates that the schema has at least one question.
  CHECK(!schema_.questions.empty());
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
  create_session_.Run(token_count_session_.BindNewPipeAndPassReceiver());
  token_count_session_.set_disconnect_handler(base::BindOnce(
      &AILanguageModelDecisionModel::FailInitialization, base::Unretained(this),
      blink::mojom::AIManagerCreateClientError::kUnableToCreateSession,
      /*quota_error_info=*/nullptr));

  // Count the tokens for each question's prompt (system turn + question)
  // before completing initialization.
  auto on_counted = base::BarrierCallback<uint32_t>(
      schema_.questions.size(),
      base::BindOnce(&AILanguageModelDecisionModel::OnPromptSizesCounted,
                     weak_ptr_factory_.GetWeakPtr()));
  for (size_t q = 0; q < schema_.questions.size(); ++q) {
    token_count_session_->GetSizeInTokens(
        on_device_model::mojom::Input::New(PromptPieces(
            system_prompt_,
            DecisionModelPromptBuilder::BuildQuestionPrompt(schema_, q))),
        on_counted);
  }
}

AILanguageModelDecisionModel::~AILanguageModelDecisionModel() {
  // If destroyed before initialization finishes, notify the caller.
  if (on_ready_) {
    std::move(on_ready_).Run(
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession,
        /*quota_error_info=*/nullptr);
  }
}

void AILanguageModelDecisionModel::Decide(const std::string& input,
                                          DecideCallback callback) {
  // `AIManager` only binds the remote to the renderer after `on_ready_` runs.
  CHECK(ready_);
  // The renderer already rejects blank input.
  if (base::TrimWhitespaceASCII(input, base::TRIM_ALL).empty()) {
    receiver_.ReportBadMessage("Decide() input is empty");
    // `ReportBadMessage()` closes `receiver_` without running its disconnect
    // handler; remove `this` explicitly.
    RemoveFromSet();
    return;
  }
  // `base::Unretained(this)` is safe because `this` owns `pending_decides_`.
  pending_decides_.push_back(
      base::BindOnce(&AILanguageModelDecisionModel::StartDecide,
                     base::Unretained(this), input, std::move(callback)));
  MaybeStartNextDecide();
}

void AILanguageModelDecisionModel::SetPriority(
    on_device_model::mojom::Priority priority) {
  // Applied to sessions created for subsequent `Decide()` calls.
  priority_ = priority;
}

void AILanguageModelDecisionModel::OnPromptSizesCounted(
    std::vector<uint32_t> sizes) {
  // Every prompt is non-empty, so a size of 0 means token counting failed.
  if (std::ranges::contains(sizes, 0u)) {
    FailInitialization(
        blink::mojom::AIManagerCreateClientError::kUnableToCalculateTokenSize,
        /*quota_error_info=*/nullptr);
    return;
  }
  prompt_tokens_ = std::ranges::max(sizes);
  // Reserve at least 1 token for the user input.
  if (prompt_tokens_ >= token_limit_) {
    FailInitialization(
        blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(prompt_tokens_ + 1, token_limit_));
    return;
  }
  // Each `Decide()` creates its own session.
  token_count_session_.reset();
  ready_ = true;
  std::move(on_ready_).Run(/*error=*/std::nullopt,
                           /*quota_error_info=*/nullptr);
}

void AILanguageModelDecisionModel::FailInitialization(
    blink::mojom::AIManagerCreateClientError error,
    blink::mojom::QuotaErrorInfoPtr quota_error_info) {
  std::move(on_ready_).Run(error, std::move(quota_error_info));
  RemoveFromSet();  // Deletes `this`.
}

void AILanguageModelDecisionModel::MaybeStartNextDecide() {
  if (active_decide_ || pending_decides_.empty()) {
    return;
  }
  base::OnceClosure start_decide = std::move(pending_decides_.front());
  pending_decides_.pop_front();
  std::move(start_decide).Run();
}

void AILanguageModelDecisionModel::StartDecide(const std::string& input,
                                               DecideCallback callback) {
  mojo::PendingRemote<on_device_model::mojom::Session> input_session;
  create_session_.Run(input_session.InitWithNewPipeAndPassReceiver());
  active_decide_ = std::make_unique<DecideRequest>(
      schema_, system_prompt_, std::move(input_session), priority_,
      token_limit_ - prompt_tokens_);
  active_decide_->Start(
      DecisionModelPromptBuilder::BuildInputPrompt(input),
      base::BindOnce(&AILanguageModelDecisionModel::OnDecideFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AILanguageModelDecisionModel::OnDecideFinished(DecideCallback callback,
                                                    DecideResult result) {
  const bool succeeded = result.has_value();
  std::move(callback).Run(std::move(result));
  active_decide_.reset();
  if (succeeded) {
    on_decide_succeeded_.Run();
  }
  MaybeStartNextDecide();
}
