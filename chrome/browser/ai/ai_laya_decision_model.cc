// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_laya_decision_model.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

base::unexpected<blink::mojom::AIDecisionModelDecideErrorPtr> DecideError(
    blink::mojom::ModelStreamingResponseStatus status) {
  return base::unexpected(blink::mojom::AIDecisionModelDecideError::New(
      status, /*quota_error_info=*/nullptr));
}

on_device_model::mojom::DecisionQuestionType ToDecisionQuestionType(
    blink::mojom::AIDecisionModelQuestionType type) {
  switch (type) {
    case blink::mojom::AIDecisionModelQuestionType::kBoolean:
      return on_device_model::mojom::DecisionQuestionType::kBoolean;
    case blink::mojom::AIDecisionModelQuestionType::kChoice:
      return on_device_model::mojom::DecisionQuestionType::kChoice;
    case blink::mojom::AIDecisionModelQuestionType::kScore:
      return on_device_model::mojom::DecisionQuestionType::kScore;
  }
}

// Uses Laya's trained `noul` option descriptions ("true: yes, the statement
// holds" / "false: no, the statement does not hold") instead of the description
// provided by the CompiledSchema.
std::string_view GetBooleanDescription(size_t option_index) {
  return option_index == 0 ? "yes, the statement holds"
                           : "no, the statement does not hold";
}

on_device_model::mojom::DecisionQueryPtr BuildDecisionQuery(
    const DecisionModelSchemaCompiler::CompiledSchema& schema) {
  auto query = on_device_model::mojom::DecisionQuery::New();
  query->questions.reserve(schema.questions.size());
  for (const auto& compiled_question : schema.questions) {
    auto question = on_device_model::mojom::DecisionQuestion::New();
    question->id = compiled_question.id;
    question->prompt = compiled_question.prompt;
    question->type = ToDecisionQuestionType(compiled_question.type);
    const bool is_boolean = compiled_question.type ==
                            blink::mojom::AIDecisionModelQuestionType::kBoolean;
    question->options.reserve(compiled_question.options.size());
    for (size_t i = 0; i < compiled_question.options.size(); ++i) {
      const auto& compiled_option = compiled_question.options[i];
      const std::string description =
          is_boolean ? std::string(GetBooleanDescription(i))
                     : compiled_option.description;
      question->options.push_back(on_device_model::mojom::DecisionOption::New(
          compiled_option.label, std::move(description)));
    }
    query->questions.push_back(std::move(question));
  }
  return query;
}

}  // namespace

AILayaDecisionModel::AILayaDecisionModel(
    AIContextBoundObjectSet& context_bound_object_set,
    DecisionModelSchemaCompiler::CompiledSchema schema,
    base::WeakPtr<optimization_guide::ModelClient> model_client,
    mojo::PendingReceiver<blink::mojom::AIDecisionModel> receiver)
    : AIContextBoundObject(context_bound_object_set),
      schema_(std::move(schema)),
      model_client_(std::move(model_client)),
      receiver_(this, std::move(receiver)) {
  CHECK(!schema_.questions.empty());
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
  EnsureSessionConnected();
}

AILayaDecisionModel::~AILayaDecisionModel() = default;

void AILayaDecisionModel::Decide(const std::string& input,
                                 DecideCallback callback) {
  if (base::TrimWhitespaceASCII(input, base::TRIM_ALL).empty()) {
    receiver_.ReportBadMessage("Decide() input is empty");
    // `ReportBadMessage()` closes `receiver_` without running its disconnect
    // handler; remove `this` explicitly.
    RemoveFromSet();
    return;
  }
  EnsureSessionConnected();
  if (!session_.is_bound()) {
    std::move(callback).Run(DecideError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure));
    return;
  }
  session_->Decide(
      input,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&AILayaDecisionModel::OnDecideComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          std::nullopt));
}

void AILayaDecisionModel::EnsureSessionConnected() {
  if (session_.is_bound() || !model_client_) {
    return;
  }
  model_client_->solution().CreateDecisionSession(
      BuildDecisionQuery(schema_), session_.BindNewPipeAndPassReceiver());
  session_.reset_on_disconnect();
}

void AILayaDecisionModel::OnDecideComplete(
    DecideCallback callback,
    std::optional<
        std::vector<on_device_model::mojom::DecisionQuestionResultPtr>>
        results) {
  if (!results.has_value()) {
    std::move(callback).Run(DecideError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure));
    return;
  }
  std::vector<std::vector<float>> scores;
  scores.reserve(results->size());
  for (auto& question_result : *results) {
    scores.push_back(std::move(question_result->probabilities));
  }
  auto result = DecisionModelSchemaCompiler::DecodeResult(schema_, scores);
  if (!result) {
    std::move(callback).Run(DecideError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure));
    return;
  }
  if (model_client_) {
    model_client_->solution().ReportHealthyCompletion();
  }
  std::move(callback).Run(std::move(result));
}
