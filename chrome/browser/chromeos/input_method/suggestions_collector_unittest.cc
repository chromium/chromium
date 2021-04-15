// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_collector.h"

#include <vector>

#include "base/test/bind.h"
#include "chrome/browser/chromeos/input_method/suggestion_enums.h"
#include "chrome/browser/chromeos/input_method/suggestions.h"
#include "chrome/browser/chromeos/input_method/suggestions_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class FakeAssistiveSuggester : public SuggestionsSource {
 public:
  // SuggestionsSource overrides
  std::vector<TextSuggestion> GetSuggestions() override {
    return std::vector<TextSuggestion>{
        TextSuggestion{.mode = SuggestionMode::kCompletion,
                       .type = SuggestionType::kAssistivePersonalInfo,
                       .text = "my name is Mr Robot"},
        TextSuggestion{.mode = SuggestionMode::kCompletion,
                       .type = SuggestionType::kAssistivePersonalInfo,
                       .text = "my address is 123 Fake St"},
    };
  }
};

TEST(SuggestionsCollectorTest,
     ReturnsSuggestionsFromInjectedSuggestionsSource) {
  FakeAssistiveSuggester suggester;
  SuggestionsCollector collector(&suggester);
  SuggestionContext context = {.mode = SuggestionMode::kCompletion};

  std::vector<TextSuggestion> suggestions_returned;
  auto callback = base::BindLambdaForTesting(
      [&suggestions_returned](const std::vector<TextSuggestion>& suggestions) {
        suggestions_returned = suggestions;
      });

  std::vector<TextSuggestion> expected_suggestions = {
      TextSuggestion{.mode = SuggestionMode::kCompletion,
                     .type = SuggestionType::kAssistivePersonalInfo,
                     .text = "my name is Mr Robot"},
      TextSuggestion{.mode = SuggestionMode::kCompletion,
                     .type = SuggestionType::kAssistivePersonalInfo,
                     .text = "my address is 123 Fake St"},
  };

  collector.GatherSuggestions(context, callback);

  EXPECT_EQ(suggestions_returned, expected_suggestions);
}

}  // namespace
}  // namespace chromeos
