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

TEST_F(AutocorrectManagerTest, HandleAutocorrectSetsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST_F(AutocorrectManagerTest, OnKeyEventHidesUnderlineAfterEnoughKeyPresses) {
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

TEST_F(AutocorrectManagerTest, RecordVirtualKeyboardMetricsWhenVisible) {
  keyboard_client_->set_keyboard_visible_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.Autocorrect.Actions.VK",
      1 /*AutocorrectActions::kUnderlined*/, 1);
}

TEST_F(AutocorrectManagerTest,
       DoesNotRecordVirtualKeyboardMetricsWhenNotVisible) {
  keyboard_client_->set_keyboard_visible_for_test(false);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.Autocorrect.Actions.VK",
      1 /*AutocorrectActions::kUnderlined*/, 0);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
