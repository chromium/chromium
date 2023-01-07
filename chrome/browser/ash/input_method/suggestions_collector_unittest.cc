// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/suggestions_collector.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/input_method/suggestions_source.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;
using ime::AssistiveSuggestionType;
using ime::DecoderCompletionCandidate;

class FakeAssistiveSuggester : public SuggestionsSource {
 public:
  std::vector<AssistiveSuggestion> GetSuggestions() override {
    return suggestions_;
  }

  void SetSuggestions(const std::vector<AssistiveSuggestion> suggestions) {
    suggestions_ = suggestions;
  }

 private:
  std::vector<AssistiveSuggestion> suggestions_;
};

class FakeSuggestionsService : public AsyncSuggestionsSource {
 public:
  void RequestSuggestions(
      const std::string& preceding_text,
      const ime::AssistiveSuggestionMode& suggestion_mode,
      const std::vector<DecoderCompletionCandidate>& completion_candidates,
      RequestSuggestionsCallback callback) override {
    std::move(callback).Run(suggestions_);
  }

  bool IsAvailable() override { return is_available_; }

  void SetSuggestions(const std::vector<AssistiveSuggestion> suggestions) {
    suggestions_ = suggestions;
  }

  void SetIsAvailable(bool is_available) { is_available_ = is_available; }

 private:
  std::vector<AssistiveSuggestion> suggestions_;
  bool is_available_ = true;
};

class SuggestionsCollectorTest : public ::testing::Test {
 public:
  void SetUp() override {
    multi_word_result_ =
        AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                            .type = AssistiveSuggestionType::kMultiWord,
                            .text = "hello there"};

    personal_info_name_result_ = AssistiveSuggestion{
        .mode = AssistiveSuggestionMode::kCompletion,
        .type = AssistiveSuggestionType::kAssistivePersonalInfo,
        .text = "my name is Mr Robot"};

    personal_info_address_result_ = AssistiveSuggestion{
        .mode = AssistiveSuggestionMode::kCompletion,
        .type = AssistiveSuggestionType::kAssistivePersonalInfo,
        .text = "my address is 123 Fake St"};
  }

  std::vector<AssistiveSuggestion> suggestions_returned() {
    return suggestions_returned_;
  }

  AssistiveSuggestion multi_word_result() { return multi_word_result_; }
  AssistiveSuggestion personal_info_name_result() {
    return personal_info_name_result_;
  }
  AssistiveSuggestion personal_info_address_result() {
    return personal_info_address_result_;
  }

  void OnSuggestionsReturned(ime::mojom::SuggestionsResponsePtr response) {
    suggestions_returned_ = response->candidates;
  }

 private:
  std::vector<AssistiveSuggestion> suggestions_returned_;
  AssistiveSuggestion multi_word_result_;
  AssistiveSuggestion personal_info_name_result_;
  AssistiveSuggestion personal_info_address_result_;
};

TEST_F(SuggestionsCollectorTest, ReturnsResultsFromAssistiveSuggester) {
  FakeAssistiveSuggester suggester;
  auto requestor = std::make_unique<FakeSuggestionsService>();

  auto expected_results = std::vector<AssistiveSuggestion>{
      personal_info_name_result(),
      personal_info_address_result(),
  };

  suggester.SetSuggestions(expected_results);
  SuggestionsCollector collector(&suggester, std::move(requestor));

  collector.GatherSuggestions(
      ime::mojom::SuggestionsRequest::New(),
      base::BindOnce(&SuggestionsCollectorTest::OnSuggestionsReturned,
                     base::Unretained(this)));

  EXPECT_EQ(suggestions_returned(), expected_results);
}

TEST_F(SuggestionsCollectorTest, ReturnsResultsFromSuggestionsRequestor) {
  FakeAssistiveSuggester suggester;
  auto requestor = std::make_unique<FakeSuggestionsService>();

  auto expected_results = std::vector<AssistiveSuggestion>{multi_word_result()};

  requestor->SetSuggestions(expected_results);
  SuggestionsCollector collector(&suggester, std::move(requestor));

  collector.GatherSuggestions(
      ime::mojom::SuggestionsRequest::New(),
      base::BindOnce(&SuggestionsCollectorTest::OnSuggestionsReturned,
                     base::Unretained(this)));

  EXPECT_EQ(suggestions_returned(), expected_results);
}

TEST_F(SuggestionsCollectorTest, ReturnsCombinedResultsIfAvailable) {
  FakeAssistiveSuggester assistive_suggester;
  auto suggestions_requestor = std::make_unique<FakeSuggestionsService>();

  suggestions_requestor->SetSuggestions({multi_word_result()});
  assistive_suggester.SetSuggestions(
      {personal_info_name_result(), personal_info_address_result()});

  SuggestionsCollector collector(&assistive_suggester,
                                 std::move(suggestions_requestor));

  auto expected_results = std::vector<AssistiveSuggestion>{
      personal_info_name_result(),
      personal_info_address_result(),
      multi_word_result(),
  };

  collector.GatherSuggestions(
      ime::mojom::SuggestionsRequest::New(),
      base::BindOnce(&SuggestionsCollectorTest::OnSuggestionsReturned,
                     base::Unretained(this)));

  EXPECT_EQ(suggestions_returned(), expected_results);
}

TEST_F(SuggestionsCollectorTest,
       OnlyReturnsAssistiveResultsIfRequestorNotAvailable) {
  FakeAssistiveSuggester assistive_suggester;
  auto suggestions_requestor = std::make_unique<FakeSuggestionsService>();

  suggestions_requestor->SetSuggestions({multi_word_result()});
  suggestions_requestor->SetIsAvailable(false);
  assistive_suggester.SetSuggestions(
      {personal_info_name_result(), personal_info_address_result()});

  SuggestionsCollector collector(&assistive_suggester,
                                 std::move(suggestions_requestor));

  auto expected_results = std::vector<AssistiveSuggestion>{
      personal_info_name_result(),
      personal_info_address_result(),
  };

  collector.GatherSuggestions(
      ime::mojom::SuggestionsRequest::New(),
      base::BindOnce(&SuggestionsCollectorTest::OnSuggestionsReturned,
                     base::Unretained(this)));

  EXPECT_EQ(suggestions_returned(), expected_results);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
