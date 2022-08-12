// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

namespace {

using ::testing::_;

constexpr char kCoverageHistogramName[] = "InputMethod.Assistive.Coverage";
constexpr char kSuccessHistogramName[] = "InputMethod.Assistive.Success";
constexpr char kDelayHistogramName[] =
    "InputMethod.Assistive.Autocorrect.Delay";
constexpr char kAutocorrectActionHistogramName[] =
    "InputMethod.Assistive.Autocorrect.Actions";
constexpr char kVKAutocorrectActionHistogramName[] =
    "InputMethod.Assistive.Autocorrect.Actions.VK";

// A helper for testing autocorrect histograms. There are redundant metrics
// for each autocorrect action and the helper ensures that all the relevant
// metrics for one action are updated properly.
void ExpectAutocorrectHistograms(const base::HistogramTester& histogram_tester,
                                 bool visible_vk,
                                 int window_shown,
                                 int underlined,
                                 int reverted,
                                 int accepted,
                                 int cleared_underline) {
  // Window shown metrics.
  histogram_tester.ExpectBucketCount(kCoverageHistogramName,
                                     AssistiveType::kAutocorrectWindowShown,
                                     window_shown);
  histogram_tester.ExpectBucketCount(kAutocorrectActionHistogramName,
                                     AutocorrectActions::kWindowShown,
                                     window_shown);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(kVKAutocorrectActionHistogramName,
                                       AutocorrectActions::kWindowShown,
                                       window_shown);
  }

  // Underlined metrics.
  histogram_tester.ExpectBucketCount(kCoverageHistogramName,
                                     AssistiveType::kAutocorrectUnderlined,
                                     underlined);
  histogram_tester.ExpectBucketCount(kAutocorrectActionHistogramName,
                                     AutocorrectActions::kUnderlined,
                                     underlined);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(kVKAutocorrectActionHistogramName,
                                       AutocorrectActions::kUnderlined,
                                       underlined);
  }

  // Revert metrics.
  histogram_tester.ExpectBucketCount(
      kCoverageHistogramName, AssistiveType::kAutocorrectReverted, reverted);
  histogram_tester.ExpectBucketCount(
      kSuccessHistogramName, AssistiveType::kAutocorrectReverted, reverted);
  histogram_tester.ExpectBucketCount(kAutocorrectActionHistogramName,
                                     AutocorrectActions::kReverted, reverted);
  histogram_tester.ExpectTotalCount(kDelayHistogramName, reverted);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(kVKAutocorrectActionHistogramName,
                                       AutocorrectActions::kReverted, reverted);
  }

  // Accept metrics.
  histogram_tester.ExpectBucketCount(
      kAutocorrectActionHistogramName,
      AutocorrectActions::kUserAcceptedAutocorrect, accepted);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(
        kVKAutocorrectActionHistogramName,
        AutocorrectActions::kUserAcceptedAutocorrect, accepted);
  }

  // Clear underline metrics.
  histogram_tester.ExpectBucketCount(
      kAutocorrectActionHistogramName,
      AutocorrectActions::kUserActionClearedUnderline, cleared_underline);

  const int total_actions =
      window_shown + underlined + reverted + accepted + cleared_underline;
  const int total_coverage = window_shown + underlined + reverted;

  // Count total bucket to test side-effects and make the helper robust against
  // future changes of the metric buckets.
  histogram_tester.ExpectTotalCount(kCoverageHistogramName, total_coverage);
  histogram_tester.ExpectTotalCount(kSuccessHistogramName, reverted);
  histogram_tester.ExpectTotalCount(kAutocorrectActionHistogramName,
                                    total_actions);
  histogram_tester.ExpectTotalCount(kVKAutocorrectActionHistogramName,
                                    visible_vk ? total_actions : 0);
}

// A helper to create properties for hidden undo window.
AssistiveWindowProperties CreateHiddenUndoWindowProperties() {
  AssistiveWindowProperties window_properties;
  window_properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
  window_properties.visible = false;
  return window_properties;
}

// A helper to create properties for shown undo window.
AssistiveWindowProperties CreateVisibleUndoWindowProperties(
    const std::u16string& original_text,
    const std::u16string& autocorrected_text) {
  AssistiveWindowProperties window_properties;
  window_properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
  window_properties.visible = true;
  window_properties.announce_string =
      l10n_util::GetStringFUTF16(IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN,
                                 original_text, autocorrected_text);
  return window_properties;
}

// A helper for creating key event.
ui::KeyEvent CreateKeyEvent(ui::DomKey key, ui::DomCode code) {
  return ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, code, ui::EF_NONE,
                      key, ui::EventTimeForNow());
}

