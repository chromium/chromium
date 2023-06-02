// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/suggestions_service_client.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/text_suggester.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

namespace machine_learning = ::chromeos::machine_learning;

using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;
using ime::AssistiveSuggestionType;
using ime::DecoderCompletionCandidate;

machine_learning::mojom::TextSuggesterResultPtr NoCandidate() {
  auto result = machine_learning::mojom::TextSuggesterResult::New();
  result->status = machine_learning::mojom::TextSuggesterResult::Status::OK;
  return result;
}

machine_learning::mojom::TextSuggesterResultPtr SingleCandidate(
    const std::string& text,
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

class SuggestionsServiceClientTest : public testing::Test {
 public:
  SuggestionsServiceClientTest() {
    machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    machine_learning::ServiceConnection::GetInstance()->Initialize();
    // After initializing a client, we need to wait for any pending tasks to
    // resolve (ie. the task to connect to the fake service connection).
    client_ = std::make_unique<SuggestionsServiceClient>();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SetTextSuggesterResult(
      machine_learning::mojom::TextSuggesterResultPtr result) {
    fake_service_connection_.SetOutputTextSuggesterResult(std::move(result));
  }

  void WaitForResults() { base::RunLoop().RunUntilIdle(); }

  SuggestionsServiceClient* client() { return client_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  machine_learning::FakeServiceConnectionImpl fake_service_connection_;
  std::unique_ptr<SuggestionsServiceClient> client_;
};

TEST_F(SuggestionsServiceClientTest, ReturnsCompletionResultsFromMojoService) {
  SetTextSuggesterResult(SingleCandidate("hi there completion", 0.5f));

  std::vector<AssistiveSuggestion> returned_results;
  client()->RequestSuggestions(
      /*preceding_text=*/"this is some text",
      /*suggestion_mode=*/AssistiveSuggestionMode::kCompletion,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {
            returned_results = results;
          }));
  WaitForResults();

  std::vector<AssistiveSuggestion> expected_results = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there completion"},
  };

  EXPECT_EQ(returned_results, expected_results);
}

TEST_F(SuggestionsServiceClientTest, ReturnsPredictionResultsFromMojoService) {
  SetTextSuggesterResult(SingleCandidate("hi there prediction", 0.5f));

  std::vector<AssistiveSuggestion> returned_results;
  client()->RequestSuggestions(
      /*preceding_text=*/"this is some text",
      /*suggestion_mode=*/AssistiveSuggestionMode::kPrediction,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {
            returned_results = results;
          }));
  WaitForResults();

  std::vector<AssistiveSuggestion> expected_results = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there prediction"},
  };

  EXPECT_EQ(returned_results, expected_results);
}

TEST_F(SuggestionsServiceClientTest, RecordsCandidateGenerationTimePerRequest) {
  SetTextSuggesterResult(SingleCandidate("hi there prediction", 0.5f));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.CandidateGenerationTime.MultiWord", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/"this is some text",
      /*suggestion_mode=*/AssistiveSuggestionMode::kPrediction,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.CandidateGenerationTime.MultiWord", 1);
}

TEST_F(SuggestionsServiceClientTest, RecordsPrecedingTextLengthPerRequest) {
  SetTextSuggesterResult(SingleCandidate("hi there prediction", 0.5f));
  std::string preceding_text =
      "This is some text that is very long, so long in fact it should be "
      "greater then 100 chars which is the limit currently set when "
      "trimming text sent to the suggestion service.";

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.PrecedingTextLength", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/preceding_text,
      /*suggestion_mode=*/AssistiveSuggestionMode::kPrediction,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.PrecedingTextLength", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.PrecedingTextLength",
      /*sample=*/preceding_text.size(), /*expected_bucket_count=*/1);
}

TEST_F(SuggestionsServiceClientTest, RecordsRequestCandidatesForCompletion) {
  SetTextSuggesterResult(SingleCandidate("hi there completion", 0.5f));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.RequestCandidates", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/"hello",
      /*suggestion_mode=*/AssistiveSuggestionMode::kCompletion,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.RequestCandidates", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.RequestCandidates",
      /*sample=*/MultiWordSuggestionType::kCompletion,
      /*expected_bucket_count=*/1);
}

TEST_F(SuggestionsServiceClientTest, RecordsRequestCandidatesForPrediction) {
  SetTextSuggesterResult(SingleCandidate("hi there prediction", 0.5f));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.RequestCandidates", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/"hello",
      /*suggestion_mode=*/AssistiveSuggestionMode::kPrediction,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.RequestCandidates", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.RequestCandidates",
      /*sample=*/MultiWordSuggestionType::kPrediction,
      /*expected_bucket_count=*/1);
}

TEST_F(SuggestionsServiceClientTest,
       DoesNotRecordCandidatesGeneratedWhenNoneReturnedForPrediction) {
  SetTextSuggesterResult(NoCandidate());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/"hello",
      /*suggestion_mode=*/AssistiveSuggestionMode::kPrediction,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated", 0);
}

TEST_F(SuggestionsServiceClientTest,
       DoesNotRecordCandidatesGeneratedWhenNoneReturnedForCompletion) {
  SetTextSuggesterResult(NoCandidate());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/"hello",
      /*suggestion_mode=*/AssistiveSuggestionMode::kCompletion,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated", 0);
}

TEST_F(SuggestionsServiceClientTest,
       RecordsCandidatesGeneratedWhenCandidateReturnedForPrediction) {
  SetTextSuggesterResult(SingleCandidate("hi there prediction", 0.5f));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/"hello",
      /*suggestion_mode=*/AssistiveSuggestionMode::kPrediction,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated",
      /*sample=*/MultiWordSuggestionType::kPrediction,
      /*expected_bucket_count=*/1);
}

TEST_F(SuggestionsServiceClientTest,
       RecordsCandidatesGeneratedWhenCandidateReturnedForCompletion) {
  SetTextSuggesterResult(SingleCandidate("hi there completion", 0.5f));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/"hello",
      /*suggestion_mode=*/AssistiveSuggestionMode::kCompletion,
      /*completion_candidates=*/std::vector<DecoderCompletionCandidate>{},
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated",
      /*sample=*/MultiWordSuggestionType::kCompletion,
      /*expected_bucket_count=*/1);
}

TEST_F(SuggestionsServiceClientTest,
       RecordsEmptyCandidateTextWhenCandidateTextMissing) {
  SetTextSuggesterResult(SingleCandidate("hi there completion", 0.5f));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.EmptyCandidate", 0);

  client()->RequestSuggestions(
      /*preceding_text=*/"hel",
      /*suggestion_mode=*/AssistiveSuggestionMode::kCompletion,
      /*completion_candidates=*/
      std::vector<DecoderCompletionCandidate>{
          DecoderCompletionCandidate{"hello", 0.1f},
          DecoderCompletionCandidate{"", 0.01f},
          DecoderCompletionCandidate{"", 0.001f},
      },
      /*callback=*/
      base::BindLambdaForTesting(
          [&](const std::vector<AssistiveSuggestion>& results) {}));
  WaitForResults();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.EmptyCandidate", 2);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.EmptyCandidate",
      /*sample=*/MultiWordSuggestionType::kCompletion,
      /*expected_bucket_count=*/2);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
