// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/suggestions_service_client.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/text_suggester.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

namespace machine_learning = ::chromeos::machine_learning;

using ime::TextCompletionCandidate;
using ime::TextSuggestion;
using ime::TextSuggestionMode;
using ime::TextSuggestionType;

class SuggestionsServiceClientTest : public testing::Test {
 public:
  SuggestionsServiceClientTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

machine_learning::mojom::TextSuggesterResultPtr GenerateMultiWordResult(
    std::string text,
    float score) {
  auto result = machine_learning::mojom::TextSuggesterResult::New();
  result->status = machine_learning::mojom::TextSuggesterResult::Status::OK;
  auto multi_word = machine_learning::mojom::MultiWordSuggestionCandidate::New(
      /*text=*/text, /*normalized_score=*/score);
  result->candidates.emplace_back(
      machine_learning::mojom::TextSuggestionCandidate::NewMultiWord(
          std::move(multi_word)));
  return result;
}

TEST_F(SuggestionsServiceClientTest, ReturnsCompletionResultsFromMojoService) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  // Construct fake output
  machine_learning::mojom::TextSuggesterResultPtr result =
      GenerateMultiWordResult("hi there completion", 0.5f);
  fake_service_connection.SetOutputTextSuggesterResult(std::move(result));

  SuggestionsServiceClient client;
  base::RunLoop().RunUntilIdle();

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.CandidateGenerationTime.MultiWord", 0);

  std::vector<TextSuggestion> returned_results;
  client.RequestSuggestions(
      /*preceding_text=*/"this is some text",
      /*suggestion_mode=*/TextSuggestionMode::kCompletion,
      /*completion_candidates=*/std::vector<TextCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<TextSuggestion>& results) {
            returned_results = results;
          }));

  std::vector<TextSuggestion> expected_results = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there completion"},
  };

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(returned_results, expected_results);
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.CandidateGenerationTime.MultiWord", 1);
}

TEST_F(SuggestionsServiceClientTest, ReturnsPredictionResultsFromMojoService) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  // Construct fake output
  machine_learning::mojom::TextSuggesterResultPtr result =
      GenerateMultiWordResult("hi there prediction", 0.5f);
  fake_service_connection.SetOutputTextSuggesterResult(std::move(result));

  SuggestionsServiceClient client;
  base::RunLoop().RunUntilIdle();

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.CandidateGenerationTime.MultiWord", 0);

  std::vector<TextSuggestion> returned_results;
  client.RequestSuggestions(
      /*preceding_text=*/"this is some text",
      /*suggestion_mode=*/TextSuggestionMode::kPrediction,
      /*completion_candidates=*/std::vector<TextCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<TextSuggestion>& results) {
            returned_results = results;
          }));

  std::vector<TextSuggestion> expected_results = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there prediction"},
  };

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(returned_results, expected_results);
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.CandidateGenerationTime.MultiWord", 1);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
