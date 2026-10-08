// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_DECISION_MODEL_SCHEMA_COMPILER_H_
#define CHROME_BROWSER_AI_DECISION_MODEL_SCHEMA_COMPILER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom.h"

// Validates a developer-defined decision schema and decodes per-option scores
// into `AIDecisionModelResult`s.
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
  static constexpr size_t kMaxScoreLevels = 9;
  static constexpr size_t kDefaultScoreLevels = 5;

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

  // Normalizes `question_option_scores[q][o]` (one non-negative weight per
  // option) into a result with the top label, confidence, full distribution,
  // and the expected value for score questions. Returns null if the shape
  // doesn't match `schema`.
  static blink::mojom::AIDecisionModelResultPtr DecodeResult(
      const CompiledSchema& schema,
      const std::vector<std::vector<float>>& question_option_scores);
};

#endif  // CHROME_BROWSER_AI_DECISION_MODEL_SCHEMA_COMPILER_H_
