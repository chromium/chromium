// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/emoji_suggester.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

const char kEmojiData[] = "happy,😀;😃;😄";

class TestSuggestionHandler : public SuggestionHandlerInterface {
 public:
  bool SetButtonHighlighted(int context_id,
                            const ui::ime::AssistiveWindowButton& button,
                            bool highlighted,
                            std::string* error) override {
    switch (button.id) {
      case ui::ime::ButtonId::kLearnMore:
        learn_more_button_highlighted_ = highlighted;
        return true;
      case ui::ime::ButtonId::kSuggestion:
        // If highlighted, needs to unhighlight previously highlighted button.
        if (currently_highlighted_index_ != INT_MAX && highlighted) {
          candidate_highlighted_[currently_highlighted_index_] = 0;
        }
        currently_highlighted_index_ = highlighted ? button.index : INT_MAX;
        candidate_highlighted_[button.index] = highlighted ? 1 : 0;
        return true;
      default:
        return false;
    }
  }

  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override {
    candidate_highlighted_.clear();
    for (size_t i = 0; i < assistive_window.candidates.size(); i++) {
      candidate_highlighted_.push_back(0);
    }
    show_indices_ = assistive_window.show_indices;
    show_setting_link_ = assistive_window.show_setting_link;
    return true;
  }

  void VerifyShowIndices(bool show_indices) {
    EXPECT_EQ(show_indices_, show_indices);
  }

  void VerifyLearnMoreButtonHighlighted(const bool highlighted) {
    EXPECT_EQ(learn_more_button_highlighted_, highlighted);
  }

  void VerifyCandidateHighlighted(const int index, const bool highlighted) {
    int expect = highlighted ? 1 : 0;
    EXPECT_EQ(candidate_highlighted_[index], expect);
  }

  void VerifyShowSettingLink(const bool show_setting_link) {
    EXPECT_EQ(show_setting_link_, show_setting_link);
  }

  bool DismissSuggestion(int context_id, std::string* error) override {
    return false;
  }

  bool AcceptSuggestion(int context_id, std::string* error) override {
    return false;
  }

  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}

  bool ShowMultipleSuggestions(int context_id,
                               const std::vector<base::string16>& candidates,
                               std::string* error) override {
    return false;
  }

  void ClickButton(const ui::ime::AssistiveWindowButton& button) override {}

  bool AcceptSuggestionCandidate(int context_id,
                                 const base::string16& candidate,
                                 std::string* error) override {
    return false;
  }

  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override {
    return false;
  }

 private:
  bool show_indices_ = false;
  bool show_setting_link_ = false;
  bool learn_more_button_highlighted_ = false;
  std::vector<int> candidate_highlighted_;
  size_t currently_highlighted_index_ = INT_MAX;
};

class EmojiSuggesterTest : public testing::Test {
 protected:
  void SetUp() override {
    engine_ = std::make_unique<TestSuggestionHandler>();
    profile_ = std::make_unique<TestingProfile>();
    emoji_suggester_ =
        std::make_unique<EmojiSuggester>(engine_.get(), profile_.get());
    emoji_suggester_->LoadEmojiMapForTesting(kEmojiData);
    chrome_keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
    chrome_keyboard_controller_client_->set_keyboard_visible_for_test(false);
  }

  SuggestionStatus Press(std::string event_key) {
    InputMethodEngineBase::KeyboardEvent event;
    event.key = event_key;
    return emoji_suggester_->HandleKeyEvent(event);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmojiSuggester> emoji_suggester_;
  std::unique_ptr<TestSuggestionHandler> engine_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
};

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpace) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
}

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpaceAndIsUppercase) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("HAPPY ")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenStringEndsWithNewLine) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy\n")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenStringDoesNotEndWithSpace) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenWordNotInMap) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("hapy ")));
}

TEST_F(EmojiSuggesterTest, DoNotShowSuggestionWhenVirtualKeyboardEnabled) {
  chrome_keyboard_controller_client_->set_keyboard_visible_for_test(true);
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  EXPECT_FALSE(emoji_suggester_->GetSuggestionShownForTesting());
}

