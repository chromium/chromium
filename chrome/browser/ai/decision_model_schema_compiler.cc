// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/decision_model_schema_compiler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
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

// Centroid whitening weights matching MediaPipe's `EmbeddingDecisionBackend`.
constexpr float kBinaryCentroidWhiteningWeight = 0.50f;
constexpr float kMultiOptionCentroidWhiteningWeight = 0.85f;
constexpr double kMinWhitenedNormSq = 1e-24;

bool IsBlank(std::string_view text) {
  return base::TrimWhitespaceASCII(text, base::TRIM_ALL).empty();
}

// Formats an option label as words (e.g. "share_link" -> "share link").
// TODO(crbug.com/565849508): Refine this naive label formatting heuristic.
std::string FormatLabel(std::string_view label) {
  std::string formatted(label);
  for (size_t i = 0; i < formatted.size(); ++i) {
    if (formatted[i] == '_' ||
        (formatted[i] == '-' && i > 0 && i + 1 < formatted.size() &&
         base::IsAsciiAlpha(formatted[i - 1]) &&
         base::IsAsciiAlpha(formatted[i + 1]))) {
      formatted[i] = ' ';
    }
  }
  return formatted;
}

// Returns a default Likert-scale description for a score option across 2-11
// levels, with odd-numbered scales including a neutral midpoint.
// TODO(crbug.com/565849508): Evaluate default score descriptions, including
// rank ratios (e.g. "4/5"), negative or descending numeric label ordering, and
// magnitude vs. sentiment polarity wording.
std::string_view GetDefaultScoreDescription(size_t option_index,
                                            size_t option_count) {
  constexpr std::string_view kExtremelyLow =
      "extremely low, lowest, most negative.";
  constexpr std::string_view kVeryLow = "very low, strongly negative.";
  constexpr std::string_view kLow = "low, fairly negative.";
  constexpr std::string_view kSomewhatLow =
      "somewhat low, moderately negative.";
  constexpr std::string_view kSlightlyLow = "slightly low, slightly negative.";
  constexpr std::string_view kModerate = "moderate, medium, neutral.";
  constexpr std::string_view kSlightlyHigh =
      "slightly high, slightly positive.";
  constexpr std::string_view kSomewhatHigh =
      "somewhat high, moderately positive.";
  constexpr std::string_view kHigh = "high, fairly positive.";
  constexpr std::string_view kVeryHigh = "very high, strongly positive.";
  constexpr std::string_view kExtremelyHigh =
      "extremely high, highest, most positive.";

  CHECK_GE(option_count, DecisionModelSchemaCompiler::kMinScoreLevels);
  CHECK_LE(option_count, DecisionModelSchemaCompiler::kMaxScoreLevels);
  CHECK_LT(option_index, option_count);
  const size_t half = option_count / 2;
  if (option_count % 2 == 1) {
    if (option_index == half) {
      return kModerate;
    }
    if (option_index > half) {
      --option_index;
    }
  }
  switch (half) {
    case 1: {
      constexpr std::array kDescriptions = {kLow, kHigh};
      return kDescriptions[option_index];
    }
    case 2: {
      constexpr std::array kDescriptions = {kVeryLow, kLow, kHigh, kVeryHigh};
      return kDescriptions[option_index];
    }
    case 3: {
      constexpr std::array kDescriptions = {kVeryLow,      kLow,  kSlightlyLow,
                                            kSlightlyHigh, kHigh, kVeryHigh};
      return kDescriptions[option_index];
    }
    case 4: {
      constexpr std::array kDescriptions = {kExtremelyLow, kVeryLow,      kLow,
                                            kSlightlyLow,  kSlightlyHigh, kHigh,
                                            kVeryHigh,     kExtremelyHigh};
      return kDescriptions[option_index];
    }
    case 5: {
      constexpr std::array kDescriptions = {
          kExtremelyLow, kVeryLow,      kLow,  kSomewhatLow, kSlightlyLow,
          kSlightlyHigh, kSomewhatHigh, kHigh, kVeryHigh,    kExtremelyHigh};
      return kDescriptions[option_index];
    }
    default:
      NOTREACHED();
  }
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
// finite number, otherwise its 1-based position. Returns true if every label is
// a finite number.
bool SetScoreValues(
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
  return all_numbers;
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
                      {"false", "No, false, none, not applicable, unrelated."}};
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
          for (size_t i = 1; i <= kDefaultScoreLevels; ++i) {
            cq.options.push_back({base::NumberToString(i), std::string()});
          }
        } else {
          cq.options = CompileOptions(q->options);
        }
        if (SetScoreValues(cq.options) &&
            std::ranges::all_of(cq.options, &IsBlank,
                                &CompiledOption::description)) {
          for (size_t i = 0; i < cq.options.size(); ++i) {
            cq.options[i].description =
                std::string(GetDefaultScoreDescription(i, cq.options.size()));
          }
        }
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
std::string DecisionModelSchemaCompiler::BuildOptionEmbeddingPassage(
    const CompiledSchema& schema,
    size_t question_index,
    size_t option_index) {
  CHECK_LT(question_index, schema.questions.size());
  const CompiledQuestion& question = schema.questions[question_index];
  CHECK_LT(option_index, question.options.size());
  const CompiledOption& option = question.options[option_index];

  const std::string label = FormatLabel(option.label);
  const std::string_view detail = IsBlank(option.description)
                                      ? std::string_view(label)
                                      : option.description;
  // TODO(crbug.com/565849508): Move passage templates to a model config,
  // incorporate `shared_context`, and avoid truncating option details when long
  // prompts exceed the token window.
  switch (question.type) {
    case QuestionType::kScore:
      return base::StrCat({"title: Level ", label, " | text: Question: ",
                           question.prompt, " - ", detail});
    case QuestionType::kBoolean:
    case QuestionType::kChoice:
      return base::StrCat({"title: ", label, " | text: Question: ",
                           question.prompt, " - ", detail});
  }
}

