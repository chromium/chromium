// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_diacritics_suggester.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

struct DiacriticsTestCase {
  char longpress_char;
  ui::DomCode code;
  bool is_shifted;
  std::u16string surrounding_text;
  std::u16string invalid_surrounding_text;
  std::vector<std::u16string> candidates;
};

class LongpressDiacriticsSuggesterTest
    : public AshTestBase,
      public testing::WithParamInterface<DiacriticsTestCase> {};

using AssistiveWindowButton = ui::ime::AssistiveWindowButton;

const int kContextId = 24601;
const char kUSEngineId[] = "xkb:us::eng";
const auto kDigitToDomCode = base::MakeFixedFlatMap<int, ui::DomCode>({
    {0, ui::DomCode::DIGIT0},
    {1, ui::DomCode::DIGIT1},
    {2, ui::DomCode::DIGIT2},
    {3, ui::DomCode::DIGIT3},
    {4, ui::DomCode::DIGIT4},
    {5, ui::DomCode::DIGIT5},
    {6, ui::DomCode::DIGIT6},
    {7, ui::DomCode::DIGIT7},
    {8, ui::DomCode::DIGIT8},
    {9, ui::DomCode::DIGIT9},
});

ui::KeyEvent CreateKeyEventFromCode(const ui::DomCode& code) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN, code,
                      ui::EF_NONE, ui::DomKey::NONE, ui::EventTimeForNow());
}

ui::KeyEvent CreateRepeatKeyEventFromCode(const ui::DomCode& code,
                                          bool shifted) {
  int flags = ui::EF_IS_REPEAT;
  if (shifted) {
    flags |= ui::EF_SHIFT_DOWN;
  }
  return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN, code, flags,
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
          ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion,
      .suggestion_index = index,
      .announce_string = announce_string,
  };
  return button;
}
}  // namespace

TEST_P(LongpressDiacriticsSuggesterTest, SuggestsOnTrySuggest) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
}

TEST_P(LongpressDiacriticsSuggesterTest,
       ShouldReturnFalseForInvalidTextChangeEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  EXPECT_FALSE(suggester.TrySuggestWithSurroundingText(
      GetParam().invalid_surrounding_text,
      gfx::Range(GetParam().invalid_surrounding_text.size())));
}

TEST_P(LongpressDiacriticsSuggesterTest, DoesNotSuggestForInvalidEngineId) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId("xkb::someunsupportedengine");
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress('a');

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
}

TEST_P(LongpressDiacriticsSuggesterTest, DoesNotSuggestForInvalidKeyChar) {
  FakeSuggestionHandler suggestion_handler;
  base::HistogramTester histogram_tester;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress('~');  // Char doesn't have diacritics.

  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(), u"");
  histogram_tester.ExpectUniqueSample("Ash.NotifierFramework.Nudge.ShownCount",
                                      NudgeCatalogName::kDisableDiacritics, 1);
}

TEST_P(LongpressDiacriticsSuggesterTest, DoesNotSuggestAfterBlur) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
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
  suggester.SetEngineId(kUSEngineId);
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
       HighlightsSettingsOnInitialPreviousKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_LEFT));

  AssistiveWindowButton learn_more_button = {
      .id = ui::ime::ButtonId::kLearnMore,
      .window_type =
          ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion,
  };

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(), learn_more_button);
}

TEST_P(LongpressDiacriticsSuggesterTest, HighlightIncrementsOnNextKeyEvent) {
  size_t expected_candidate_index = 2 % GetParam().candidates.size();
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
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

TEST_P(LongpressDiacriticsSuggesterTest, HighlightIncrementsOnTabKeyEvent) {
  size_t expected_candidate_index = 2 % GetParam().candidates.size();
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::TAB));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::TAB));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::TAB));

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
  suggester.SetEngineId(kUSEngineId);
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
  size_t expected_candidate_index = 9 % (GetParam().candidates.size() + 1);
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  for (int i = 0; i < 10; i++) {
    suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  }

  AssistiveWindowButton learn_more_button = {
      .id = ui::ime::ButtonId::kLearnMore,
      .window_type =
          ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion,
  };
  AssistiveWindowButton expectedButton =
      expected_candidate_index == GetParam().candidates.size()
          ? learn_more_button
          : CreateDiacriticsButtonFor(
                expected_candidate_index,
                GetParam().candidates[expected_candidate_index]);

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(), expectedButton);
}

TEST_P(LongpressDiacriticsSuggesterTest,
       HighlightWrapsAroundAfterFirstIndexOnPreviousKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_LEFT));

  AssistiveWindowButton learn_more_button = {
      .id = ui::ime::ButtonId::kLearnMore,
      .window_type =
          ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion,
  };

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetSuggestionText(),
            Join(GetParam().candidates));
  EXPECT_EQ(suggestion_handler.GetHighlightedButton(), learn_more_button);
}

