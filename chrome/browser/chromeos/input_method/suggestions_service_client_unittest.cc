// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_service_client.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/text_suggester.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using ::chromeos::ime::TextCompletionCandidate;
using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

class SuggestionsServiceClientTest : public testing::Test {
 public:
  SuggestionsServiceClientTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SuggestionsServiceClientTest, ReturnsResultsFromMojoService) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  // Construct fake output
  auto result = machine_learning::mojom::TextSuggesterResult::New();
  result->status = machine_learning::mojom::TextSuggesterResult::Status::OK;
  auto multi_word = machine_learning::mojom::MultiWordSuggestionCandidate::New(
      /*text=*/"hi there", /*normalized_score=*/0.5f);
  auto candidate = machine_learning::mojom::TextSuggestionCandidate::New();
  candidate->set_multi_word(std::move(multi_word));
  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputTextSuggesterResult(result);

  SuggestionsServiceClient client;
  base::RunLoop().RunUntilIdle();

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
                     .text = "hi there"},
  };

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(returned_results, expected_results);
}

TEST_F(SuggestionsServiceClientTest, DoesntRequestPredictionCandidates) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  SuggestionsServiceClient client;
  base::RunLoop().RunUntilIdle();

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

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(returned_results.empty());
}

}  // namespace
}  // namespace chromeos
