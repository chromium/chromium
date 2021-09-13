// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/multi_word_suggester.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

constexpr int kFocusedContextId = 5;

void SendKeyEvent(MultiWordSuggester* suggester, const ui::DomCode& code) {
  suggester->HandleKeyEvent(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN,
                                         code, ui::EF_NONE, ui::DomKey::NONE,
                                         ui::EventTimeForNow()));
}

}  // namespace

TEST(MultiWordSuggesterTest, IgnoresIrrelevantExternalSuggestions) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kAssistivePersonalInfo,
                     .text = "my name is John Wayne"}};

  suggester.OnFocus(kFocusedContextId);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST(MultiWordSuggesterTest, IgnoresEmpyExternalSuggestions) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  suggester.OnFocus(kFocusedContextId);
  suggester.OnExternalSuggestionsUpdated({});

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST(MultiWordSuggesterTest, DisplaysRelevantExternalSuggestions) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there!"}};

  suggester.OnFocus(kFocusedContextId);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"hello there!");
}

TEST(MultiWordSuggesterTest, AcceptsSuggestionOnTabPress) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester.OnFocus(kFocusedContextId);
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

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(&suggester, ui::DomCode::ARROW_DOWN);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"hi there!");
}

TEST(MultiWordSuggesterTest, CalculatesConfirmedLengthForOneWord) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you going"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"ho", /*cursor_pos=*/2, /*anchor_pos=*/2);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"how are you going");
  EXPECT_EQ(suggestion_handler.GetConfirmedLength(), 2);  // ho
}

TEST(MultiWordSuggesterTest, CalculatesConfirmedLengthForManyWords) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "where are you going"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"hey there sam whe",
                                     /*cursor_pos=*/17, /*anchor_pos=*/17);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"where are you going");
  EXPECT_EQ(suggestion_handler.GetConfirmedLength(), 3);  // whe
}

TEST(MultiWordSuggesterTest, TracksLastSuggestionOnSurroundingTextChange) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "where are you going"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"hey there sam whe", 17, 17);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.OnSurroundingTextChanged(u"hey there sam wher", 18, 18);
  suggester.Suggest(u"hey there sam wher", 18, 18);
  suggester.OnSurroundingTextChanged(u"hey there sam where", 19, 19);
  suggester.Suggest(u"hey there sam where", 19, 19);
  suggester.OnSurroundingTextChanged(u"hey there sam where ", 20, 20);
  suggester.Suggest(u"hey there sam where ", 20, 20);
  suggester.OnSurroundingTextChanged(u"hey there sam where a", 21, 21);
  suggester.Suggest(u"hey there sam where a", 21, 21);
  suggester.OnSurroundingTextChanged(u"hey there sam where ar", 22, 22);
  suggester.Suggest(u"hey there sam where ar", 22, 22);
  suggester.OnSurroundingTextChanged(u"hey there sam where are", 23, 23);
  suggester.Suggest(u"hey there sam where are", 23, 23);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"where are you going");
  EXPECT_EQ(suggestion_handler.GetConfirmedLength(), 9);  // where are
}

TEST(MultiWordSuggesterTest,
     TracksLastSuggestionOnSurroundingTextChangeAtBeginningText) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"h", 1, 1);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.OnSurroundingTextChanged(u"ho", 2, 2);
  suggester.Suggest(u"ho", 2, 2);
  suggester.OnSurroundingTextChanged(u"how", 3, 3);
  suggester.Suggest(u"how", 3, 3);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"how are you");
  EXPECT_EQ(suggestion_handler.GetConfirmedLength(), 3);  // how
}

TEST(MultiWordSuggesterTest, TracksLastSuggestionOnLargeSurroundingTextChange) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"h", 1, 1);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester.Suggest(u"how ar", 6, 6);
  suggester.OnSurroundingTextChanged(u"how are yo", 10, 10);
  suggester.Suggest(u"how are yo", 10, 10);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"how are you");
  EXPECT_EQ(suggestion_handler.GetConfirmedLength(), 10);  // how are yo
}

