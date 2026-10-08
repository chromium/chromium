// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/decision_model_schema_compiler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

using QuestionType = blink::mojom::AIDecisionModelQuestionType;

DecisionModelSchemaCompiler::CompiledQuestion::CompiledQuestion() = default;
DecisionModelSchemaCompiler::CompiledQuestion::CompiledQuestion(
    const CompiledQuestion&) = default;
DecisionModelSchemaCompiler::CompiledQuestion&
DecisionModelSchemaCompiler::CompiledQuestion::operator=(
    const CompiledQuestion&) = default;
DecisionModelSchemaCompiler::CompiledQuestion::CompiledQuestion(
    CompiledQuestion&&) = default;
DecisionModelSchemaCompiler::CompiledQuestion&
DecisionModelSchemaCompiler::CompiledQuestion::operator=(CompiledQuestion&&) =
    default;
DecisionModelSchemaCompiler::CompiledQuestion::~CompiledQuestion() = default;

DecisionModelSchemaCompiler::CompiledSchema::CompiledSchema() = default;
DecisionModelSchemaCompiler::CompiledSchema::CompiledSchema(
    const CompiledSchema&) = default;
DecisionModelSchemaCompiler::CompiledSchema&
DecisionModelSchemaCompiler::CompiledSchema::operator=(const CompiledSchema&) =
    default;
DecisionModelSchemaCompiler::CompiledSchema::CompiledSchema(CompiledSchema&&) =
    default;
DecisionModelSchemaCompiler::CompiledSchema&
DecisionModelSchemaCompiler::CompiledSchema::operator=(CompiledSchema&&) =
    default;
DecisionModelSchemaCompiler::CompiledSchema::~CompiledSchema() = default;

namespace {

bool IsBlank(std::string_view text) {
  return base::TrimWhitespaceASCII(text, base::TRIM_ALL).empty();
}

std::vector<DecisionModelSchemaCompiler::CompiledOption> CompileOptions(
    const std::vector<blink::mojom::AIDecisionModelOptionPtr>& options) {
  std::vector<DecisionModelSchemaCompiler::CompiledOption> compiled;
  compiled.reserve(options.size());
  for (const auto& option : options) {
    compiled.push_back(
        {option->label, option->description.value_or(std::string())});
  }
  return compiled;
}

// Sets each score option's value: its label as a number if every label is a
// finite number, otherwise its 1-based position.
void SetScoreValues(
    std::vector<DecisionModelSchemaCompiler::CompiledOption>& options) {
  std::vector<double> numbers;
  numbers.reserve(options.size());
  for (const auto& option : options) {
    double number = 0;
    if (!base::StringToDouble(option.label, &number) ||
        !std::isfinite(number)) {
      break;
    }
    numbers.push_back(number);
  }
  const bool all_numbers = numbers.size() == options.size();
  for (size_t i = 0; i < options.size(); ++i) {
    options[i].value = all_numbers ? numbers[i] : static_cast<double>(i + 1);
  }
}

}  // namespace

