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
  kMaxValue = kReverted,
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

void AutocorrectManager::HandleAutocorrect(gfx::Range autocorrect_range,
                                           const std::string& original_text) {
  // TODO(crbug/1111135): call setAutocorrectTime() (for metrics)
  // TODO(crbug/1111135): record metric (coverage)
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  original_text_ = original_text;
  key_presses_until_underline_hide_ = kKeysUntilAutocorrectWindowHides;
  ClearUnderline();

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
    button.announce_string =
        l10n_util::GetStringFUTF8(IDS_SUGGESTION_AUTOCORRECT_UNDO_BUTTON,
                                  base::UTF8ToUTF16(original_text_));
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
  if (key_presses_until_underline_hide_ > 0) {
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
  if (input_context) {
    input_context->SetAutocorrectRange(gfx::Range());
  }
}

void AutocorrectManager::OnSurroundingTextChanged(const base::string16& text,
                                                  const int cursor_pos,
                                                  const int anchpr_pos) {
  std::string error;
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  const gfx::Range range = input_context->GetAutocorrectRange();
  if (!range.is_empty() && cursor_pos >= range.start() &&
      cursor_pos <= range.end()) {
    if (!window_visible) {
      const std::string autocorrected_text =
          base::UTF16ToUTF8(text.substr(range.start(), range.length()));
      chromeos::AssistiveWindowProperties properties;
      properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
      properties.visible = true;
      properties.announce_string = l10n_util::GetStringFUTF8(
          IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN,
          base::UTF8ToUTF16(original_text_),
          base::UTF8ToUTF16(autocorrected_text));
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
  const ui::SurroundingTextInfo surrounding_text =
      input_context->GetSurroundingTextInfo();

  // TODO(crbug/1111135): Can we get away with deleting less text?
  // This will not quite work properly if there is text actually highlighted,
  // and cursor is at end of the highlight block, but no easy way around it.
  // First delete everything before cursor.
  input_context->DeleteSurroundingText(
      -static_cast<int>(surrounding_text.selection_range.start()),
      surrounding_text.surrounding_text.length());

  // Submit the text after the cursor in composition mode to leave the cursor at
  // the start
  ui::CompositionText composition_text;
  composition_text.text = surrounding_text.surrounding_text.substr(range.end());
  input_context->UpdateCompositionText(composition_text,
                                       /*cursor_pos=*/0, /*visible=*/true);
  input_context->ConfirmCompositionText(/*reset_engine=*/false,
                                        /*keep_selection=*/true);

  // Insert the text before the cursor - now there should be the correct text
  // and the cursor position will not have changed.
  input_context->CommitText(
      (base::UTF16ToUTF8(
           surrounding_text.surrounding_text.substr(0, range.start())) +
       original_text_));
  LogAssistiveAutocorrectAction(AutocorrectActions::kReverted);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectReverted);
  RecordAssistiveSuccess(AssistiveType::kAutocorrectReverted);
  LogAssistiveAutocorrectDelay(base::TimeTicks::Now() - autocorrect_time_);
}

}  // namespace chromeos
