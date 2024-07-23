// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/emoji_suggester.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using AssistiveSuggestion = ime::AssistiveSuggestion;
using AssistiveSuggestionMode = ime::AssistiveSuggestionMode;
using AssistiveSuggestionType = ime::AssistiveSuggestionType;

}  // namespace

ui::KeyEvent CreateKeyEventFromCode(const ui::DomCode& code) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN, code,
                      ui::EF_NONE, ui::DomKey::NONE, ui::EventTimeForNow());
}

const char kEmojiData[] = "happy,😀;😃;😄";
const int kContextId = 24601;

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
        currently_highlighted_index_ =
            highlighted ? button.suggestion_index : INT_MAX;
        candidate_highlighted_[button.suggestion_index] = highlighted ? 1 : 0;
        return true;
      default:
        return false;
    }
  }

  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override {
    context_id_ = context_id;
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

  void VerifyContextId(const int context_id) {
    EXPECT_EQ(context_id_, context_id);
  }

  bool DismissSuggestion(int context_id, std::string* error) override {
    return false;
  }

  bool AcceptSuggestion(int context_id, std::string* error) override {
    return false;
  }

  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}

  void ClickButton(const ui::ime::AssistiveWindowButton& button) override {}

  bool AcceptSuggestionCandidate(int context_id,
                                 const std::u16string& candidate,
                                 size_t delete_previous_utf16_len,
                                 bool use_replace_surrounding_text,
                                 std::string* error) override {
    return false;
  }

  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override {
    return false;
  }

  void Announce(const std::u16string& message) override {}

 private:
  bool show_indices_ = false;
  bool show_setting_link_ = false;
  bool learn_more_button_highlighted_ = false;
  std::vector<int> candidate_highlighted_;
  size_t currently_highlighted_index_ = INT_MAX;
  int context_id_ = -1;
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
    emoji_suggester_->OnFocus(kContextId);
  }

  SuggestionStatus Press(ui::DomCode code) {
    return emoji_suggester_->HandleKeyEvent(CreateKeyEventFromCode(code));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmojiSuggester> emoji_suggester_;
  std::unique_ptr<TestSuggestionHandler> engine_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
};

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpace) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
}

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpaceInNewLine) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(
      u"oldline\nhappy ", gfx::Range(14)));
}

TEST_F(EmojiSuggesterTest, PassesContextIdToHandlerOnSuggestion) {
  emoji_suggester_->TrySuggestWithSurroundingText(u"happy ", gfx::Range(6));
  engine_->VerifyContextId(kContextId);
}

TEST_F(EmojiSuggesterTest, SuggestWhenStringStartsWithOpenBracket) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"(happy ",
                                                              gfx::Range(7)));
}

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpaceAndIsUppercase) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"HAPPY ",
                                                              gfx::Range(6)));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenStringEndsWithNewLine) {
  EXPECT_FALSE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy\n",
                                                               gfx::Range(6)));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenStringDoesNotEndWithSpace) {
  EXPECT_FALSE(
      emoji_suggester_->TrySuggestWithSurroundingText(u"happy", gfx::Range(5)));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestOnWhenContainsCursorSelection) {
  EXPECT_FALSE(emoji_suggester_->TrySuggestWithSurroundingText(
      u"happy ", gfx::Range(6, 2)));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestOnWhenNotAtEndOfText) {
  EXPECT_FALSE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                               gfx::Range(3)));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenWordNotInMap) {
  EXPECT_FALSE(
      emoji_suggester_->TrySuggestWithSurroundingText(u"hapy ", gfx::Range(5)));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestAfterBlur) {
  emoji_suggester_->OnBlur();
  EXPECT_FALSE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                               gfx::Range(6)));
}

TEST_F(EmojiSuggesterTest, DoNotShowSuggestionWhenVirtualKeyboardEnabled) {
  chrome_keyboard_controller_client_->set_keyboard_visible_for_test(true);
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  EXPECT_FALSE(emoji_suggester_->HasSuggestions());
}

TEST_F(EmojiSuggesterTest, ReturnkBrowsingWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  ui::KeyEvent event = CreateKeyEventFromCode(ui::DomCode::ARROW_DOWN);
  EXPECT_EQ(SuggestionStatus::kBrowsing,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkBrowsingWhenPressingUp) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  ui::KeyEvent event = CreateKeyEventFromCode(ui::DomCode::ARROW_UP);
  EXPECT_EQ(SuggestionStatus::kBrowsing,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkDismissWhenPressingEsc) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  ui::KeyEvent event = CreateKeyEventFromCode(ui::DomCode::ESCAPE);
  EXPECT_EQ(SuggestionStatus::kDismiss,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenValidNumber) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  ui::KeyEvent event1 = CreateKeyEventFromCode(ui::DomCode::ARROW_DOWN);
  emoji_suggester_->HandleKeyEvent(event1);
  ui::KeyEvent event2 = CreateKeyEventFromCode(ui::DomCode::DIGIT1);
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenNotANumber) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  ui::KeyEvent event1 = CreateKeyEventFromCode(ui::DomCode::ARROW_DOWN);
  emoji_suggester_->HandleKeyEvent(event1);
  ui::KeyEvent event2 = CreateKeyEventFromCode(ui::DomCode::US_A);
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest,
       ReturnkNotHandledWhenPressingEnterAndACandidateHasNotBeenChosen) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  ui::KeyEvent event = CreateKeyEventFromCode(ui::DomCode::ENTER);
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest,
       ReturnkAcceptWhenPressingEnterAndACandidateHasBeenChosenByPressingDown) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  // Press ui::DomCode::ARROW_DOWN to choose a candidate.
  ui::KeyEvent event1 = CreateKeyEventFromCode(ui::DomCode::ARROW_DOWN);
  emoji_suggester_->HandleKeyEvent(event1);
  ui::KeyEvent event2 = CreateKeyEventFromCode(ui::DomCode::ENTER);
  EXPECT_EQ(SuggestionStatus::kAccept,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, HighlightFirstCandidateWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  Press(ui::DomCode::ARROW_DOWN);
  engine_->VerifyCandidateHighlighted(0, true);
}