TEST(MultiWordSuggesterTest,
     DoesNotTrackLastSuggestionIfSurroundingTextChange) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"h", 1, 1);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester.Suggest(u"how ar", 6, 6);
  suggester.OnSurroundingTextChanged(u"how yo", 6, 6);

  // The consumer will handle dismissing the suggestion
  EXPECT_FALSE(suggester.Suggest(u"how yo", 6, 6));
}

TEST(MultiWordSuggesterTest,
     DoesNotTrackLastSuggestionIfCursorBeforeSuggestionStartPos) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = " for the example"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"this is some text", 17, 17);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  suggester.OnSurroundingTextChanged(u"this is some text ", 18, 18);
  suggester.Suggest(u"this is some text ", 18, 18);
  suggester.OnSurroundingTextChanged(u"this is some text f", 19, 19);
  suggester.Suggest(u"this is some text f", 19, 19);
  suggester.OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  suggester.Suggest(u"this is some text fo", 20, 20);
  suggester.OnSurroundingTextChanged(u"this is some text f", 19, 19);
  suggester.Suggest(u"this is some text f", 19, 19);
  suggester.OnSurroundingTextChanged(u"this is some text ", 18, 18);
  suggester.Suggest(u"this is some text ", 18, 18);
  suggester.OnSurroundingTextChanged(u"this is some text", 17, 17);

  EXPECT_TRUE(suggester.Suggest(u"this is some text", 17, 17));
  EXPECT_FALSE(suggester.Suggest(u"this is some tex", 16, 16));
}

TEST(MultiWordSuggesterTest, ReturnsGenericActionIfNoSuggestionHasBeenShown) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"hey there sam whe", 17, 17);

  EXPECT_EQ(suggester.GetProposeActionType(), AssistiveType::kGenericAction);
}

TEST(MultiWordSuggesterTest,
     ReturnsCompletionActionIfCompletionSuggestionShown) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  EXPECT_EQ(suggester.GetProposeActionType(),
            AssistiveType::kMultiWordCompletion);
}

TEST(MultiWordSuggesterTest,
     ReturnsPredictionActionIfPredictionSuggestionShown) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"how", 3, 3);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  EXPECT_EQ(suggester.GetProposeActionType(),
            AssistiveType::kMultiWordPrediction);
}

TEST(MultiWordSuggesterTest,
     ReturnsCompletionActionAfterAcceptingCompletionSuggestion) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why ar", 6, 6);
  suggester.Suggest(u"why", 6, 6);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(&suggester, ui::DomCode::TAB);

  ASSERT_EQ(suggester.GetProposeActionType(),
            AssistiveType::kMultiWordCompletion);
}

TEST(MultiWordSuggesterTest,
     ReturnsPredictionActionAfterAcceptingPredictionSuggestion) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why", 3, 3);
  suggester.Suggest(u"why", 3, 3);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(&suggester, ui::DomCode::TAB);

  ASSERT_EQ(suggester.GetProposeActionType(),
            AssistiveType::kMultiWordPrediction);
}

TEST(MultiWordSuggesterTest, RecordsTimeToAcceptMetric) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.MultiWord", 0);

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"how", 3, 3);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(&suggester, ui::DomCode::TAB);

  EXPECT_TRUE(suggestion_handler.GetAcceptedSuggestion());
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.MultiWord", 1);
}

TEST(MultiWordSuggesterTest, RecordsTimeToDismissMetric) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.MultiWord", 0);

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"how", 3, 3);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.DismissSuggestion();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.MultiWord", 1);
}

TEST(MultiWordSuggesterTest, RecordsDismissedAccuracyMetric) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.DismissedAccuracy.MultiWord", 43, 0);

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"how ", 4, 4);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.Suggest(u"how a", 5, 5);
  suggester.Suggest(u"how ar", 6, 6);
  suggester.Suggest(u"how are", 7, 7);
  suggester.Suggest(u"how aren", 8, 8);
  suggester.DismissSuggestion();

  // Correctly predicted "are" which is three of the seven chars in "are you",
  // thus accuracy is 3/7 ~= 0.428 which comes to a rounded percentage int val
  // of 43.
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.DismissedAccuracy.MultiWord", 43, 1);
}

