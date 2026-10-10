// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_DECISION_MODEL_PROMPT_BUILDER_H_
#define CHROME_BROWSER_AI_DECISION_MODEL_PROMPT_BUILDER_H_

#include <cstddef>
#include <string>
#include <string_view>

#include "chrome/browser/ai/decision_model_schema_compiler.h"

// Builds the language model prompts that `AILanguageModelDecisionModel`
// scores with `on_device_model::mojom::Session::Score()`.
//
// Each scoring context consists of a system turn (`BuildSystemPrompt()`), a
// user turn (`BuildInputPrompt()` followed by `BuildQuestionPrompt()`), and
// the start of a model turn where single-letter option keys (`GetOptionKey()`)
// are scored. Using single-letter keys ensures every option is scored as a
// single distinct token.
class DecisionModelPromptBuilder {
 public:
  using CompiledSchema = DecisionModelSchemaCompiler::CompiledSchema;

  DecisionModelPromptBuilder() = delete;

  // Returns the single-letter key ("A".."Z") for the option at `index`.
  static std::string GetOptionKey(size_t index);

  // Returns the system prompt containing the task instructions and optional
  // shared context.
  static std::string BuildSystemPrompt(const CompiledSchema& schema);

  // Returns the first part of the user turn containing the input to classify.
  static std::string BuildInputPrompt(std::string_view input);

  // Returns the second part of the user turn: question `question_index` and
  // its lettered options. Each question is prompted separately so questions do
  // not bias one another.
  static std::string BuildQuestionPrompt(const CompiledSchema& schema,
                                         size_t question_index);
};

#endif  // CHROME_BROWSER_AI_DECISION_MODEL_PROMPT_BUILDER_H_
