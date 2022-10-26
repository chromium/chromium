// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/multi_word_suggester.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;
using ime::AssistiveSuggestionType;

constexpr int kFocusedContextId = 5;

void SendKeyEvent(MultiWordSuggester* suggester, const ui::DomCode& code) {
  suggester->HandleKeyEvent(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN,
                                         code, ui::EF_NONE, ui::DomKey::NONE,
                                         ui::EventTimeForNow()));
}

void SetFirstAcceptTimeTo(Profile* profile, int days_ago) {
  ScopedDictPrefUpdate update(profile->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  base::TimeDelta since_epoch = base::Time::Now() - base::Time::UnixEpoch();
  update->Set("multi_word_first_accept",
              since_epoch.InDaysFloored() - days_ago);
}

absl::optional<int> GetFirstAcceptTime(Profile* profile) {
  ScopedDictPrefUpdate update(profile->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  return update->FindInt("multi_word_first_accept");
}

}  // namespace

class MultiWordSuggesterTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    suggester_ = std::make_unique<MultiWordSuggester>(&suggestion_handler_,
                                                      profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  FakeSuggestionHandler suggestion_handler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MultiWordSuggester> suggester_;
};

TEST_F(MultiWordSuggesterTest, IgnoresIrrelevantExternalSuggestions) {
  std::vector<AssistiveSuggestion> suggestions = {AssistiveSuggestion{
      .mode = AssistiveSuggestionMode::kPrediction,
      .type = AssistiveSuggestionType::kAssistivePersonalInfo,
      .text = "my name is John Wayne"}};

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler_.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"");
}

TEST_F(MultiWordSuggesterTest, IgnoresEmpyExternalSuggestions) {
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated({});

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler_.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"");
}

TEST_F(MultiWordSuggesterTest, DisplaysRelevantExternalSuggestions) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there!"}};

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hello there!");
}

TEST_F(MultiWordSuggesterTest,
       AfterBlurDoesNotDisplayRelevantExternalSuggestions) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there!"}};

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnBlur();
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler_.GetContextId(), kFocusedContextId);
}

TEST_F(MultiWordSuggesterTest, AcceptsSuggestionOnTabPress) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetDismissedSuggestion());
  EXPECT_TRUE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"");
}

TEST_F(MultiWordSuggesterTest, DoesNotAcceptSuggestionAfterBlur) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnBlur();
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  EXPECT_FALSE(suggestion_handler_.GetAcceptedSuggestion());
}

TEST_F(MultiWordSuggesterTest, DoesNotAcceptSuggestionOnNonTabKeypress) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hi there!");
}

TEST_F(MultiWordSuggesterTest, DoesNotAcceptSuggestionOnArrowDownKeypress) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hi there!");
}

TEST_F(MultiWordSuggesterTest, DoesNotAcceptSuggestionOnEnterKeypress) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ENTER);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hi there!");
}

TEST_F(MultiWordSuggesterTest, AcceptsSuggestionOnDownPlusEnterPress) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);
  SendKeyEvent(suggester_.get(), ui::DomCode::ENTER);

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetDismissedSuggestion());
  EXPECT_TRUE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"");
}

TEST_F(MultiWordSuggesterTest, DoesNotHighlightAfterBlur) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnBlur();
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);

  EXPECT_FALSE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, HighlightsSuggestionOnDownArrow) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);

  EXPECT_TRUE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, MaintainsHighlightOnMultipleDownArrow) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);

  EXPECT_TRUE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, RemovesHighlightOnDownThenUpArrow) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);

  EXPECT_FALSE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, HighlightIsNotShownWithUpArrow) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);

  EXPECT_FALSE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, HighlightIsNotShownWithMultipleUpArrow) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);

  EXPECT_FALSE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, DisplaysTabGuideline) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  auto suggestion_details = suggestion_handler_.GetLastSuggestionDetails();
  EXPECT_TRUE(suggestion_details.show_quick_accept_annotation);
}

