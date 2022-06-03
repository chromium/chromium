// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_diacritics_suggester.h"

#include <string>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

struct DiacriticsTestCase {
  char longpress_char;
  std::u16string surrounding_text;
  std::u16string invalid_surrounding_text;
  std::vector<std::u16string> candidates;
};

using LongpressDiacriticsSuggesterTest =
    ::testing::TestWithParam<DiacriticsTestCase>;

using AssistiveWindowButton = ui::ime::AssistiveWindowButton;

const int kContextId = 24601;

ui::KeyEvent CreateKeyEventFromCode(const ui::DomCode& code) {
  return ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, code, ui::EF_NONE,
                      ui::DomKey::NONE, ui::EventTimeForNow());
}

// Required since FakeSuggestionHandler joins the candidates.
std::u16string Join(std::vector<std::u16string> candidates) {
  return base::JoinString(candidates, u";");
}

AssistiveWindowButton CreateDiacriticsButtonFor(
    size_t index,
    std::u16string announce_string) {
  AssistiveWindowButton button = {
      .id = ui::ime::ButtonId::kSuggestion,
      .window_type =
          ui::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion,
      .index = index,
      .announce_string = announce_string,
  };
  return button;
}
}  // namespace

TEST_P(LongpressDiacriticsSuggesterTest, SuggestsOnTrySuggest) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
}

TEST_P(LongpressDiacriticsSuggesterTest,
       ShouldOnlyReturnTrueForFirstTextChangeEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  EXPECT_TRUE(suggester.TrySuggestWithSurroundingText(
      GetParam().surrounding_text, GetParam().surrounding_text.size(),
      GetParam().surrounding_text.size()));
  // This cursor position hasn't changed, so the text is still valid. However
  // it should return false because it is the second on change event.
  EXPECT_FALSE(suggester.TrySuggestWithSurroundingText(
      GetParam().surrounding_text + u"somechanges",
      GetParam().surrounding_text.size(), GetParam().surrounding_text.size()));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       ShouldReturnFalseForInvalidTextChangeEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  EXPECT_FALSE(suggester.TrySuggestWithSurroundingText(
      GetParam().invalid_surrounding_text,
      GetParam().invalid_surrounding_text.size(),
      GetParam().invalid_surrounding_text.size()));
}

TEST_P(LongpressDiacriticsSuggesterTest, DoesNotSuggestForInvalidKeyChar) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress('~');  // Char doesn't have diacritics.

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST_P(LongpressDiacriticsSuggesterTest, DoesNotSuggestAfterBlur) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.OnBlur();
  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST_P(LongpressDiacriticsSuggesterTest, HighlightsFirstOnInitialNextKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(0, GetParam().candidates[0]));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       HighlightsLastOnInitialPreviousKeyEvent) {
  size_t expected_candidate_index = GetParam().candidates.size() - 1;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_LEFT));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(
                expected_candidate_index,
                GetParam().candidates[expected_candidate_index]));
}

TEST_P(LongpressDiacriticsSuggesterTest, HighlightIncrementsOnNextKeyEvent) {
  size_t expected_candidate_index = 2 % GetParam().candidates.size();
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(
                expected_candidate_index,
                GetParam().candidates[expected_candidate_index]));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       HighlightDecrementsOnPreviousKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_LEFT));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(0, GetParam().candidates[0]));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       HighlightWrapsAroundAfterLastIndexOnNextKeyEvent) {
  size_t expected_candidate_index = (9 % GetParam().candidates.size());
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  for (int i = 0; i < 10; i++) {
    suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  }

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(
                expected_candidate_index,
                GetParam().candidates[expected_candidate_index]));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       HighlightWrapsAroundAfterFirstIndexOnPreviousKeyEvent) {
  size_t expected_candidate_index = GetParam().candidates.size() - 1;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_LEFT));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(
                expected_candidate_index,
                GetParam().candidates[expected_candidate_index]));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       ResetsHighlightsAfterBlurForNextKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);
  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.OnBlur();
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(0, GetParam().candidates[0]));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       ResetsHighlightsAfterAcceptForNextKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);
  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.AcceptSuggestion(2);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(0, GetParam().candidates[0]));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       ResetsHighlightsAfterDismissForNextKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);
  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.DismissSuggestion();

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(0, GetParam().candidates[0]));
}

TEST_P(LongpressDiacriticsSuggesterTest,
       ResetsHighlightsAfterFocusChangeForNextKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(1);
  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.OnFocus(2);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));

  EXPECT_EQ(suggestion_handler.GetContextId(), 2);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(),
            CreateDiacriticsButtonFor(0, GetParam().candidates[0]));
}

TEST_P(LongpressDiacriticsSuggesterTest, AcceptsOnEnterKeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ENTER));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
            GetParam().candidates[0]);
  EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1);
}

TEST_P(LongpressDiacriticsSuggesterTest,
       NotHandledOnEnterKeyPressIfNoHighlight) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ENTER));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
}

TEST_P(LongpressDiacriticsSuggesterTest, DismissOnEscKeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ESCAPE));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LongpressDiacriticsSuggesterTest,
    testing::ValuesIn<DiacriticsTestCase>(
        {{'a', u"ca", u"caf", {u"à", u"á", u"â", u"ã", u"ã", u"ä", u"å", u"ā"}},
         {'A', u"cA", u"cAf", {u"À", u"Á", u"Â", u"Ã", u"Ä", u"Å", u"Æ", u"Ā"}},
         {'c', u"c", u"ca", {u"ç"}},
         {'C', u"C", u"Ca", {u"Ç"}},
         {'e', u"soufle", u"soufles", {u"è", u"é", u"ê", u"ë", u"ē"}},
         {'E', u"SOUFLE", u"SOUFLES", {u"È", u"É", u"Ê", u"Ë", u"Ē"}}}),
    [](const testing::TestParamInfo<
        LongpressDiacriticsSuggesterTest::ParamType>& info) {
      return std::string(1, info.param.longpress_char);
    });

}  // namespace input_method
}  // namespace ash
