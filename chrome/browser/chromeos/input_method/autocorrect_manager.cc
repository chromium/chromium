// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/autocorrect_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "chrome/browser/chromeos/input_method/suggestion_enums.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_input_context_handler_interface.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to match ImeAutocorrectActions
// in enums.xml.
enum class AutocorrectActions {
  kWindowShown = 0,
  kUnderlined = 1,
  kReverted = 2,
  kUserAcceptedAutocorrect = 3,
  kUserActionClearedUnderline = 4,
  kUserExitedTextFieldWithUnderline = 5,
  kMaxValue = kUserExitedTextFieldWithUnderline,
};

bool IsCurrentInputMethodExperimentalMultilingual() {
  auto* input_method_manager = input_method::InputMethodManager::Get();
  if (!input_method_manager) {
    return false;
  }
  return extension_ime_util::IsExperimentalMultilingual(
      input_method_manager->GetActiveIMEState()->GetCurrentInputMethod().id());
}

void LogAssistiveAutocorrectAction(AutocorrectActions action) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Actions",
                                action);
  if (IsCurrentInputMethodExperimentalMultilingual()) {
    base::UmaHistogramEnumeration(
        "InputMethod.MultilingualExperiment.Autocorrect.Actions", action);
  }
}

void LogAssistiveAutocorrectDelay(base::TimeDelta delay) {
  base::UmaHistogramMediumTimes("InputMethod.Assistive.Autocorrect.Delay",
                                delay);
  if (IsCurrentInputMethodExperimentalMultilingual()) {
    base::UmaHistogramMediumTimes(
        "InputMethod.MultilingualExperiment.Autocorrect.Delay", delay);
  }
}

void RecordAssistiveCoverage(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Coverage", type);
}

void RecordAssistiveSuccess(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Success", type);
}

constexpr int kKeysUntilAutocorrectWindowHides = 4;

}  // namespace

AutocorrectManager::AutocorrectManager(
    SuggestionHandlerInterface* suggestion_handler)
    : suggestion_handler_(suggestion_handler) {}

void AutocorrectManager::HandleAutocorrect(const gfx::Range autocorrect_range,
                                           const std::u16string& original_text,
                                           const std::u16string& current_text) {
  // TODO(crbug/1111135): call setAutocorrectTime() (for metrics)
  // TODO(crbug/1111135): record metric (coverage)
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  // TODO(crbug/1159297): Record diacritics-related metrics for multilingual
  // experiment, based on `current_text` and `original_text`.

  original_text_ = original_text;
  key_presses_until_underline_hide_ = kKeysUntilAutocorrectWindowHides;
  if (!input_context->GetAutocorrectRange().is_empty()) {
    ClearUnderline();
  }

  input_context->SetAutocorrectRange(autocorrect_range);
  LogAssistiveAutocorrectAction(AutocorrectActions::kUnderlined);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectUnderlined);
  autocorrect_time_ = base::TimeTicks::Now();
}

bool AutocorrectManager::OnKeyEvent(const ui::KeyEvent& event) {
  if (event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }
  if (event.code() == ui::DomCode::ARROW_UP && window_visible) {
    std::string error;
    auto button = ui::ime::AssistiveWindowButton();
    button.id = ui::ime::ButtonId::kUndo;
    button.window_type = ui::ime::AssistiveWindowType::kUndoWindow;
    button.announce_string = l10n_util::GetStringFUTF8(
        IDS_SUGGESTION_AUTOCORRECT_UNDO_BUTTON, original_text_);
    suggestion_handler_->SetButtonHighlighted(context_id_, button, true,
                                              &error);
    button_highlighted = true;
    return true;
  }
  if (event.code() == ui::DomCode::ENTER && window_visible &&
      button_highlighted) {
    UndoAutocorrect();
    return true;
  }
  if (key_presses_until_underline_hide_ >= 0) {
    --key_presses_until_underline_hide_;
  }
  if (key_presses_until_underline_hide_ == 0) {
    ClearUnderline();
  }
  return false;
}