TEST_F(MultiWordSuggesterTest,
       DisplaysTabGuidelineWithinSevenDaysOfFirstAccept) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  SetFirstAcceptTimeTo(profile_.get(), /*days_ago=*/6);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  auto suggestion_details = suggestion_handler_.GetLastSuggestionDetails();
  EXPECT_TRUE(suggestion_details.show_quick_accept_annotation);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotDisplayTabGuidelineSevenDaysAfterFirstAccept) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  SetFirstAcceptTimeTo(profile_.get(), /*days_ago=*/7);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  auto suggestion_details = suggestion_handler_.GetLastSuggestionDetails();
  EXPECT_FALSE(suggestion_details.show_quick_accept_annotation);
}

TEST_F(MultiWordSuggesterTest, SetsAcceptTimeOnFirstSuggestionAcceptedOnly) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hi there!"},
  };

  auto pref_before_accept = GetFirstAcceptTime(profile_.get());
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);
  auto pref_after_first_accept = GetFirstAcceptTime(profile_.get());

  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);
  auto pref_after_second_accept = GetFirstAcceptTime(profile_.get());

  EXPECT_EQ(pref_before_accept, absl::nullopt);
  ASSERT_TRUE(pref_after_first_accept.has_value());
  ASSERT_TRUE(pref_after_second_accept.has_value());
  EXPECT_EQ(*pref_after_first_accept, *pref_after_second_accept);
}

TEST_F(MultiWordSuggesterTest, CalculatesConfirmedLengthForOneWord) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you going"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"ho", /*cursor_pos=*/2,
                                       /*anchor_pos=*/2);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"how are you going");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 2u);  // ho
}

TEST_F(MultiWordSuggesterTest, CalculatesConfirmedLengthForManyWords) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "where are you going"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"hey there sam whe",
                                       /*cursor_pos=*/17, /*anchor_pos=*/17);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"where are you going");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 3u);  // whe
}

TEST_F(MultiWordSuggesterTest, CalculatesConfirmedLengthGreedily) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hohohohoho"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"merry christmas hohoho",
                                       /*cursor_pos=*/22,
                                       /*anchor_pos=*/22);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hohohohoho");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 6u);  // hohoho
}

TEST_F(MultiWordSuggesterTest, CalculatesConfirmedLengthForPredictions) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "is the next task"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"this ",
                                       /*cursor_pos=*/5, /*anchor_pos=*/5);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"is the next task");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 0u);
}

TEST_F(MultiWordSuggesterTest, HandlesNewlinesWhenCalculatingConfirmedLength) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"\nh",
                                       /*cursor_pos=*/2, /*anchor_pos=*/2);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"how are you");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 1u);  // h
}

TEST_F(MultiWordSuggesterTest, HandlesMultipleRepeatingCharsWhenTracking) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", /*cursor_pos=*/1,
                                       /*anchor_pos=*/1);
  suggester_->TrySuggestWithSurroundingText(u"h", /*cursor_pos=*/1,
                                            /*anchor_pos=*/1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"hh", /*cursor_pos=*/2,
                                       /*anchor_pos=*/2);

  EXPECT_FALSE(suggester_->TrySuggestWithSurroundingText(
      u"hh", /*cursor_pos=*/2, /*anchor_pos=*/2));
}

TEST_F(MultiWordSuggesterTest, DoesNotDismissOnMultipleCursorMoveToEndOfText) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"hello h", /*cursor_pos=*/7,
                                       /*anchor_pos=*/7);
  suggester_->TrySuggestWithSurroundingText(u"hello h", /*cursor_pos=*/7,
                                            /*anchor_pos=*/7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"hello h", /*cursor_pos=*/7,
                                       /*anchor_pos=*/7);
  suggester_->TrySuggestWithSurroundingText(u"hello h", /*cursor_pos=*/7,
                                            /*anchor_pos=*/7);
  suggester_->OnSurroundingTextChanged(u"hello h", /*cursor_pos=*/7,
                                       /*anchor_pos=*/7);

  EXPECT_TRUE(suggester_->TrySuggestWithSurroundingText(u"hello h",
                                                        /*cursor_pos=*/7,
                                                        /*anchor_pos=*/7));
}