// static
std::string DecisionModelSchemaCompiler::BuildInputEmbeddingPassage(
    std::string_view input) {
  // Omit `shared_context` and `question.prompt` for EmbeddingGemma; bi-encoder
  // queries degrade when schema text outweighs the input (Gemma 4 includes
  // `shared_context` via `DecisionModelPromptBuilder::BuildSystemPrompt()`).
  // TODO(crbug.com/565849508): Move per-model template differences to model
  // configs.
  return base::StrCat({"task: classification | query: ", input});
}

// static
std::vector<float> DecisionModelSchemaCompiler::ScoreQuestionEmbeddings(
    const passage_embeddings::Embedding& input_embedding,
    base::span<const passage_embeddings::Embedding> option_embeddings,
    float temperature) {
  if (option_embeddings.size() < 2 || !std::isfinite(temperature) ||
      temperature <= 0.0f) {
    return {};
  }
  const auto& input_data = input_embedding.GetData();
  const size_t dim = input_data.size();
  if (dim == 0) {
    return {};
  }

  // Whiten option embeddings by their weighted centroid before scoring.
  // TODO(crbug.com/565849508): Precompute per-question whitening scale at
  // session creation time.
  std::vector<float> weighted_centroid(dim, 0.0f);
  for (const auto& opt_emb : option_embeddings) {
    const auto& opt_data = opt_emb.GetData();
    if (opt_data.size() != dim) {
      return {};
    }
    for (size_t d = 0; d < dim; ++d) {
      weighted_centroid[d] += opt_data[d];
    }
  }
  const float whitening_weight = option_embeddings.size() == 2
                                     ? kBinaryCentroidWhiteningWeight
                                     : kMultiOptionCentroidWhiteningWeight;
  const float centroid_scale =
      whitening_weight / static_cast<float>(option_embeddings.size());
  for (float& c : weighted_centroid) {
    c *= centroid_scale;
  }

  std::vector<double> dots(option_embeddings.size(), 0.0);
  double total_norm_sq = 0.0;
  for (size_t i = 0; i < option_embeddings.size(); ++i) {
    const auto& opt_data = option_embeddings[i].GetData();
    double dot = 0.0;
    double norm_sq = 0.0;
    for (size_t d = 0; d < dim; ++d) {
      const double whitened =
          static_cast<double>(opt_data[d] - weighted_centroid[d]);
      dot += static_cast<double>(input_data[d]) * whitened;
      norm_sq += whitened * whitened;
    }
    dots[i] = dot;
    total_norm_sq += norm_sq;
  }

  // Normalize by the shared RMS residual norm so temperature scaling stays
  // calibrated without distorting relative option rankings by per-option
  // distance from the centroid.
  const double mean_norm_sq =
      total_norm_sq / static_cast<double>(option_embeddings.size());
  const double rms_norm =
      mean_norm_sq > kMinWhitenedNormSq ? std::sqrt(mean_norm_sq) : 0.0;

  std::vector<float> sims;
  sims.reserve(option_embeddings.size());
  float max_sim = -std::numeric_limits<float>::infinity();
  for (size_t i = 0; i < option_embeddings.size(); ++i) {
    const float sim = rms_norm > 0.0
                          ? static_cast<float>(dots[i] / rms_norm)
                          : input_embedding.ScoreWith(option_embeddings[i]);
    sims.push_back(sim);
    max_sim = std::max(max_sim, sim);
  }

  std::vector<float> weights(sims.size());
  for (size_t i = 0; i < sims.size(); ++i) {
    weights[i] = std::exp((sims[i] - max_sim) / temperature);
  }
  return weights;
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
