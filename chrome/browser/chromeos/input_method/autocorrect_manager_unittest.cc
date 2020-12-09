// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/autocorrect_manager.h"

#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
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
               const base::string16& candidate,
               std::string* error),
              (override));
  MOCK_METHOD(bool,
              SetAssistiveWindowProperties,
              (int context_id,
               const AssistiveWindowProperties& assistive_window,
               std::string* error),
              (override));
};

TEST(AutocorrectManagerTest, HandleAutocorrectSetsAutocorrectRange) {
  ui::IMEBridge::Initialize();
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  MockSuggestionHandler mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);

  manager.HandleAutocorrect(gfx::Range(0, 3), "teh");

  EXPECT_EQ(mock_ime_input_context_handler.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST(AutocorrectManagerTest, OnKeyEventHidesUnderlineAfterEnoughKeyPresses) {
  ui::IMEBridge::Initialize();
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  MockSuggestionHandler mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  manager.HandleAutocorrect(gfx::Range(0, 3), "teh");

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
  ui::IMEBridge::Initialize();
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  manager.OnSurroundingTextChanged(base::ASCIIToUTF16("the "), /*cursor_pos=*/4,
                                   /*anchor_pos=*/4);
  manager.HandleAutocorrect(gfx::Range(0, 3), "teh");

  AssistiveWindowProperties properties;
  properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
  properties.visible = true;
  properties.announce_string = l10n_util::GetStringFUTF8(
      IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN, base::ASCIIToUTF16("teh"),
      base::ASCIIToUTF16("the"));
  EXPECT_CALL(mock_suggestion_handler,
              SetAssistiveWindowProperties(_, properties, _));

  manager.OnSurroundingTextChanged(base::ASCIIToUTF16("the "), /*cursor_pos=*/1,
                                   /*anchor_pos=*/1);
}

TEST(AutocorrectManagerTest, MovingCursorOutsideRangeHidesAssistiveWindow) {
  ui::IMEBridge::Initialize();
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  AutocorrectManager manager(&mock_suggestion_handler);
  manager.OnSurroundingTextChanged(base::ASCIIToUTF16("the "), /*cursor_pos=*/4,
                                   /*anchor_pos=*/4);
  manager.HandleAutocorrect(gfx::Range(0, 3), "teh");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties;
    shown_properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    shown_properties.visible = true;
    shown_properties.announce_string = l10n_util::GetStringFUTF8(
        IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN, base::ASCIIToUTF16("teh"),
        base::ASCIIToUTF16("the"));
    EXPECT_CALL(mock_suggestion_handler,
                SetAssistiveWindowProperties(_, shown_properties, _));

    AssistiveWindowProperties hidden_properties;
    hidden_properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    hidden_properties.visible = false;
    EXPECT_CALL(mock_suggestion_handler,
                SetAssistiveWindowProperties(_, hidden_properties, _));
  }

  manager.OnSurroundingTextChanged(base::ASCIIToUTF16("the "), /*cursor_pos=*/1,
                                   /*anchor_pos=*/1);
  manager.OnSurroundingTextChanged(base::ASCIIToUTF16("the "), /*cursor_pos=*/4,
                                   /*anchor_pos=*/4);
}

// TODO(b/171920749): Add unit tests for UndoAutocorrect().

}  // namespace
}  // namespace chromeos