class MockSuggestionHandler : public SuggestionHandlerInterface {
 public:
  MOCK_METHOD(bool,
              DismissSuggestion,
              (int context_id, std::string* error),
              (override));
  MOCK_METHOD(bool,
              SetSuggestion,
              (int context_id,
               const ui::ime::SuggestionDetails& details,
               std::string* error),
              (override));
  MOCK_METHOD(bool,
              AcceptSuggestion,
              (int context_id, std::string* error),
              (override));
  MOCK_METHOD(void,
              OnSuggestionsChanged,
              (const std::vector<std::string>& suggestions),
              (override));
  MOCK_METHOD(bool,
              SetButtonHighlighted,
              (int context_id,
               const ui::ime::AssistiveWindowButton& button,
               bool highlighted,
               std::string* error),
              (override));
  MOCK_METHOD(void,
              ClickButton,
              (const ui::ime::AssistiveWindowButton& button),
              (override));
  MOCK_METHOD(bool,
              AcceptSuggestionCandidate,
              (int context_id,
               const std::u16string& candidate,
               size_t delete_previous_utf16_len,
               std::string* error),
              (override));
  MOCK_METHOD(bool,
              SetAssistiveWindowProperties,
              (int context_id,
               const AssistiveWindowProperties& assistive_window,
               std::string* error),
              (override));
  MOCK_METHOD(void, Announce, (const std::u16string& text), (override));
};

class AutocorrectManagerTest : public testing::Test {
 protected:
  AutocorrectManagerTest() : manager_(&mock_suggestion_handler_) {
    ui::IMEBridge::Get()->SetInputContextHandler(
        &mock_ime_input_context_handler_);
    keyboard_client_ = ChromeKeyboardControllerClient::CreateForTest();
    keyboard_client_->set_keyboard_visible_for_test(false);
  }

  ui::MockIMEInputContextHandler mock_ime_input_context_handler_;
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler_;
  AutocorrectManager manager_;
  std::unique_ptr<ChromeKeyboardControllerClient> keyboard_client_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectSetsRangeWhenNoPendingAutocorrectExists) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectSetsRangeWhenPendingAutocorrectExists) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"cn", u"can");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(4, 7));
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectDoesNotSetRangeWhenInputContextIsNull) {
  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"cn", u"can");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectClearsRangeWithEmptyInputRange) {
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(3, 7));
  manager_.HandleAutocorrect(gfx::Range(), u"", u"");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest, FewKeyPressesDoesNotClearAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));

  EXPECT_TRUE(
      !mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest, EnoughKeyPressesClearsAutcorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));

  EXPECT_TRUE(mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest, MovingCursorInsideRangeShowsAssistiveWindow) {
  manager_.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/4,
                                    /*anchor_pos=*/4);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  AssistiveWindowProperties properties =
      CreateVisibleUndoWindowProperties(u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, properties, _));

  manager_.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/1,
                                    /*anchor_pos=*/1);
}

TEST_F(AutocorrectManagerTest, MovingCursorOutsideRangeHidesAssistiveWindow) {
  manager_.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/4,
                                    /*anchor_pos=*/4);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowProperties(u"teh", u"the");
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    AssistiveWindowProperties hidden_properties =
        CreateHiddenUndoWindowProperties();
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, hidden_properties, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/1,
                                    /*anchor_pos=*/1);
  manager_.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/4,
                                    /*anchor_pos=*/4);
}

TEST_F(AutocorrectManagerTest, UndoAutocorrectSingleWordInComposition) {
  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::InputMethodAsh ime(nullptr);
  ui::IMEBridge::Get()->SetInputContextHandler(&ime);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  manager_.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/4,
                                    /*anchor_pos=*/4);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Move cursor to the middle of 'the' and bring the text into composition.
  fake_text_input_client.SetTextAndSelection(u"the ", gfx::Range(2));
  ime.SetComposingRange(0, 3, {});

  manager_.UndoAutocorrect();

  EXPECT_EQ(fake_text_input_client.text(), u"teh ");
}

TEST_F(AutocorrectManagerTest, UndoAutocorrectMultipleWordInComposition) {
  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::InputMethodAsh ime(nullptr);
  ui::IMEBridge::Get()->SetInputContextHandler(&ime);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  manager_.OnSurroundingTextChanged(u"hello world ", /*cursor_pos=*/12,
                                    /*anchor_pos=*/12);
  manager_.HandleAutocorrect(gfx::Range(0, 11), u"helloworld", u"hello world");

  // Move cursor to the middle of 'hello' and bring the word into composition.
  fake_text_input_client.SetTextAndSelection(u"hello world ", gfx::Range(2));
  ime.SetComposingRange(0, 5, {});

  manager_.UndoAutocorrect();

  EXPECT_EQ(fake_text_input_client.text(), u"helloworld ");
}

