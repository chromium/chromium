// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/decision_model_prompt_builder.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/ai/decision_model_schema_compiler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom.h"

namespace {

using QuestionType = blink::mojom::AIDecisionModelQuestionType;

blink::mojom::AIDecisionModelQuestionPtr MakeQuestion(
    std::string id,
    QuestionType type,
    std::vector<std::pair<std::string, std::optional<std::string>>> options =
        {}) {
  auto question = blink::mojom::AIDecisionModelQuestion::New();
  question->id = std::move(id);
  question->prompt = "Prompt for " + question->id;
  question->type = type;
  for (auto& [label, description] : options) {
    question->options.push_back(blink::mojom::AIDecisionModelOption::New(
        std::move(label), std::move(description)));
  }
  return question;
}

DecisionModelSchemaCompiler::CompiledSchema CompileOrDie(
    base::span<const blink::mojom::AIDecisionModelQuestionPtr> questions) {
  auto schema = DecisionModelSchemaCompiler::Compile(questions);
  CHECK(schema.has_value()) << schema.error();
  return std::move(schema).value();
}

TEST(DecisionModelPromptBuilderTest, SystemPrompt) {
  const std::string system = DecisionModelPromptBuilder::BuildSystemPrompt();
  EXPECT_FALSE(system.empty());
}

TEST(DecisionModelPromptBuilderTest, InputPrompt) {
  EXPECT_EQ(DecisionModelPromptBuilder::BuildInputPrompt("hi"), "Input:\nhi");
}

TEST(DecisionModelPromptBuilderTest, BooleanQuestionUsesYesNo) {
  const auto schema = CompileOrDie({MakeQuestion("b", QuestionType::kBoolean)});

  EXPECT_EQ(DecisionModelPromptBuilder::BuildQuestionPrompt(schema, 0),
            "\n\nQuestion: Prompt for b\nA) true: yes\nB) false: no\n\n"
            "Reply with just the letter of the best option.");
}

TEST(DecisionModelPromptBuilderTest, ChoiceQuestionShowsDescriptions) {
  const auto schema = CompileOrDie({MakeQuestion(
      "c", QuestionType::kChoice,
      {{"bug", "Something is broken"}, {"billing", std::nullopt}})});

  EXPECT_EQ(DecisionModelPromptBuilder::BuildQuestionPrompt(schema, 0),
            "\n\nQuestion: Prompt for c\nA) bug: Something is broken\n"
            "B) billing\n\nReply with just the letter of the best option.");
}

TEST(DecisionModelPromptBuilderTest, DefaultScoreLevelsAreKeyed) {
  const auto schema = CompileOrDie({MakeQuestion("s", QuestionType::kScore)});

  EXPECT_EQ(DecisionModelPromptBuilder::BuildQuestionPrompt(schema, 0),
            "\n\nQuestion: Prompt for s\n"
            "A) 1: very low, strongly negative.\n"
            "B) 2: low, fairly negative.\n"
            "C) 3: moderate, medium, neutral.\n"
            "D) 4: high, fairly positive.\n"
            "E) 5: very high, strongly positive.\n\n"
            "Reply with just the letter of the best option.");
}

TEST(DecisionModelPromptBuilderTest, BlankDescriptionsAreLeftOut) {
  const auto schema = CompileOrDie({MakeQuestion(
      "c", QuestionType::kChoice, {{"bug", " "}, {"billing", ""}})});

  EXPECT_EQ(DecisionModelPromptBuilder::BuildQuestionPrompt(schema, 0),
            "\n\nQuestion: Prompt for c\nA) bug\nB) billing\n\n"
            "Reply with just the letter of the best option.");
}

TEST(DecisionModelPromptBuilderTest, OptionKeys) {
  EXPECT_EQ(DecisionModelPromptBuilder::GetOptionKey(0), "A");
  EXPECT_EQ(DecisionModelPromptBuilder::GetOptionKey(25), "Z");
}

}  // namespace