TEST_F(MultiWordSuggesterTest, TracksLastSuggestionOnSurroundingTextChange) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "where are you going"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"hey there sam whe", 17, 17);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"hey there sam wher", 18, 18);
  suggester_->TrySuggestWithSurroundingText(u"hey there sam wher", 18, 18);
  suggester_->OnSurroundingTextChanged(u"hey there sam where", 19, 19);
  suggester_->TrySuggestWithSurroundingText(u"hey there sam where", 19, 19);
  suggester_->OnSurroundingTextChanged(u"hey there sam where ", 20, 20);
  suggester_->TrySuggestWithSurroundingText(u"hey there sam where ", 20, 20);
  suggester_->OnSurroundingTextChanged(u"hey there sam where a", 21, 21);
  suggester_->TrySuggestWithSurroundingText(u"hey there sam where a", 21, 21);
  suggester_->OnSurroundingTextChanged(u"hey there sam where ar", 22, 22);
  suggester_->TrySuggestWithSurroundingText(u"hey there sam where ar", 22, 22);
  suggester_->OnSurroundingTextChanged(u"hey there sam where are", 23, 23);
  suggester_->TrySuggestWithSurroundingText(u"hey there sam where are", 23, 23);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"where are you going");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 9u);  // where are
}

TEST_F(MultiWordSuggesterTest,
       TracksLastSuggestionOnSurroundingTextChangeAtBeginningText) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"ho", 2, 2);
  suggester_->TrySuggestWithSurroundingText(u"ho", 2, 2);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->TrySuggestWithSurroundingText(u"how", 3, 3);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"how are you");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 3u);  // how
}

TEST_F(MultiWordSuggesterTest,
       TracksLastSuggestionOnLargeSurroundingTextChange) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->TrySuggestWithSurroundingText(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are yo", 10, 10);
  suggester_->TrySuggestWithSurroundingText(u"how are yo", 10, 10);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"how are you");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 10u);  // how are yo
}

TEST_F(MultiWordSuggesterTest, MaintainsPredictionSuggestionModeWhenTracking) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"ho", 2, 2);
  suggester_->TrySuggestWithSurroundingText(u"ho", 2, 2);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->TrySuggestWithSurroundingText(u"how", 3, 3);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->TrySuggestWithSurroundingText(u"how ", 4, 4);

  EXPECT_EQ(suggester_->GetSuggestions()[0].mode,
            AssistiveSuggestionMode::kPrediction);
}

TEST_F(MultiWordSuggesterTest, MaintainsCompletionSuggestionModeWhenTracking) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"ho", 2, 2);
  suggester_->TrySuggestWithSurroundingText(u"ho", 2, 2);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->TrySuggestWithSurroundingText(u"how", 3, 3);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->TrySuggestWithSurroundingText(u"how ", 4, 4);

  EXPECT_EQ(suggester_->GetSuggestions()[0].mode,
            AssistiveSuggestionMode::kCompletion);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotTrackLastSuggestionIfSurroundingTextChange) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->TrySuggestWithSurroundingText(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how yo", 6, 6);

  // The consumer will handle dismissing the suggestion
  EXPECT_FALSE(suggester_->TrySuggestWithSurroundingText(u"how yo", 6, 6));
}

TEST_F(MultiWordSuggesterTest,
       DoesNotTrackLastSuggestionIfCursorBeforeSuggestionStartPos) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = " for the example"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"this is some text", 17, 17);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"this is some text ", 18, 18);
  suggester_->TrySuggestWithSurroundingText(u"this is some text ", 18, 18);
  suggester_->OnSurroundingTextChanged(u"this is some text f", 19, 19);
  suggester_->TrySuggestWithSurroundingText(u"this is some text f", 19, 19);
  suggester_->OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  suggester_->TrySuggestWithSurroundingText(u"this is some text fo", 20, 20);
  suggester_->OnSurroundingTextChanged(u"this is some text f", 19, 19);
  suggester_->TrySuggestWithSurroundingText(u"this is some text f", 19, 19);
  suggester_->OnSurroundingTextChanged(u"this is some text ", 18, 18);
  suggester_->TrySuggestWithSurroundingText(u"this is some text ", 18, 18);
  suggester_->OnSurroundingTextChanged(u"this is some text", 17, 17);

  EXPECT_FALSE(
      suggester_->TrySuggestWithSurroundingText(u"this is some text", 17, 17));
}

