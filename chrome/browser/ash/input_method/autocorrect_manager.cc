// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_manager.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/ime_rules_config.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_input_context_handler_interface.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

namespace {

bool IsCurrentInputMethodExperimentalMultilingual() {
  auto* input_method_manager = InputMethodManager::Get();
  if (!input_method_manager) {
    return false;
  }
  return extension_ime_util::IsExperimentalMultilingual(
      input_method_manager->GetActiveIMEState()->GetCurrentInputMethod().id());
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

constexpr int kKeysUntilUnderlineHides = 4;
constexpr int kDistanceUntilUnderlineHides = 3;

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

  in_diacritical_autocorrect_session_ =
      IsCurrentInputMethodExperimentalMultilingual() &&
      diacritics_insensitive_string_comparator_.Equal(original_text,
                                                      current_text);

  original_text_ = original_text;
  key_presses_until_underline_hide_ = kKeysUntilUnderlineHides;
  if (!input_context->GetAutocorrectRange().is_empty()) {
    input_context->SetAutocorrectRange(gfx::Range());  // clear underline
    LogAssistiveAutocorrectAction(AutocorrectActions::kUserAcceptedAutocorrect);
  }

  input_context->SetAutocorrectRange(autocorrect_range);  // show underline
  LogAssistiveAutocorrectAction(AutocorrectActions::kUnderlined);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectUnderlined);
  autocorrect_time_ = base::TimeTicks::Now();
}

void AutocorrectManager::LogAssistiveAutocorrectAction(
    AutocorrectActions action) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Actions",
                                action);

  if (ChromeKeyboardControllerClient::HasInstance() &&
      ChromeKeyboardControllerClient::Get()->is_keyboard_visible()) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.Autocorrect.Actions.VK", action);
  }

  if (IsCurrentInputMethodExperimentalMultilingual()) {
    base::UmaHistogramEnumeration(
        "InputMethod.MultilingualExperiment.Autocorrect.Actions", action);

    if (in_diacritical_autocorrect_session_) {
      base::UmaHistogramEnumeration(
          "InputMethod.MultilingualExperiment.DiacriticalAutocorrect.Actions",
          action);
    }
  }
}

bool AutocorrectManager::OnKeyEvent(const ui::KeyEvent& event) {
  if (event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }
  if (event.code() == ui::DomCode::ARROW_UP && window_visible_) {
    std::string error;
    auto button = ui::ime::AssistiveWindowButton();
    button.id = ui::ime::ButtonId::kUndo;
    button.window_type = ui::ime::AssistiveWindowType::kUndoWindow;
    button.announce_string = l10n_util::GetStringFUTF16(
        IDS_SUGGESTION_AUTOCORRECT_UNDO_BUTTON, original_text_);
    suggestion_handler_->SetButtonHighlighted(context_id_, button, true,
                                              &error);
    button_highlighted_ = true;
    return true;
  }
  if (event.code() == ui::DomCode::ENTER && window_visible_ &&
      button_highlighted_) {
    UndoAutocorrect();
    return true;
  }
  if (key_presses_until_underline_hide_ >= 0) {
    --key_presses_until_underline_hide_;
  }

  if (key_presses_until_underline_hide_ == 0) {
    ui::IMEInputContextHandlerInterface* input_context =
        ui::IMEBridge::Get()->GetInputContextHandler();

    if (input_context && !input_context->GetAutocorrectRange().is_empty()) {
      input_context->SetAutocorrectRange(gfx::Range());  // clear underline
      LogAssistiveAutocorrectAction(
          AutocorrectActions::kUserAcceptedAutocorrect);
    } else {
      LogAssistiveAutocorrectAction(
          AutocorrectActions::kUserActionClearedUnderline);
    }
  }
  return false;
}

void AutocorrectManager::OnSurroundingTextChanged(const std::u16string& text,
                                                  const int cursor_pos,
                                                  const int anchor_pos) {
  std::string error;
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  const gfx::Range range = input_context->GetAutocorrectRange();
  const uint32_t cursor_pos_unsigned = base::checked_cast<uint32_t>(cursor_pos);
  if (!range.is_empty() &&
      (cursor_pos_unsigned + kDistanceUntilUnderlineHides < range.start() ||
       cursor_pos_unsigned - kDistanceUntilUnderlineHides > range.end())) {
    input_context->SetAutocorrectRange(gfx::Range());  // clear underline
    LogAssistiveAutocorrectAction(AutocorrectActions::kUserAcceptedAutocorrect);
  }
  // Explaination of checks:
  // 1) Check there is an autocorrect range
  // 2) Check cursor is in range
  // 3) Ensure there is no selection (selection UI clashes with autocorrect UI).
  if (!range.is_empty() && cursor_pos_unsigned >= range.start() &&
      cursor_pos_unsigned <= range.end() && cursor_pos == anchor_pos) {
    if (!window_visible_) {
      const std::u16string autocorrected_text =
          text.substr(range.start(), range.length());
      AssistiveWindowProperties properties;
      properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
      properties.visible = true;
      properties.announce_string = l10n_util::GetStringFUTF16(
          IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN, original_text_,
          autocorrected_text);
      window_visible_ = true;
      button_highlighted_ = false;
      suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                        &error);
      LogAssistiveAutocorrectAction(AutocorrectActions::kWindowShown);
      RecordAssistiveCoverage(AssistiveType::kAutocorrectWindowShown);
    }
    key_presses_until_underline_hide_ = kKeysUntilUnderlineHides;
  } else if (window_visible_) {
    AssistiveWindowProperties properties;
    properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    properties.visible = false;
    window_visible_ = false;
    button_highlighted_ = false;
    suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                      &error);
  }
}

void AutocorrectManager::OnFocus(int context_id) {
  if (base::FeatureList::IsEnabled(ash::features::kImeRuleConfig)) {
    GetTextFieldContextualInfo(
        base::BindOnce(&AutocorrectManager::OnTextFieldContextualInfoChanged,
                       base::Unretained(this)));
  }

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
  AssistiveWindowProperties properties;
  properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
  properties.visible = false;
  window_visible_ = false;
  button_highlighted_ = false;
  suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                    &error);

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  const gfx::Range autocorrect_range = input_context->GetAutocorrectRange();

  if (input_context->HasCompositionText()) {
    input_context->SetComposingRange(autocorrect_range.start(),
                                     autocorrect_range.end(), {});
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

    // Delete the autocorrected text.
    // This will not quite work properly if there is text actually highlighted,
    // and cursor is at end of the highlight block, but no easy way around it.
    // First delete everything before cursor.
    input_context->DeleteSurroundingText(
        autocorrect_range.start() - surrounding_text.selection_range.start(),
        autocorrect_range.length());

    // Replace with the original text.
    input_context->CommitText(
        original_text_,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  LogAssistiveAutocorrectAction(AutocorrectActions::kReverted);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectReverted);
  RecordAssistiveSuccess(AssistiveType::kAutocorrectReverted);
  LogAssistiveAutocorrectDelay(base::TimeTicks::Now() - autocorrect_time_);
}

void AutocorrectManager::OnTextFieldContextualInfoChanged(
    const TextFieldContextualInfo& info) {
  disabled_by_rule_ =
      ImeRulesConfig::GetInstance()->IsAutoCorrectDisabled(info);
}

bool AutocorrectManager::DisabledByRule() {
  return disabled_by_rule_;
}

}  // namespace input_method
}  // namespace ash
