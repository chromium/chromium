// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/multi_word_suggester.h"

#include <vector>

#include "chrome/browser/chromeos/input_method/fake_suggestion_handler.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

using TextSuggestion = ::chromeos::ime::TextSuggestion;
using TextSuggestionMode = ::chromeos::ime::TextSuggestionMode;
using TextSuggestionType = ::chromeos::ime::TextSuggestionType;

void SendKeyEvent(MultiWordSuggester* suggester, const ui::DomCode& code) {
  suggester->HandleKeyEvent(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN,
                                         code, ui::EF_NONE, ui::DomKey::NONE,
                                         ui::EventTimeForNow()));
}

}  // namespace

TEST(MultiWordSuggesterTest, IgnoresIrrelevantExternalSuggestions) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);
  int focused_context_id = 5;

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kAssistivePersonalInfo,
                     .text = "my name is John Wayne"}};

  suggester.OnFocus(focused_context_id);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler.GetContextId(), focused_context_id);
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST(MultiWordSuggesterTest, IgnoresEmpyExternalSuggestions) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);
  int focused_context_id = 5;

  suggester.OnFocus(focused_context_id);
  suggester.OnExternalSuggestionsUpdated({});

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler.GetContextId(), focused_context_id);
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST(MultiWordSuggesterTest, DisplaysRelevantExternalSuggestions) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);
  int focused_context_id = 5;

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there!"}};

  suggester.OnFocus(focused_context_id);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetContextId(), focused_context_id);
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"hello there!");
}

TEST(MultiWordSuggesterTest, AcceptsSuggestionOnTabPress) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);
  int focused_context_id = 5;

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester.OnFocus(focused_context_id);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(&suggester, ui::DomCode::TAB);

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler.GetDismissedSuggestion());
  EXPECT_TRUE(suggestion_handler.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST(MultiWordSuggesterTest, DoesNotAcceptSuggestionOnNonTabKeypress) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);
  int focused_context_id = 5;

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester.OnFocus(focused_context_id);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(&suggester, ui::DomCode::ARROW_DOWN);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"hi there!");
}

}  // namespace chromeos
