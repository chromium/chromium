// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/decision_model_schema_compiler.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom.h"

namespace {

using QuestionType = blink::mojom::AIDecisionModelQuestionType;
using ::testing::ElementsAre;
using ::testing::FloatNear;

blink::mojom::AIDecisionModelQuestionPtr MakeQuestion(
    std::string id,
    QuestionType type,
    std::vector<std::string> labels = {}) {
  auto question = blink::mojom::AIDecisionModelQuestion::New();
  question->id = std::move(id);
  question->prompt = "Prompt for " + question->id;
  question->type = type;
  for (auto& label : labels) {
    question->options.push_back(blink::mojom::AIDecisionModelOption::New(
        std::move(label), std::nullopt));
  }
  return question;
}

std::vector<std::string> Labels(
    const DecisionModelSchemaCompiler::CompiledQuestion& question) {
  std::vector<std::string> labels;
  for (const auto& option : question.options) {
    labels.push_back(option.label);
  }
  return labels;
}

TEST(DecisionModelSchemaCompilerTest, CompilesEachQuestionType) {
  auto schema = DecisionModelSchemaCompiler::Compile(
      "ctx",
      {MakeQuestion("b", QuestionType::kBoolean),
       MakeQuestion("c", QuestionType::kChoice, {"x", "y"}),
       MakeQuestion("s", QuestionType::kScore),
       MakeQuestion("s3", QuestionType::kScore, {"low", "mid", "high"})});
  ASSERT_TRUE(schema.has_value()) << schema.error();
  EXPECT_EQ(schema->shared_context, "ctx");
  ASSERT_EQ(schema->questions.size(), 4u);
  EXPECT_THAT(Labels(schema->questions[0]), ElementsAre("true", "false"));
  EXPECT_THAT(Labels(schema->questions[1]), ElementsAre("x", "y"));
  EXPECT_THAT(Labels(schema->questions[2]),
              ElementsAre("1", "2", "3", "4", "5"));
  EXPECT_THAT(Labels(schema->questions[3]), ElementsAre("low", "mid", "high"));
}

TEST(DecisionModelSchemaCompilerTest, RejectsInvalidSchemas) {
  EXPECT_FALSE(
      DecisionModelSchemaCompiler::Compile(std::nullopt, {}).has_value());

  std::vector<blink::mojom::AIDecisionModelQuestionPtr> too_many;
  for (size_t i = 0; i <= DecisionModelSchemaCompiler::kMaxQuestions; ++i) {
    too_many.push_back(
        MakeQuestion("q" + base::NumberToString(i), QuestionType::kBoolean));
  }
  EXPECT_FALSE(
      DecisionModelSchemaCompiler::Compile(std::nullopt, too_many).has_value());

  EXPECT_FALSE(DecisionModelSchemaCompiler::Compile(
                   std::nullopt, {MakeQuestion("q", QuestionType::kBoolean),
                                  MakeQuestion("q", QuestionType::kBoolean)})
                   .has_value());

  EXPECT_FALSE(DecisionModelSchemaCompiler::Compile(
                   std::nullopt, {MakeQuestion("  ", QuestionType::kBoolean)})
                   .has_value());
  auto blank_prompt = MakeQuestion("q", QuestionType::kBoolean);
  blank_prompt->prompt = " \t ";
  EXPECT_FALSE(DecisionModelSchemaCompiler::Compile(std::nullopt,
                                                    {std::move(blank_prompt)})
                   .has_value());
  EXPECT_FALSE(
      DecisionModelSchemaCompiler::Compile(
          std::nullopt, {MakeQuestion("b", QuestionType::kBoolean, {"true"})})
          .has_value());
  EXPECT_FALSE(DecisionModelSchemaCompiler::Compile(
                   std::nullopt, {MakeQuestion("c", QuestionType::kChoice)})
                   .has_value());
  EXPECT_FALSE(DecisionModelSchemaCompiler::Compile(
                   std::nullopt,
                   {MakeQuestion("c", QuestionType::kChoice, {"only_one"})})
                   .has_value());
  EXPECT_FALSE(
      DecisionModelSchemaCompiler::Compile(
          std::nullopt, {MakeQuestion("c", QuestionType::kChoice, {"a", " "})})
          .has_value());
  EXPECT_FALSE(
      DecisionModelSchemaCompiler::Compile(
          std::nullopt, {MakeQuestion("c", QuestionType::kChoice, {"a", "a"})})
          .has_value());

  std::vector<std::string> too_many_options;
  for (size_t i = 0; i <= DecisionModelSchemaCompiler::kMaxChoiceOptions; ++i) {
    too_many_options.push_back("opt" + base::NumberToString(i));
  }
  EXPECT_FALSE(DecisionModelSchemaCompiler::Compile(
                   std::nullopt,
                   {MakeQuestion("c", QuestionType::kChoice, too_many_options)})
                   .has_value());

  EXPECT_FALSE(
      DecisionModelSchemaCompiler::Compile(
          std::nullopt, {MakeQuestion("s", QuestionType::kScore, {"only_one"})})
          .has_value());

  std::vector<std::string> too_many_levels;
  for (size_t i = 0; i <= DecisionModelSchemaCompiler::kMaxScoreLevels; ++i) {
    too_many_levels.push_back(base::NumberToString(i));
  }
  EXPECT_FALSE(DecisionModelSchemaCompiler::Compile(
                   std::nullopt,
                   {MakeQuestion("s", QuestionType::kScore, too_many_levels)})
                   .has_value());
}

TEST(DecisionModelSchemaCompilerTest, AllowsSurroundingWhitespace) {
  auto q = MakeQuestion(" q ", QuestionType::kChoice, {" a\nb ", "c"});
  q->prompt = "line 1\nline 2";
  q->options[0]->description = "desc 1\ndesc 2";
  EXPECT_TRUE(
      DecisionModelSchemaCompiler::Compile("ctx 1\nctx 2", {std::move(q)})
          .has_value());
}

TEST(DecisionModelSchemaCompilerTest, DecodeResultAndNumericScoreWeighting) {
  auto schema = DecisionModelSchemaCompiler::Compile(
      std::nullopt,
      {MakeQuestion("b", QuestionType::kBoolean),
       MakeQuestion("s", QuestionType::kScore, {"0", "5", "10"})});
  ASSERT_TRUE(schema.has_value());

  auto result = DecisionModelSchemaCompiler::DecodeResult(
      *schema, {{0.2f, 0.6f}, {0.0f, 0.5f, 0.5f}});
  ASSERT_TRUE(result);
  ASSERT_EQ(result->decisions.size(), 2u);

  EXPECT_EQ(result->decisions[0]->top_label, "false");
  EXPECT_THAT(result->decisions[0]->confidence, FloatNear(0.75f, 1e-5));
  ASSERT_EQ(result->decisions[0]->probabilities.size(), 2u);
  EXPECT_EQ(result->decisions[0]->probabilities[0]->label, "true");
  EXPECT_THAT(result->decisions[0]->probabilities[0]->probability,
              FloatNear(0.25f, 1e-5));
  EXPECT_FALSE(result->decisions[0]->score.has_value());

  EXPECT_THAT(*result->decisions[1]->score, FloatNear(7.5f, 1e-5));

  // Negative and non-finite option weights are clamped to zero.
  auto clamped = DecisionModelSchemaCompiler::DecodeResult(
      *schema,
      {{-1.0f, 2.0f}, {std::numeric_limits<float>::quiet_NaN(), -5.0f, 4.0f}});
  ASSERT_TRUE(clamped);
  EXPECT_EQ(clamped->decisions[0]->top_label, "false");
  EXPECT_THAT(clamped->decisions[0]->confidence, FloatNear(1.0f, 1e-5));
  EXPECT_THAT(*clamped->decisions[1]->score, FloatNear(10.0f, 1e-5));

  // All-zero weights fall back to a uniform distribution.
  auto uniform = DecisionModelSchemaCompiler::DecodeResult(
      *schema, {{0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
  ASSERT_TRUE(uniform);
  EXPECT_THAT(uniform->decisions[0]->confidence, FloatNear(0.5f, 1e-5));
  EXPECT_THAT(*uniform->decisions[1]->score, FloatNear(5.0f, 1e-5));

  // Shape mismatches return null.
  EXPECT_FALSE(DecisionModelSchemaCompiler::DecodeResult(*schema, {{1.0f}}));
}

TEST(DecisionModelSchemaCompilerTest,
     ScoreUsesPositionsUnlessAllLabelsAreNumbers) {
  auto schema = DecisionModelSchemaCompiler::Compile(
      std::nullopt,
      {MakeQuestion("words", QuestionType::kScore, {"low", "mid", "high"}),
       MakeQuestion("mixed", QuestionType::kScore, {"1", "2", "3+"}),
       MakeQuestion("neg_pos", QuestionType::kScore, {"-1", "0", "0.5"}),
       MakeQuestion("all_neg", QuestionType::kScore, {"-10", "-5", "-1"})});
  ASSERT_TRUE(schema.has_value());

  auto result =
      DecisionModelSchemaCompiler::DecodeResult(*schema, {{0.0f, 0.5f, 0.5f},
                                                          {0.0f, 0.0f, 1.0f},
                                                          {0.5f, 0.0f, 0.5f},
                                                          {0.5f, 0.5f, 0.0f}});
  ASSERT_TRUE(result);
  // Word and partially-numeric scales fall back to 1..N positions.
  EXPECT_THAT(*result->decisions[0]->score, FloatNear(2.5f, 1e-5));
  EXPECT_THAT(*result->decisions[1]->score, FloatNear(3.0f, 1e-5));
  // Negative and fractional numbers are parsed as numbers.
  EXPECT_THAT(*result->decisions[2]->score, FloatNear(-0.25f, 1e-5));
  EXPECT_THAT(*result->decisions[3]->score, FloatNear(-7.5f, 1e-5));
}

TEST(DecisionModelSchemaCompilerTest, ScoreStaysFiniteForHugeLabels) {
  constexpr float kMaxFloat = std::numeric_limits<float>::max();
  auto schema = DecisionModelSchemaCompiler::Compile(
      std::nullopt,
      {MakeQuestion("single_huge", QuestionType::kScore, {"0", "1e300"}),
       MakeQuestion("sum_pos_overflow", QuestionType::kScore,
                    {"1.79e308", "1.795e308", "1.797e308"}),
       MakeQuestion("sum_neg_overflow", QuestionType::kScore,
                    {"-1.79e308", "-1.795e308", "-1.797e308"})});
  ASSERT_TRUE(schema.has_value());
  auto result = DecisionModelSchemaCompiler::DecodeResult(
      *schema, {{0.0f, 1.0f},
                {kMaxFloat, kMaxFloat, kMaxFloat},
                {kMaxFloat, kMaxFloat, kMaxFloat}});
  ASSERT_TRUE(result);
  EXPECT_EQ(*result->decisions[0]->score, kMaxFloat);
  EXPECT_EQ(*result->decisions[1]->score, kMaxFloat);
  EXPECT_THAT(result->decisions[1]->confidence, FloatNear(1.0f / 3.0f, 1e-5));
  EXPECT_EQ(*result->decisions[2]->score, std::numeric_limits<float>::lowest());
}

}  // namespace