TEST_F(EmojiSuggesterTest, ReturnkBrowsingWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event;
  event.key = "Down";
  EXPECT_EQ(SuggestionStatus::kBrowsing,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkBrowsingWhenPressingUp) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event;
  event.key = "Up";
  EXPECT_EQ(SuggestionStatus::kBrowsing,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkDismissWhenPressingEsc) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event;
  event.key = "Esc";
  EXPECT_EQ(SuggestionStatus::kDismiss,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenValidNumber) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Down";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "1";
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenNotANumber) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Down";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "a";
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest,
       ReturnkNotHandledWhenPressingEnterAndACandidateHasNotBeenChosen) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event;
  event.key = "Enter";
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest,
       ReturnkAcceptWhenPressingEnterAndACandidateHasBeenChosenByPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  // Press "Down" to choose a candidate.
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Down";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "Enter";
  EXPECT_EQ(SuggestionStatus::kAccept,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, HighlightFirstCandidateWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  Press("Down");
  engine_->VerifyCandidateHighlighted(0, true);
}

TEST_F(EmojiSuggesterTest, HighlightButtonCorrectlyWhenPressingUp) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  // Go into the window.
  Press("Down");

  // Press "Up" to choose learn more button.
  Press("Up");
  engine_->VerifyLearnMoreButtonHighlighted(true);

  // Press "Up" to go through candidates;
  for (size_t i = emoji_suggester_->GetCandidatesSizeForTesting(); i > 0; i--) {
    Press("Up");
    engine_->VerifyCandidateHighlighted(i - 1, true);
    engine_->VerifyLearnMoreButtonHighlighted(false);
    if (i != emoji_suggester_->GetCandidatesSizeForTesting()) {
      engine_->VerifyCandidateHighlighted(i, false);
    }
  }

  // Press "Up" to go to learn more button from first candidate.
  Press("Up");
  engine_->VerifyLearnMoreButtonHighlighted(true);
}

TEST_F(EmojiSuggesterTest, HighlightButtonCorrectlyWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  // Press "Down" to go through candidates.
  for (size_t i = 0; i < emoji_suggester_->GetCandidatesSizeForTesting(); i++) {
    Press("Down");
    engine_->VerifyCandidateHighlighted(i, true);
    engine_->VerifyLearnMoreButtonHighlighted(false);
    if (i != 0) {
      engine_->VerifyCandidateHighlighted(i - 1, false);
    }
  }

  // Go to LearnMore Button
  Press("Down");
  engine_->VerifyLearnMoreButtonHighlighted(true);
  engine_->VerifyCandidateHighlighted(
      emoji_suggester_->GetCandidatesSizeForTesting() - 1, false);

  // Go to first candidate
  Press("Down");
  engine_->VerifyLearnMoreButtonHighlighted(false);
  engine_->VerifyCandidateHighlighted(0, true);
}

TEST_F(EmojiSuggesterTest,
       OpenSettingWhenPressingEnterAndLearnMoreButtonIsChosen) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  // Go into the window.
  Press("Down");
  // Choose Learn More Button.
  Press("Up");
  engine_->VerifyLearnMoreButtonHighlighted(true);

  EXPECT_EQ(Press("Enter"), SuggestionStatus::kOpenSettings);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndicesWhenFirstSuggesting) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndexAfterPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  Press("Down");

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndicesAfterGettingSuggestionsTwice) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest,
       DoesNotShowIndicesAfterPressingDownThenGetNewSuggestions) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  Press("Down");
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, ShowSettingLinkCorrectly) {
  for (int i = 0; i < kEmojiSuggesterShowSettingMaxCount; i++) {
    emoji_suggester_->Suggest(base::UTF8ToUTF16("happy "));
    // Dismiss suggestion.
    Press("Esc");
    engine_->VerifyShowSettingLink(true);
  }
  emoji_suggester_->Suggest(base::UTF8ToUTF16("happy "));
  engine_->VerifyShowSettingLink(false);
}

TEST_F(EmojiSuggesterTest, RecordsTimeToAccept) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToAccept.Emoji",
                                    0);
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  // Press "Down" to choose and accept a candidate.
  Press("Down");
  Press("Enter");
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToAccept.Emoji",
                                    1);
}

TEST_F(EmojiSuggesterTest, RecordsTimeToDismiss) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToDismiss.Emoji",
                                    0);
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  // Press "Esc" to dismiss.
  Press("Esc");
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToDismiss.Emoji",
                                    1);
}

}  // namespace chromeos