// static
base::expected<DecisionModelSchemaCompiler::CompiledSchema, std::string>
DecisionModelSchemaCompiler::Compile(
    const std::optional<std::string>& shared_context,
    base::span<const blink::mojom::AIDecisionModelQuestionPtr> questions) {
  if (questions.empty()) {
    return base::unexpected("At least one question is required.");
  }
  if (questions.size() > kMaxQuestions) {
    return base::unexpected(
        base::StrCat({"At most ", base::NumberToString(kMaxQuestions),
                      " questions are supported."}));
  }

  CompiledSchema schema;
  schema.shared_context = shared_context.value_or(std::string());

  base::flat_set<std::string> ids;
  for (const auto& q : questions) {
    if (IsBlank(q->id)) {
      return base::unexpected("Question id must not be empty.");
    }
    if (IsBlank(q->prompt)) {
      return base::unexpected(base::StrCat(
          {"Question prompt in question '", q->id, "' must not be empty."}));
    }
    if (!ids.insert(q->id).second) {
      return base::unexpected(base::StrCat({"Duplicate question id: ", q->id}));
    }

    if (q->type == QuestionType::kBoolean && !q->options.empty()) {
      return base::unexpected(base::StrCat(
          {"Boolean question '", q->id, "' must not specify options."}));
    }

    base::flat_set<std::string> labels;
    for (const auto& option : q->options) {
      if (IsBlank(option->label)) {
        return base::unexpected(base::StrCat(
            {"Option labels in question '", q->id, "' must not be empty."}));
      }
      if (!labels.insert(option->label).second) {
        return base::unexpected(
            base::StrCat({"Duplicate option label '", option->label,
                          "' in question '", q->id, "'."}));
      }
    }

    CompiledQuestion cq;
    cq.id = q->id;
    cq.prompt = q->prompt;
    cq.type = q->type;

    switch (q->type) {
      case QuestionType::kBoolean:
        // TODO(crbug.com/565849508): Evaluate default boolean descriptions.
        cq.options = {{"true", "true, yes, affirmative."},
                      {"false", "false, no, negative."}};
        break;
      case QuestionType::kScore:
        if (!q->options.empty() && (q->options.size() < kMinScoreLevels ||
                                    q->options.size() > kMaxScoreLevels)) {
          return base::unexpected(base::StrCat(
              {"Score question '", q->id, "' needs between ",
               base::NumberToString(kMinScoreLevels), " and ",
               base::NumberToString(kMaxScoreLevels), " levels."}));
        }
        if (q->options.empty()) {
          // TODO(crbug.com/565849508): Evaluate default score descriptions
          // (e.g. "low", "high" per Likert scale).
          for (size_t i = 1; i <= kDefaultScoreLevels; ++i) {
            cq.options.push_back({base::NumberToString(i), std::string()});
          }
        } else {
          cq.options = CompileOptions(q->options);
        }
        SetScoreValues(cq.options);
        break;
      case QuestionType::kChoice:
        if (q->options.size() < kMinChoiceOptions ||
            q->options.size() > kMaxChoiceOptions) {
          return base::unexpected(base::StrCat(
              {"Choice question '", q->id, "' needs between ",
               base::NumberToString(kMinChoiceOptions), " and ",
               base::NumberToString(kMaxChoiceOptions), " options."}));
        }
        cq.options = CompileOptions(q->options);
        break;
    }

    schema.questions.push_back(std::move(cq));
  }

  return schema;
}

// static
blink::mojom::AIDecisionModelResultPtr
DecisionModelSchemaCompiler::DecodeResult(
    const CompiledSchema& schema,
    const std::vector<std::vector<float>>& question_option_scores) {
  if (question_option_scores.size() != schema.questions.size()) {
    return nullptr;
  }

  auto result = blink::mojom::AIDecisionModelResult::New();
  for (size_t q = 0; q < schema.questions.size(); ++q) {
    const CompiledQuestion& cq = schema.questions[q];
    const std::vector<float>& raw = question_option_scores[q];
    const size_t num_options = cq.options.size();
    if (raw.size() != num_options || num_options == 0) {
      return nullptr;
    }

    std::vector<float> probs(num_options);
    double sum = 0.0;
    // TODO(crbug.com/565849508): Handle negative values, maximums, and
    // overflow.
    for (size_t i = 0; i < num_options; ++i) {
      probs[i] = std::isfinite(raw[i]) ? std::max(raw[i], 0.0f) : 0.0f;
      sum += probs[i];
    }
    // TODO(crbug.com/565849508): Handle zero sum (e.g. report a failure).
    for (float& p : probs) {
      p = sum > 0.0 ? static_cast<float>(p / sum) : 1.0f / num_options;
    }

    const size_t top = static_cast<size_t>(
        std::max_element(probs.begin(), probs.end()) - probs.begin());

    auto decision = blink::mojom::AIDecisionModelDecision::New();
    decision->question_id = cq.id;
    decision->top_label = cq.options[top].label;
    decision->confidence = probs[top];
    for (size_t i = 0; i < num_options; ++i) {
      decision->probabilities.push_back(
          blink::mojom::AIDecisionModelOptionProbability::New(
              cq.options[i].label, probs[i]));
    }
    if (cq.type == QuestionType::kScore) {
      double expected_score = 0;
      for (size_t i = 0; i < num_options; ++i) {
        expected_score += cq.options[i].value * probs[i];
      }
      // Labels like "1e300" are finite doubles but don't fit in a float.
      decision->score = static_cast<float>(std::clamp<double>(
          expected_score, std::numeric_limits<float>::lowest(),
          std::numeric_limits<float>::max()));
    }
    result->decisions.push_back(std::move(decision));
  }
  return result;
}
