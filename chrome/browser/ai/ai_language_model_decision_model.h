// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_DECISION_MODEL_H_
#define CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_DECISION_MODEL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/decision_model_schema_compiler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom.h"

class AIContextBoundObjectSet;

namespace optimization_guide {
class ModelClient;
}  // namespace optimization_guide

// Browser-side implementation of `blink::mojom::AIDecisionModel` backed by the
// on-device language model (e.g. Gemma 4). Each question is formatted as a
// multiple-choice prompt with lettered options ("A".."Z"), and the model's
// probability of generating each letter is used as that option's score.
//
// Session and scoring flow:
//   Init (ctor), on `token_count_session_`:
//     |-- GetSizeInTokens("<system>{system}<end>{question q}<end><model>")
//     |   for each question q
//     `-- Reserve the largest question's token count; the rest of
//     `token_limit_`
//         is the max input size.
//
//   Each `Decide(input)`, run one at a time via `pending_decides_`:
//     `input_session_` (new session)
//       |-- GetSizeInTokens("<user>Input:\n{input}") and check against max
//       input
//       `-- Append("<system>{system}<end><user>Input:\n{input}")
//             +-- Clone -> `question_session_` (for each question q)
//                   `-- Append("\n\nQuestion: {question q}<end><model>")
//                         +-- Clone -> `option_session_` (for each option o)
//                               `-- Score("A".."Z") -> raw option probability
//     `DecisionModelSchemaCompiler::DecodeResult()` normalizes each question's
//     raw probabilities into the result.
//
// Token counts are checked before appending because exceeding the model's
// context limit crashes the model service. A failed `Decide()` only rejects
// that call; the model remains usable.
//
// TODO(crbug.com/565849508): Append the system turn once at creation and clone
// it per `Decide()` once LiteRT-LM fixes a KV-cache bug where cloning the same
// session multiple times shifts scores on subsequent clones.
// TODO(crbug.com/565849508): Score all options for a question in one pass
// (e.g. LiteRT-LM's RunChoiceScoring) instead of cloning per option.
class AILanguageModelDecisionModel : public AIContextBoundObject,
                                     public blink::mojom::AIDecisionModel {
 public:
  // Called once initialization succeeds (`error` is `std::nullopt`) or fails.
  // `quota_error_info` is populated when `error` is `kInitialInputTooLarge`.
  using ReadyCallback = base::OnceCallback<void(
      std::optional<blink::mojom::AIManagerCreateClientError> error,
      blink::mojom::QuotaErrorInfoPtr quota_error_info)>;

  // Starts a new session on the model, or drops `session` if the model is
  // unavailable.
  using SessionFactory = base::RepeatingCallback<void(
      mojo::PendingReceiver<on_device_model::mojom::Session> session)>;

  // Safety margin subtracted from the model's context limit to account for
  // minor token-counting differences when pieces are combined.
  static constexpr uint32_t kReservedTokens = 16;

  // Scores with `model_client`'s model, which must be ready. `on_ready`
  // replies to the page's `create()` request.
  AILanguageModelDecisionModel(
      AIContextBoundObjectSet& context_bound_object_set,
      DecisionModelSchemaCompiler::CompiledSchema schema,
      base::WeakPtr<optimization_guide::ModelClient> model_client,
      mojo::PendingReceiver<blink::mojom::AIDecisionModel> receiver,
      ReadyCallback on_ready);
  AILanguageModelDecisionModel(const AILanguageModelDecisionModel&) = delete;
  AILanguageModelDecisionModel& operator=(const AILanguageModelDecisionModel&) =
      delete;
  ~AILanguageModelDecisionModel() override;

  // Like the constructor, but takes custom callbacks and `max_tokens` for unit
  // tests.
  static std::unique_ptr<AILanguageModelDecisionModel> CreateForTesting(
      AIContextBoundObjectSet& context_bound_object_set,
      DecisionModelSchemaCompiler::CompiledSchema schema,
      SessionFactory create_session,
      mojo::PendingReceiver<blink::mojom::AIDecisionModel> receiver,
      ReadyCallback on_ready,
      uint32_t max_tokens,
      base::RepeatingClosure on_decide_succeeded);

  // blink::mojom::AIDecisionModel:
  void Decide(const std::string& input, DecideCallback callback) override;

  // AIContextBoundObject:
  void SetPriority(on_device_model::mojom::Priority priority) override;

 private:
  class DecideRequest;

  using DecideResult =
      base::expected<blink::mojom::AIDecisionModelResultPtr,
                     blink::mojom::AIDecisionModelDecideErrorPtr>;

  AILanguageModelDecisionModel(
      AIContextBoundObjectSet& context_bound_object_set,
      DecisionModelSchemaCompiler::CompiledSchema schema,
      SessionFactory create_session,
      mojo::PendingReceiver<blink::mojom::AIDecisionModel> receiver,
      ReadyCallback on_ready,
      uint32_t max_tokens,
      base::RepeatingClosure on_decide_succeeded);

  // Called with the token count of the system turn plus each question's prompt.
  void OnPromptSizesCounted(std::vector<uint32_t> sizes);

  // Runs `on_ready_` with `error` and deletes `this`. Only called before
  // initialization completes.
  void FailInitialization(blink::mojom::AIManagerCreateClientError error,
                          blink::mojom::QuotaErrorInfoPtr quota_error_info);

  // Starts the next queued `Decide()` if none is currently running.
  void MaybeStartNextDecide();
  void StartDecide(const std::string& input, DecideCallback callback);
  void OnDecideFinished(DecideCallback callback, DecideResult result);

  const DecisionModelSchemaCompiler::CompiledSchema schema_;
  const std::string system_prompt_;
  // Maximum tokens allowed for any single scoring context.
  const uint32_t token_limit_;
  // Token count of the system turn plus the largest question prompt. The
  // remaining `token_limit_ - prompt_tokens_` tokens are available for input.
  uint32_t prompt_tokens_ = 0;
  bool ready_ = false;
  ReadyCallback on_ready_;
  const SessionFactory create_session_;
  const base::RepeatingClosure on_decide_succeeded_;
  on_device_model::mojom::Priority priority_ =
      on_device_model::mojom::Priority::kForeground;

  // Used only during initialization to count schema tokens.
  mojo::Remote<on_device_model::mojom::Session> token_count_session_;

  // Queued here to minimize ODMS queue overhead with concurrent sessions.
  base::circular_deque<base::OnceClosure> pending_decides_;
  std::unique_ptr<DecideRequest> active_decide_;

  mojo::Receiver<blink::mojom::AIDecisionModel> receiver_;

  base::WeakPtrFactory<AILanguageModelDecisionModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_DECISION_MODEL_H_