TEST_F(MultiWordSuggesterTest, DoesNotTrackSuggestionPastSuggestionPoint) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = " for the example"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"this is some text for", 21, 21);
  suggester_->TrySuggestWithSurroundingText(u"this is some text for", 21, 21);
  suggester_->OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  bool at_suggestion_point = suggester_->TrySuggestWithSurroundingText(
      u"this is some text fo", 20, 20);
  suggester_->OnSurroundingTextChanged(u"this is some text f", 19, 19);
  bool before_suggestion_point =
      suggester_->TrySuggestWithSurroundingText(u"this is some text f", 19, 19);

  EXPECT_TRUE(at_suggestion_point);
  EXPECT_FALSE(before_suggestion_point);
}

TEST_F(MultiWordSuggesterTest,
       DismissesSuggestionAfterCursorMoveFromEndOfText) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = " for the example"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"this is some text for", 21, 21);
  suggester_->TrySuggestWithSurroundingText(u"this is some text for", 21, 21);
  suggester_->OnSurroundingTextChanged(u"this is some text for", 15, 15);

  EXPECT_FALSE(suggester_->TrySuggestWithSurroundingText(
      u"this is some text for", 15, 15));
}

TEST_F(MultiWordSuggesterTest, DismissesSuggestionOnUserTypingFullSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = " are"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->TrySuggestWithSurroundingText(u"how ", 4, 4);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->TrySuggestWithSurroundingText(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->TrySuggestWithSurroundingText(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);

  EXPECT_FALSE(suggester_->TrySuggestWithSurroundingText(u"how are", 7, 7));
}

TEST_F(MultiWordSuggesterTest, ReturnsGenericActionIfNoSuggestionHasBeenShown) {
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"hey there sam whe", 17, 17);

  EXPECT_EQ(suggester_->GetProposeActionType(), AssistiveType::kGenericAction);
}

TEST_F(MultiWordSuggesterTest,
       ReturnsCompletionActionIfCompletionSuggestionShown) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_EQ(suggester_->GetProposeActionType(),
            AssistiveType::kMultiWordCompletion);
}

TEST_F(MultiWordSuggesterTest,
       ReturnsPredictionActionIfPredictionSuggestionShown) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_EQ(suggester_->GetProposeActionType(),
            AssistiveType::kMultiWordPrediction);
}

TEST_F(MultiWordSuggesterTest,
       ReturnsCompletionActionAfterAcceptingCompletionSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why ar", 6, 6);
  suggester_->TrySuggestWithSurroundingText(u"why", 6, 6);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  ASSERT_EQ(suggester_->GetProposeActionType(),
            AssistiveType::kMultiWordCompletion);
}

TEST_F(MultiWordSuggesterTest,
       ReturnsPredictionActionAfterAcceptingPredictionSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why", 3, 3);
  suggester_->TrySuggestWithSurroundingText(u"why", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  ASSERT_EQ(suggester_->GetProposeActionType(),
            AssistiveType::kMultiWordPrediction);
}

TEST_F(MultiWordSuggesterTest, RecordsTimeToAcceptMetric) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.MultiWord", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  EXPECT_TRUE(suggestion_handler_.GetAcceptedSuggestion());
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.MultiWord", 1);
}

TEST_F(MultiWordSuggesterTest, RecordsTimeToDismissMetric) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.MultiWord", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->DismissSuggestion();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.MultiWord", 1);
}

TEST_F(MultiWordSuggesterTest, RecordsSuggestionLengthMetric) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.SuggestionLength", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.SuggestionLength", 1);
  // "how are you" = 11 chars
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.SuggestionLength", /*sample=*/11,
      /*expected_bucket_count=*/1);
}

