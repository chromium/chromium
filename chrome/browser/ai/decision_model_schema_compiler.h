// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_DECISION_MODEL_SCHEMA_COMPILER_H_
#define CHROME_BROWSER_AI_DECISION_MODEL_SCHEMA_COMPILER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom.h"

// Validates and compiles a decision schema, decodes per-option weights into
// `AIDecisionModelResult`s for both the language model and embedding backends,
// and formats and scores passages for the embedding backend.
//
// For score questions, each option's value is its label as a number if every
// label in the question is a finite number (`base::StringToDouble()`), and its
// 1-based position otherwise. So options {"0", "5", "10"} give a score in
// [0, 10], and {"low", "mid", "high"} give a score in [1, 3]. This is the rule
// documented on `blink.mojom.AIDecisionModelDecision.score`.
class DecisionModelSchemaCompiler {
 public:
  // TODO(crbug.com/565849508): Track these limits with the manifest use case.
  static constexpr size_t kMaxQuestions = 16;
  static constexpr size_t kMinChoiceOptions = 2;
  static constexpr size_t kMaxChoiceOptions = 26;
  static constexpr size_t kMinScoreLevels = 2;
  static constexpr size_t kMaxScoreLevels = 11;
  static constexpr size_t kDefaultScoreLevels = 5;

  // TODO(crbug.com/565849508): Configure softmax temperature via model
  // metadata.
  static constexpr float kDefaultEmbeddingTemperature = 0.05f;

  struct CompiledOption {
    // The label returned to the developer (e.g. "true", "bug", "4").
    std::string label;
    // Optional criteria shown to the model next to the label.
    std::string description;
    // Score questions only: the option's value in the expected score.
    double value = 0;
  };

  struct CompiledQuestion {
    CompiledQuestion();
    CompiledQuestion(const CompiledQuestion&);
    CompiledQuestion& operator=(const CompiledQuestion&);
    CompiledQuestion(CompiledQuestion&&);
    CompiledQuestion& operator=(CompiledQuestion&&);
    ~CompiledQuestion();

    // Unique question identifier returned in each `AIDecisionModelDecision`.
    std::string id;
    // The question or classification criteria evaluated by the model.
    std::string prompt;
    // Question type (`kBoolean`, `kChoice`, or `kScore`).
    blink::mojom::AIDecisionModelQuestionType type =
        blink::mojom::AIDecisionModelQuestionType::kChoice;
    // Candidate options; boolean questions always have "true" then "false".
    std::vector<CompiledOption> options;
  };

  struct CompiledSchema {
    CompiledSchema();
    CompiledSchema(const CompiledSchema&);
    CompiledSchema& operator=(const CompiledSchema&);
    CompiledSchema(CompiledSchema&&);
    CompiledSchema& operator=(CompiledSchema&&);
    ~CompiledSchema();

    // Optional background instructions or task context shared across questions.
    std::string shared_context;
    // Ordered list of compiled questions to evaluate in each `Decide()` call.
    std::vector<CompiledQuestion> questions;
  };

  // Validates and compiles the schema. Returns a human-readable error if the
  // schema is empty, too large, or malformed.
  static base::expected<CompiledSchema, std::string> Compile(
      const std::optional<std::string>& shared_context,
      base::span<const blink::mojom::AIDecisionModelQuestionPtr> questions);

  // Returns the EmbeddingGemma document passage for a question's option.
  static std::string BuildOptionEmbeddingPassage(const CompiledSchema& schema,
                                                 size_t question_index,
                                                 size_t option_index);

  // Returns the EmbeddingGemma classification query passage for `input`.
  static std::string BuildInputEmbeddingPassage(std::string_view input);

  // Returns temperature-scaled cosine similarity weights for EmbeddingGemma.
  static std::vector<float> ScoreQuestionEmbeddings(
      const passage_embeddings::Embedding& input_embedding,
      base::span<const passage_embeddings::Embedding> option_embeddings,
      float temperature = kDefaultEmbeddingTemperature);

  // Normalizes `question_option_scores[q][o]` (one non-negative weight per
  // option) into a result with the top label, confidence, full distribution,
  // and the expected value for score questions. Returns null if the shape
  // doesn't match `schema`.
  static blink::mojom::AIDecisionModelResultPtr DecodeResult(
      const CompiledSchema& schema,
      const std::vector<std::vector<float>>& question_option_scores);
};

#endif  // CHROME_BROWSER_AI_DECISION_MODEL_SCHEMA_COMPILER_H_