void AutocorrectManager::ClearUnderline() {
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (input_context && !input_context->GetAutocorrectRange().is_empty()) {
    input_context->SetAutocorrectRange(gfx::Range());
    LogAssistiveAutocorrectAction(AutocorrectActions::kUserAcceptedAutocorrect);
  } else {
    LogAssistiveAutocorrectAction(
        AutocorrectActions::kUserActionClearedUnderline);
  }
}

void AutocorrectManager::OnSurroundingTextChanged(const std::u16string& text,
                                                  const int cursor_pos,
                                                  const int anchpr_pos) {
  std::string error;
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  const gfx::Range range = input_context->GetAutocorrectRange();
  if (!range.is_empty() && cursor_pos >= range.start() &&
      cursor_pos <= range.end()) {
    if (!window_visible) {
      const std::u16string autocorrected_text =
          text.substr(range.start(), range.length());
      chromeos::AssistiveWindowProperties properties;
      properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
      properties.visible = true;
      properties.announce_string = l10n_util::GetStringFUTF8(
          IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN, original_text_,
          autocorrected_text);
      window_visible = true;
      button_highlighted = false;
      suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                        &error);
      LogAssistiveAutocorrectAction(AutocorrectActions::kWindowShown);
      RecordAssistiveCoverage(AssistiveType::kAutocorrectWindowShown);
    }
    key_presses_until_underline_hide_ = kKeysUntilAutocorrectWindowHides;
  } else if (window_visible) {
    chromeos::AssistiveWindowProperties properties;
    properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    properties.visible = false;
    window_visible = false;
    button_highlighted = false;
    suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                      &error);
  }
}

void AutocorrectManager::OnFocus(int context_id) {
  if (key_presses_until_underline_hide_ > 0) {
    // TODO(b/149796494): move this to onblur()
    LogAssistiveAutocorrectAction(
        AutocorrectActions::kUserExitedTextFieldWithUnderline);
    key_presses_until_underline_hide_ = -1;
  }
  context_id_ = context_id;
}

void AutocorrectManager::UndoAutocorrect() {
  // TODO(crbug/1111135): error handling and metrics
  std::string error;
  chromeos::AssistiveWindowProperties properties;
  properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
  properties.visible = false;
  window_visible = false;
  button_highlighted = false;
  window_visible = false;
  suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                    &error);

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  const gfx::Range range = input_context->GetAutocorrectRange();

  if (input_context->HasCompositionText()) {
    input_context->SetComposingRange(range.start(), range.end(), {});
    input_context->CommitText(
        original_text_,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  } else {
    // NOTE: GetSurroundingTextInfo() could return a stale cache that no longer
    // reflects reality, due to async-ness between IMF and TextInputClient.
    // TODO(crbug/1194424): Work around the issue or fix
    // GetSurroundingTextInfo().
    const ui::SurroundingTextInfo surrounding_text =
        input_context->GetSurroundingTextInfo();

    // TODO(crbug/1111135): Can we get away with deleting less text?
    // This will not quite work properly if there is text actually highlighted,
    // and cursor is at end of the highlight block, but no easy way around it.
    // First delete everything before cursor.
    input_context->DeleteSurroundingText(
        -static_cast<int>(surrounding_text.selection_range.start()),
        surrounding_text.surrounding_text.length());

    // Submit the text after the cursor in composition mode to leave the cursor
    // at the start
    ui::CompositionText composition_text;
    composition_text.text =
        surrounding_text.surrounding_text.substr(range.end());
    input_context->UpdateCompositionText(composition_text,
                                         /*cursor_pos=*/0, /*visible=*/true);
    input_context->ConfirmCompositionText(/*reset_engine=*/false,
                                          /*keep_selection=*/true);

    // Insert the text before the cursor - now there should be the correct text
    // and the cursor position will not have changed.
    input_context->CommitText(
        surrounding_text.surrounding_text.substr(0, range.start()) +
            original_text_,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  LogAssistiveAutocorrectAction(AutocorrectActions::kReverted);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectReverted);
  RecordAssistiveSuccess(AssistiveType::kAutocorrectReverted);
  LogAssistiveAutocorrectDelay(base::TimeTicks::Now() - autocorrect_time_);
}

}  // namespace chromeos