TEST_F(MultiWordSuggesterTest, DoesntRecordIfSuggestionLengthIsBig) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = std::string(101, 'h')},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.SuggestionLength", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.SuggestionLength", 0);
}

TEST_F(MultiWordSuggesterTest, RecordsCouldPossiblyShowCompletionSuggestion) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);

  // For a completion suggestion to show, we need to have the cursor at the end
  // of the text.
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion",
      /*sample=*/MultiWordSuggestionType::kCompletion,
      /*expected_bucket_count=*/1);
}

TEST_F(MultiWordSuggesterTest, RecordsCouldPossiblyShowPredictionSuggestion) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);

  // For a prediction suggestion to show, we need to have a whitespace char at
  // the end of the text, and the cursor must be at the end of the text.
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion",
      /*sample=*/MultiWordSuggestionType::kPrediction,
      /*expected_bucket_count=*/1);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotRecordCouldPossiblyShowSuggestionWhenCursorNotAtEndOfText) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how are you today", 4, 4);
  suggester_->OnSurroundingTextChanged(u"how are you today", 3, 3);
  suggester_->OnSurroundingTextChanged(u"how are you today", 0, 0);
  suggester_->OnSurroundingTextChanged(u"how are you today", 16, 16);
  suggester_->OnSurroundingTextChanged(u"how are you today", 10, 10);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotRecordCouldPossiblyShowSuggestionWhenThereIsASelection) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how are you today", 4, 17);
  suggester_->OnSurroundingTextChanged(u"how are you today", 0, 17);
  suggester_->OnSurroundingTextChanged(u"how are you today", 16, 17);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotRecordCouldPossiblyShowSuggestionWhenTextLengthIsSmall) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnSurroundingTextChanged(u"ho", 2, 2);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);
}

TEST_F(MultiWordSuggesterTest,
       RecordsCouldPossiblyShowSuggestionForMultipleConsecutiveTextUpdates) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"how are ", 8, 8);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 5);
  histogram_tester.ExpectBucketCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion",
      /*sample=*/MultiWordSuggestionType::kPrediction,
      /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion",
      /*sample=*/MultiWordSuggestionType::kCompletion,
      /*expected_count=*/3);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotRecordCouldPossiblyShowSuggestionWhenSuggestionIsShowing) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"how are ", 8, 8);

  // Only one metric should be recorded, when we receive a surrounding text
  // event prior to the suggestions being generated and shown to the user. Each
  // subsequent surrounding text event should NOT record the metric.
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion",
      /*sample=*/MultiWordSuggestionType::kPrediction,
      /*expected_bucket_count=*/1);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotRecordImplicitAcceptanceWhenAUserPartiallyTypesWord) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"how are ", 8, 8);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotRecordImplicitAcceptanceWhenSuggestionDismissed) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"how are ", 8, 8);
  // Dismisses suggestion as text no longer matches
  suggester_->OnSurroundingTextChanged(u"how are t", 9, 9);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotRecordImplicitAcceptanceWhenSuggestionAccepted) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);
}

TEST_F(MultiWordSuggesterTest,
       RecordsImplicitAcceptanceOnceWhenPredictionSuggestionTypedFully) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"how are ", 8, 8);
  suggester_->OnSurroundingTextChanged(u"how are y", 9, 9);
  suggester_->OnSurroundingTextChanged(u"how are yo", 10, 10);
  suggester_->OnSurroundingTextChanged(u"how are you", 11, 11);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance",
      /*sample=*/MultiWordSuggestionType::kPrediction,
      /*expected_bucket_count=*/1);
}

TEST_F(MultiWordSuggesterTest,
       RecordsImplicitAcceptanceOnceWhenCompletionSuggestionTypedFully) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"ho", 2, 2);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"how are ", 8, 8);
  suggester_->OnSurroundingTextChanged(u"how are y", 9, 9);
  suggester_->OnSurroundingTextChanged(u"how are yo", 10, 10);
  suggester_->OnSurroundingTextChanged(u"how are you", 11, 11);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance",
      /*sample=*/MultiWordSuggestionType::kCompletion,
      /*expected_bucket_count=*/1);
}