TEST_P(LongpressDiacriticsSuggesterTest,
       ResetsHighlightsAfterBlurForNextKeyEvent) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
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
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);
  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.AcceptSuggestion(1);

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
  suggester.SetEngineId(kUSEngineId);
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
  suggester.SetEngineId(kUSEngineId);
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
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ENTER));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
            GetParam().candidates[0]);
  EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
}

TEST_P(LongpressDiacriticsSuggesterTest, NotHandledOnDigit0KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT0));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
}

TEST_P(LongpressDiacriticsSuggesterTest, HandlesDigit1KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT1));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
            GetParam().candidates[0]);
  EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
}

TEST_P(LongpressDiacriticsSuggesterTest, HandlesDigit2KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT2));

  if (GetParam().candidates.size() < 2) {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  } else {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
    EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
              GetParam().candidates[1]);
    EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
  }
}

TEST_P(LongpressDiacriticsSuggesterTest, HandlesDigit3KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT3));

  if (GetParam().candidates.size() < 3) {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  } else {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
    EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
              GetParam().candidates[2]);
    EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
  }
}

TEST_P(LongpressDiacriticsSuggesterTest, HandlesDigit4KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT4));

  if (GetParam().candidates.size() < 4) {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  } else {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
    EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
              GetParam().candidates[3]);
    EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
  }
}

TEST_P(LongpressDiacriticsSuggesterTest, HandlesDigit5KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT5));

  if (GetParam().candidates.size() < 5) {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  } else {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
    EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
              GetParam().candidates[4]);
    EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
  }
}

TEST_P(LongpressDiacriticsSuggesterTest, HandlesDigit6KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT6));

  if (GetParam().candidates.size() < 6) {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  } else {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
    EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
              GetParam().candidates[5]);
    EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
  }
}

TEST_P(LongpressDiacriticsSuggesterTest, HandlesDigit7KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT7));

  if (GetParam().candidates.size() < 7) {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  } else {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
    EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
              GetParam().candidates[6]);
    EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
  }
}

TEST_P(LongpressDiacriticsSuggesterTest, HandlesDigit8KeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT8));

  if (GetParam().candidates.size() < 8) {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
  } else {
    EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
    EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
    EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
              GetParam().candidates[7]);
    EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
  }
}

TEST_P(LongpressDiacriticsSuggesterTest,
       NotHandledOnEnterKeyPressIfNoHighlight) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ENTER));

  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
}

TEST_P(LongpressDiacriticsSuggesterTest, DismissSuggestionOnEscKeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));

  EXPECT_EQ(
      suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ESCAPE)),
      SuggestionStatus::kDismiss);
  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
}

TEST_P(LongpressDiacriticsSuggesterTest, DismissSuggestionOnSecondKeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  EXPECT_EQ(suggester.HandleKeyEvent(CreateKeyEventFromCode(GetParam().code)),
            SuggestionStatus::kNotHandled);
  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
}

TEST_P(LongpressDiacriticsSuggesterTest,
       AcceptSuggestionOnSecondKeyPressIfHighlighted) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));

  EXPECT_EQ(suggester.HandleKeyEvent(CreateKeyEventFromCode(GetParam().code)),
            SuggestionStatus::kNotHandled);
  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_FALSE(suggestion_handler.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler.GetAcceptedSuggestionText(),
            GetParam().candidates[0]);
  EXPECT_EQ(suggestion_handler.GetDeletePreviousUtf16Len(), 1u);
}

TEST_P(LongpressDiacriticsSuggesterTest, NoDismissSuggestionOnRepeatKeyPress) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  EXPECT_EQ(suggester.HandleKeyEvent(CreateRepeatKeyEventFromCode(
                GetParam().code, GetParam().is_shifted)),
            SuggestionStatus::kNotHandled);
  EXPECT_EQ(suggestion_handler.GetContextId(), kContextId);
  EXPECT_TRUE(suggestion_handler.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler.GetAcceptedSuggestion());
}

TEST_P(LongpressDiacriticsSuggesterTest, ReturnsDiacriticsProposeActionType) {
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  EXPECT_EQ(suggester.GetProposeActionType(),
            AssistiveType::kLongpressDiacritics);
}

TEST_P(LongpressDiacriticsSuggesterTest, RecordsAcceptanceCharCodeMetric) {
  base::HistogramTester histogram_tester;

  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  int histogram_accept_count = 0;
  for (size_t i = 0; i < 9 && i < GetParam().candidates.size(); i++) {
    // Insert using dom code for index + 1 (i.e. DIGIT1 inserts 0th index
    // candidate)
    ui::DomCode dom_code = kDigitToDomCode.find(i + 1)->second;
    suggester.TrySuggestOnLongpress(GetParam().longpress_char);
    suggester.HandleKeyEvent(CreateKeyEventFromCode(dom_code));

    histogram_tester.ExpectTotalCount(
        "InputMethod.PhysicalKeyboard.LongpressDiacritics.AcceptedChar",
        ++histogram_accept_count);
    int char_code = int(GetParam().candidates[i][0]);
    histogram_tester.ExpectBucketCount(
        "InputMethod.PhysicalKeyboard.LongpressDiacritics.AcceptedChar",
        char_code, 1);
  }
}