TEST_F(EmojiSuggesterTest, HighlightButtonCorrectlyWhenPressingUp) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));

  // Go into the window.
  Press(ui::DomCode::ARROW_DOWN);

  // Press ui::DomCode::ARROW_UP to choose learn more button.
  Press(ui::DomCode::ARROW_UP);
  engine_->VerifyLearnMoreButtonHighlighted(true);

  // Press ui::DomCode::ARROW_UP to go through candidates;
  for (size_t i = emoji_suggester_->GetCandidatesSizeForTesting(); i > 0; i--) {
    Press(ui::DomCode::ARROW_UP);
    engine_->VerifyCandidateHighlighted(i - 1, true);
    engine_->VerifyLearnMoreButtonHighlighted(false);
    if (i != emoji_suggester_->GetCandidatesSizeForTesting()) {
      engine_->VerifyCandidateHighlighted(i, false);
    }
  }

  // Press ui::DomCode::ARROW_UP to go to learn more button from first
  // candidate.
  Press(ui::DomCode::ARROW_UP);
  engine_->VerifyLearnMoreButtonHighlighted(true);
}

TEST_F(EmojiSuggesterTest, HighlightButtonCorrectlyWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));

  // Press ui::DomCode::ARROW_DOWN to go through candidates.
  for (size_t i = 0; i < emoji_suggester_->GetCandidatesSizeForTesting(); i++) {
    Press(ui::DomCode::ARROW_DOWN);
    engine_->VerifyCandidateHighlighted(i, true);
    engine_->VerifyLearnMoreButtonHighlighted(false);
    if (i != 0) {
      engine_->VerifyCandidateHighlighted(i - 1, false);
    }
  }

  // Go to LearnMore Button
  Press(ui::DomCode::ARROW_DOWN);
  engine_->VerifyLearnMoreButtonHighlighted(true);
  engine_->VerifyCandidateHighlighted(
      emoji_suggester_->GetCandidatesSizeForTesting() - 1, false);

  // Go to first candidate
  Press(ui::DomCode::ARROW_DOWN);
  engine_->VerifyLearnMoreButtonHighlighted(false);
  engine_->VerifyCandidateHighlighted(0, true);
}

TEST_F(EmojiSuggesterTest,
       OpenSettingWhenPressingEnterAndLearnMoreButtonIsChosen) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));

  // Go into the window.
  Press(ui::DomCode::ARROW_DOWN);
  // Choose Learn More Button.
  Press(ui::DomCode::ARROW_UP);
  engine_->VerifyLearnMoreButtonHighlighted(true);

  EXPECT_EQ(Press(ui::DomCode::ENTER), SuggestionStatus::kOpenSettings);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndicesWhenFirstSuggesting) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndexAfterPressingDown) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  Press(ui::DomCode::ARROW_DOWN);

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, DoesNotShowIndicesAfterGettingSuggestionsTwice) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest,
       DoesNotShowIndicesAfterPressingDownThenGetNewSuggestions) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  Press(ui::DomCode::ARROW_DOWN);
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));

  engine_->VerifyShowIndices(false);
}

TEST_F(EmojiSuggesterTest, ShowSettingLinkCorrectly) {
  for (int i = 0; i < kEmojiSuggesterShowSettingMaxCount; i++) {
    emoji_suggester_->TrySuggestWithSurroundingText(u"happy ", gfx::Range(6));
    // Dismiss suggestion.
    Press(ui::DomCode::ESCAPE);
    engine_->VerifyShowSettingLink(true);
  }
  emoji_suggester_->TrySuggestWithSurroundingText(u"happy ", gfx::Range(6));
  engine_->VerifyShowSettingLink(false);
}

TEST_F(EmojiSuggesterTest, IsShowingSuggestionTrueWhenCandidatesAvailable) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  EXPECT_TRUE(emoji_suggester_->HasSuggestions());
}

TEST_F(EmojiSuggesterTest, IsShowingSuggestionFalseWhenCandidatesUnavailable) {
  EXPECT_FALSE(
      emoji_suggester_->TrySuggestWithSurroundingText(u"hapy", gfx::Range(4)));
  EXPECT_FALSE(emoji_suggester_->HasSuggestions());
}

TEST_F(EmojiSuggesterTest, GetSuggestionReturnsCandidatesWhenAvailable) {
  EXPECT_TRUE(emoji_suggester_->TrySuggestWithSurroundingText(u"happy ",
                                                              gfx::Range(6)));
  EXPECT_EQ(
      emoji_suggester_->GetSuggestions(),
      (std::vector<AssistiveSuggestion>{
          AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                              .type = AssistiveSuggestionType::kAssistiveEmoji,
                              .text = "😀"},
          AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                              .type = AssistiveSuggestionType::kAssistiveEmoji,
                              .text = "😃"},
          AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                              .type = AssistiveSuggestionType::kAssistiveEmoji,
                              .text = "😄"},
      }));
}

TEST_F(EmojiSuggesterTest,
       GetSuggestionDoesNotReturnCandidatesWhenUnavailable) {
  EXPECT_FALSE(
      emoji_suggester_->TrySuggestWithSurroundingText(u"hapy", gfx::Range(4)));
  EXPECT_TRUE(emoji_suggester_->GetSuggestions().empty());
}
}  // namespace input_method
}  // namespace ash