TEST_F(MultiWordSuggesterTest,
       RecordsImplicitAcceptanceOnceWhenTypingMoreThenSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"how are ", 8, 8);
  suggester_->OnSurroundingTextChanged(u"how are y", 9, 9);
  suggester_->OnSurroundingTextChanged(u"how are yo", 10, 10);
  // Metric should be recorded after the following surrounding text
  suggester_->OnSurroundingTextChanged(u"how are you", 11, 11);
  suggester_->OnSurroundingTextChanged(u"how are you ", 12, 12);
  suggester_->OnSurroundingTextChanged(u"how are you g", 13, 13);
  suggester_->OnSurroundingTextChanged(u"how are you go", 14, 14);
  suggester_->OnSurroundingTextChanged(u"how are you goi", 15, 15);
  suggester_->OnSurroundingTextChanged(u"how are you goin", 16, 16);
  suggester_->OnSurroundingTextChanged(u"how are you going", 17, 17);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance",
      /*sample=*/MultiWordSuggestionType::kPrediction,
      /*expected_bucket_count=*/1);
}

TEST_F(MultiWordSuggesterTest,
       SurroundingTextChangesDoNotTriggerAnnouncements) {
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->TrySuggestWithSurroundingText(u"why are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester_->TrySuggestWithSurroundingText(u"why aren", 8, 8);
  suggester_->OnSurroundingTextChanged(u"why aren'", 9, 9);
  suggester_->TrySuggestWithSurroundingText(u"why aren'", 9, 9);
  suggester_->OnSurroundingTextChanged(u"why aren't", 10, 10);
  suggester_->TrySuggestWithSurroundingText(u"why aren't", 10, 10);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 0u);
}

TEST_F(MultiWordSuggesterTest, ShowingSuggestionsTriggersAnnouncement) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->TrySuggestWithSurroundingText(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 1u);
  EXPECT_EQ(suggestion_handler_.GetAnnouncements().back(),
            u"predictive writing candidate shown, press down to select or "
            u"press tab to accept");
}

TEST_F(MultiWordSuggesterTest,
       TrackingSuggestionsTriggersAnnouncementOnlyOnce) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->TrySuggestWithSurroundingText(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester_->TrySuggestWithSurroundingText(u"why aren", 8, 8);
  suggester_->OnSurroundingTextChanged(u"why aren'", 9, 9);
  suggester_->TrySuggestWithSurroundingText(u"why aren'", 9, 9);
  suggester_->OnSurroundingTextChanged(u"why aren't", 10, 10);
  suggester_->TrySuggestWithSurroundingText(u"why aren't", 10, 10);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 1u);
  EXPECT_EQ(suggestion_handler_.GetAnnouncements().back(),
            u"predictive writing candidate shown, press down to select or "
            u"press tab to accept");
}

TEST_F(MultiWordSuggesterTest, AcceptingSuggestionTriggersAnnouncement) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->TrySuggestWithSurroundingText(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 2u);
  EXPECT_EQ(suggestion_handler_.GetAnnouncements().back(),
            u"predictive writing candidate inserted");
}

TEST_F(MultiWordSuggesterTest,
       TransitionsFromAcceptSuggestionToNoSuggestionDoesNotTriggerAnnounce) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->TrySuggestWithSurroundingText(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);
  suggester_->OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester_->TrySuggestWithSurroundingText(u"why aren", 8, 8);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 2u);
}

TEST_F(MultiWordSuggesterTest, DismissingSuggestionTriggersAnnouncement) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->TrySuggestWithSurroundingText(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->DismissSuggestion();

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 2u);
  EXPECT_EQ(suggestion_handler_.GetAnnouncements().back(),
            u"predictive writing candidate dismissed");
}

TEST_F(MultiWordSuggesterTest,
       TransitionsFromDismissSuggestionToNoSuggestionDoesNotTriggerAnnounce) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->TrySuggestWithSurroundingText(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->DismissSuggestion();
  suggester_->OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester_->TrySuggestWithSurroundingText(u"why aren", 8, 8);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 2u);
}

}  // namespace input_method
}  // namespace ash