TEST_P(LongpressDiacriticsSuggesterTest, RecordsShowWindowActionMetric) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.LongpressDiacritics.Action",
      IMEPKLongpressDiacriticAction::kShowWindow, 1);
}

TEST_P(LongpressDiacriticsSuggesterTest, RecordsAcceptActionMetric) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT1));

  histogram_tester.ExpectBucketCount(
      "InputMethod.PhysicalKeyboard.LongpressDiacritics.Action",
      IMEPKLongpressDiacriticAction::kAccept, 1);
}

TEST_P(LongpressDiacriticsSuggesterTest, RecordsDismissActionMetricOnEsc) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ESCAPE));

  histogram_tester.ExpectBucketCount(
      "InputMethod.PhysicalKeyboard.LongpressDiacritics.Action",
      IMEPKLongpressDiacriticAction::kDismiss, 1);
}

TEST_P(LongpressDiacriticsSuggesterTest,
       RecordsDismissActionMetricOnOtherKeyPress) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(GetParam().code));

  histogram_tester.ExpectBucketCount(
      "InputMethod.PhysicalKeyboard.LongpressDiacritics.Action",
      IMEPKLongpressDiacriticAction::kDismiss, 1);
}

TEST_P(LongpressDiacriticsSuggesterTest, A11yAnnounceOnShowWindow) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 1u);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().front(),
            u"Accent marks menu open. Press left, right, or number keys to "
            u"navigate and enter to insert.");
}

TEST_P(LongpressDiacriticsSuggesterTest, A11yAnnounceOnDismissWithEsc) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ESCAPE));

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 2u);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().front(),
            u"Accent marks menu open. Press left, right, or number keys to "
            u"navigate and enter to insert.");
  EXPECT_EQ(suggestion_handler.GetAnnouncements().back(),
            u"Accent marks menu dismissed.");
}

TEST_P(LongpressDiacriticsSuggesterTest, A11yAnnounceOnDismissByTyping) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(GetParam().code));

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 2u);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().front(),
            u"Accent marks menu open. Press left, right, or number keys to "
            u"navigate and enter to insert.");
  EXPECT_EQ(suggestion_handler.GetAnnouncements().back(),
            u"Accent marks menu dismissed.");
}

TEST_P(LongpressDiacriticsSuggesterTest, A11yAnnounceOnAcceptViaDigit) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::DIGIT1));

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 2u);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().front(),
            u"Accent marks menu open. Press left, right, or number keys to "
            u"navigate and enter to insert.");
  EXPECT_EQ(suggestion_handler.GetAnnouncements().back(),
            u"Accent mark inserted.");
}

TEST_P(LongpressDiacriticsSuggesterTest, A11yAnnounceOnAcceptViaEnter) {
  base::HistogramTester histogram_tester;
  FakeSuggestionHandler suggestion_handler;
  LongpressDiacriticsSuggester suggester =
      LongpressDiacriticsSuggester(&suggestion_handler);
  suggester.SetEngineId(kUSEngineId);
  suggester.OnFocus(kContextId);

  suggester.TrySuggestOnLongpress(GetParam().longpress_char);
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ARROW_RIGHT));
  suggester.HandleKeyEvent(CreateKeyEventFromCode(ui::DomCode::ENTER));

  ASSERT_EQ(suggestion_handler.GetAnnouncements().size(), 2u);
  EXPECT_EQ(suggestion_handler.GetAnnouncements().front(),
            u"Accent marks menu open. Press left, right, or number keys to "
            u"navigate and enter to insert.");
  EXPECT_EQ(suggestion_handler.GetAnnouncements().back(),
            u"Accent mark inserted.");
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LongpressDiacriticsSuggesterTest,
    testing::ValuesIn<DiacriticsTestCase>(
        {{'a',
          ui::DomCode::US_A,
          false,
          u"ca",
          u"caf",
          {u"à", u"á", u"â", u"ä", u"æ", u"ã", u"å", u"ā"}},
         {'A',
          ui::DomCode::US_A,
          true,
          u"cA",
          u"cAf",
          {u"À", u"Á", u"Â", u"Ä", u"Æ", u"Ã", u"Å", u"Ā"}},
         {'c', ui::DomCode::US_C, false, u"c", u"ca", {u"ç"}},
         {'C', ui::DomCode::US_C, true, u"C", u"Ca", {u"Ç"}},
         {'e',
          ui::DomCode::US_E,
          false,
          u"soufle",
          u"soufles",
          {u"é", u"è", u"ê", u"ë", u"ē"}},
         {'E',
          ui::DomCode::US_E,
          true,
          u"SOUFLE",
          u"SOUFLES",
          {u"É", u"È", u"Ê", u"Ë", u"Ē"}}}),
    [](const testing::TestParamInfo<DiacriticsTestCase>& info) {
      return std::string(1, info.param.longpress_char);
    });

}  // namespace input_method
}  // namespace ash