TEST(MultiWordSuggesterTest, RecordsZeroValuedDismissedAccuracy) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.DismissedAccuracy.MultiWord", 0, 0);

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"how ", 4, 4);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.Suggest(u"how d", 5, 5);
  suggester.DismissSuggestion();

  // Zero predicted chars
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.DismissedAccuracy.MultiWord", 0, 1);
}

TEST(MultiWordSuggesterTest, RecordsCompletionCandidateDismissedAccuracy) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.DismissedAccuracy.MultiWord", 63, 0);

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why ar", 6, 6);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.Suggest(u"why are", 7, 7);
  suggester.Suggest(u"why aren", 8, 8);
  suggester.Suggest(u"why aren\'", 9, 9);
  suggester.Suggest(u"why aren\'t", 10, 10);
  suggester.Suggest(u"why aren\'t ", 11, 11);
  suggester.Suggest(u"why aren\'t w", 12, 12);
  suggester.DismissSuggestion();

  // Predicted the cars "en\'t " which is 5 of the 8 chars in "en\'t you",
  // thus accuracy is 5/8 or approx 62 percent
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.DismissedAccuracy.MultiWord", 63, 1);
}

TEST(MultiWordSuggesterTest, SurroundingTextChangesDoNotTriggerAnnouncements) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why are", 7, 7);
  suggester.Suggest(u"why are", 7, 7);
  suggester.OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester.Suggest(u"why aren", 8, 8);
  suggester.OnSurroundingTextChanged(u"why aren'", 9, 9);
  suggester.Suggest(u"why aren'", 9, 9);
  suggester.OnSurroundingTextChanged(u"why aren't", 10, 10);
  suggester.Suggest(u"why aren't", 10, 10);

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 0);
}

TEST(MultiWordSuggesterTest, ShowingSuggestionsTriggersAnnouncement) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why are", 7, 7);
  suggester.Suggest(u"why are", 7, 7);
  suggester.OnExternalSuggestionsUpdated(suggestions);

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 1);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().back(),
            u"predictive writing candidate shown, press tab to accept");
}

TEST(MultiWordSuggesterTest, TrackingSuggestionsTriggersAnnouncementOnlyOnce) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why are", 7, 7);
  suggester.Suggest(u"why are", 7, 7);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester.Suggest(u"why aren", 8, 8);
  suggester.OnSurroundingTextChanged(u"why aren'", 9, 9);
  suggester.Suggest(u"why aren'", 9, 9);
  suggester.OnSurroundingTextChanged(u"why aren't", 10, 10);
  suggester.Suggest(u"why aren't", 10, 10);

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 1);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().back(),
            u"predictive writing candidate shown, press tab to accept");
}

TEST(MultiWordSuggesterTest, AcceptingSuggestionTriggersAnnouncement) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why are", 7, 7);
  suggester.Suggest(u"why are", 7, 7);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(&suggester, ui::DomCode::TAB);

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 2);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().back(),
            u"predictive writing candidate inserted");
}

TEST(MultiWordSuggesterTest,
     TransitionsFromAcceptSuggestionToNoSuggestionDoesNotTriggerAnnounce) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why are", 7, 7);
  suggester.Suggest(u"why are", 7, 7);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(&suggester, ui::DomCode::TAB);
  suggester.OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester.Suggest(u"why aren", 8, 8);

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 2);
}

TEST(MultiWordSuggesterTest, DismissingSuggestionTriggersAnnouncement) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why are", 7, 7);
  suggester.Suggest(u"why are", 7, 7);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.DismissSuggestion();

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 2);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().back(),
            u"predictive writing candidate dismissed");
}

TEST(MultiWordSuggesterTest,
     TransitionsFromDismissSuggestionToNoSuggestionDoesNotTriggerAnnounce) {
  FakeSuggestionHandler suggestion_handler;
  MultiWordSuggester suggester(&suggestion_handler);

  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester.OnFocus(kFocusedContextId);
  suggester.OnSurroundingTextChanged(u"why are", 7, 7);
  suggester.Suggest(u"why are", 7, 7);
  suggester.OnExternalSuggestionsUpdated(suggestions);
  suggester.DismissSuggestion();
  suggester.OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester.Suggest(u"why aren", 8, 8);

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 2);
}

}  // namespace input_method
}  // namespace ash