TEST_F(AutocorrectManagerTest, MovingCursorFarAfterRangeAcceptsAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"the abc", 7, 7);
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest, MovingCursorFarBeforeRangeAcceptsAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"abc the", 0, 0);
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest,
       MovingCursorCloseToRangeDoesNotClearAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"abc the def", 1, 1);
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(4, 7));
  manager_.OnSurroundingTextChanged(u"abc the def", 10, 10);
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(4, 7));
}

TEST_F(AutocorrectManagerTest,
       MovingCursorFarAfterRangeRecordsMetricsWhenAcceptingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the abc", 7, 7);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorFarBeforeRangeRecordsMetricsWhenAcceptingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"abc the", 0, 0);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorCloseToRangeDoesNotRecordMetricsForPendingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"abc the def", 1, 1);
  manager_.OnSurroundingTextChanged(u"abc the def", 10, 10);
  manager_.OnSurroundingTextChanged(u"abc the def", 3, 3);
  manager_.OnSurroundingTextChanged(u"abc the def", 8, 8);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorToRangeStartRecordsMetricsForShownUndoWindow) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  manager_.OnSurroundingTextChanged(u"the", 0, 0);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorToRangeEndRecordsMetricsForShownUndoWindow) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  // Moving cursor inside the range does not increase window_shown.
  manager_.OnSurroundingTextChanged(u"the", 3, 3);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       KeepingCursorInsideRangeRecordsMetricsForShownUndoWindowOnce) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  manager_.OnSurroundingTextChanged(u"the", 0, 0);
  manager_.OnSurroundingTextChanged(u"the", 3, 3);
  manager_.OnSurroundingTextChanged(u"the", 2, 2);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorOutThenInsideRangeRecordsMetricsForShownUndoWindowTwice) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // The function is called two times for show and one time for hide.
  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _))
      .Times(3);

  // Moving cursor first inside range, then outside the range and then again
  // back to the range increments the metric for shown window twice.
  manager_.OnSurroundingTextChanged(u"the", 1, 1);
  manager_.OnSurroundingTextChanged(u"the ", 4, 4);
  manager_.OnSurroundingTextChanged(u"the", 3, 3);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/2, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest, KeyPressRecordsMetricsWhenAcceptingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest, KeyPressRecordsMetricsWhenClearingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range());

  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  // TODO(b/161490813): Remove extra calls after fixing OnKeyEvent logic.
  //  Currently, OnKeyEvent waits for four keys before updating metrics for
  //  cleared range.
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       KeyPressDoesNotRecordMetricsWhenAutocorrectIsStillPending) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);

  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest, HandleAutocorrectRecordsMetricsWhenVkIsVisible) {
  keyboard_client_->set_keyboard_visible_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/true,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsWhenNoPendingAutocorrectExists) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsWhenAcceptingPendingAutocorrect) {
  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Create a new autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"cn", u"can");

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/2,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsWhenClearingPendingAutocorrect) {
  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Clear the previous autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range());

  // Handle a new range.
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"cn", u"can");

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/2,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsCorrectlyForNullInputContext) {
  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Make Input context null.
  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // The pending range must be counted as cleared, but `underlined` metric must
  // not be incremented with the empty input context.
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);

  // When there is no pending autocorrect range, nothing is incremented.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsForEmptyInputAndNoPendingRange) {
  // Empty input range does not change autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(), u"", u"");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsForEmptyInputAndPendingRange) {
  // When there is a pending autocorrect, empty input range makes the pending
  // to be counted as accepted.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.HandleAutocorrect(gfx::Range(), u"", u"");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsForEmptyInputAndClearedPending) {
  // When there is a pending autocorrect, but cleared beforehand,
  // empty input range makes the pending to be counted as cleared.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range());
  manager_.HandleAutocorrect(gfx::Range(), u"", u"");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       KeyPressDoesNotRecordMetricsForStaleAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Press enough keys to be on the boundary of accepting autocorrect
  // with key presses.
  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"the abc", 7, 7);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);

  // Set stale autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3));

  // Trigger the logic for implicitly accepting autocorrect by keypress
  // and ensure duplicate count is not happening.
  manager_.OnKeyEvent(key_event);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       AutocorrectHandlerDoesNotRecordMetricsForStaleAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"the abc", 7, 7);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);

  // Set stale autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3));

  // Handle a new autocorrect and ensure the metric is not increase twice.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/2,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
