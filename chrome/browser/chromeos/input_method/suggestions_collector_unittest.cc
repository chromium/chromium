// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_collector.h"

#include <vector>

#include "base/test/bind.h"
#include "chrome/browser/chromeos/input_method/suggestions_source.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using TextSuggestion = ::chromeos::ime::TextSuggestion;
using TextSuggestionMode = ::chromeos::ime::TextSuggestionMode;
using TextSuggestionType = ::chromeos::ime::TextSuggestionType;

class FakeAssistiveSuggester : public SuggestionsSource {
 public:
  // SuggestionsSource overrides
  std::vector<TextSuggestion> GetSuggestions() override {
    return std::vector<TextSuggestion>{
        TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                       .type = TextSuggestionType::kAssistivePersonalInfo,
                       .text = "my name is Mr Robot"},
        TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                       .type = TextSuggestionType::kAssistivePersonalInfo,
                       .text = "my address is 123 Fake St"},
    };
  }
};

TEST(SuggestionsCollectorTest,
     ReturnsSuggestionsFromInjectedSuggestionsSource) {
  FakeAssistiveSuggester suggester;
  SuggestionsCollector collector(&suggester);
  auto request = ime::mojom::SuggestionsRequest::New();

  std::vector<TextSuggestion> suggestions_returned;
  auto callback = base::BindLambdaForTesting(
      [&suggestions_returned](ime::mojom::SuggestionsResponsePtr response) {
        suggestions_returned = response->candidates;
      });

  std::vector<TextSuggestion> expected_suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kAssistivePersonalInfo,
                     .text = "my name is Mr Robot"},
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kAssistivePersonalInfo,
                     .text = "my address is 123 Fake St"},
  };

  collector.GatherSuggestions(std::move(request), callback);

  EXPECT_EQ(suggestions_returned, expected_suggestions);
}

}  // namespace
}  // namespace chromeos
