// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_manager.h"

#include "base/test/metrics/histogram_tester.h"
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

TEST(AutocorrectManagerTest, HandleAutocorrectSetsAutocorrectRange) {
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  MockSuggestionHandler mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);

  manager.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  EXPECT_EQ(mock_ime_input_context_handler.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST(AutocorrectManagerTest, OnKeyEventHidesUnderlineAfterEnoughKeyPresses) {
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  MockSuggestionHandler mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  manager.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  const auto key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  EXPECT_FALSE(manager.OnKeyEvent(key_event));
  EXPECT_FALSE(manager.OnKeyEvent(key_event));
  EXPECT_FALSE(manager.OnKeyEvent(key_event));
  EXPECT_FALSE(manager.OnKeyEvent(key_event));
  EXPECT_FALSE(manager.OnKeyEvent(key_event));

  EXPECT_TRUE(mock_ime_input_context_handler.GetAutocorrectRange().is_empty());
}

TEST(AutocorrectManagerTest, MovingCursorInsideRangeShowsAssistiveWindow) {
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  manager.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/4,
                                   /*anchor_pos=*/4);
  manager.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  AssistiveWindowProperties properties;
  properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
  properties.visible = true;
  properties.announce_string = l10n_util::GetStringFUTF16(
      IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN, u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler,
              SetAssistiveWindowProperties(_, properties, _));

  manager.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/1,
                                   /*anchor_pos=*/1);
}

TEST(AutocorrectManagerTest, MovingCursorOutsideRangeHidesAssistiveWindow) {
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  manager.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/4,
                                   /*anchor_pos=*/4);
  manager.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties;
    shown_properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    shown_properties.visible = true;
    shown_properties.announce_string = l10n_util::GetStringFUTF16(
        IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN, u"teh", u"the");
    EXPECT_CALL(mock_suggestion_handler,
                SetAssistiveWindowProperties(_, shown_properties, _));

    AssistiveWindowProperties hidden_properties;
    hidden_properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    hidden_properties.visible = false;
    EXPECT_CALL(mock_suggestion_handler,
                SetAssistiveWindowProperties(_, hidden_properties, _));
  }

  manager.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/1,
                                   /*anchor_pos=*/1);
  manager.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/4,
                                   /*anchor_pos=*/4);
}

TEST(AutocorrectManagerTest, UndoAutocorrectSingleWordInComposition) {
  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::InputMethodAsh ime(nullptr);
  ui::IMEBridge::Get()->SetInputContextHandler(&ime);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ::testing::NiceMock<MockSuggestionHandler> mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  manager.OnSurroundingTextChanged(u"the ", /*cursor_pos=*/4,
                                   /*anchor_pos=*/4);
  manager.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Move cursor to the middle of 'the' and bring the text into composition.
  fake_text_input_client.SetTextAndSelection(u"the ", gfx::Range(2));
  ime.SetComposingRange(0, 3, {});

  manager.UndoAutocorrect();

  EXPECT_EQ(fake_text_input_client.text(), u"teh ");
}

TEST(AutocorrectManagerTest, UndoAutocorrectMultipleWordInComposition) {
  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::InputMethodAsh ime(nullptr);
  ui::IMEBridge::Get()->SetInputContextHandler(&ime);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  ::testing::NiceMock<MockSuggestionHandler> mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  manager.OnSurroundingTextChanged(u"hello world ", /*cursor_pos=*/12,
                                   /*anchor_pos=*/12);
  manager.HandleAutocorrect(gfx::Range(0, 11), u"helloworld", u"hello world");

  // Move cursor to the middle of 'hello' and bring the word into composition.
  fake_text_input_client.SetTextAndSelection(u"hello world ", gfx::Range(2));
  ime.SetComposingRange(0, 5, {});

  manager.UndoAutocorrect();

  EXPECT_EQ(fake_text_input_client.text(), u"helloworld ");
}

TEST(AutocorrectManagerTest, RecordVirtualKeyboardMetricsWhenVisible) {
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  auto keyboard_client = ChromeKeyboardControllerClient::CreateForTest();
  keyboard_client->set_keyboard_visible_for_test(true);
  base::HistogramTester histogram_tester;
  manager.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Autocorrect.Actions.VK",
      1 /*AutocorrectActions::kUnderlined*/, 1);
}

TEST(AutocorrectManagerTest,
     DoesNotRecordVirtualKeyboardMetricsWhenNotVisible) {
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  auto keyboard_client = ChromeKeyboardControllerClient::CreateForTest();
  keyboard_client->set_keyboard_visible_for_test(false);
  base::HistogramTester histogram_tester;
  manager.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Autocorrect.Actions.VK",
      1 /*AutocorrectActions::kUnderlined*/, 0);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
