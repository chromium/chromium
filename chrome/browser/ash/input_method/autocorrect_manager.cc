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

constexpr int kDistanceUntilUnderlineHides = 3;

}  // namespace

AutocorrectManager::AutocorrectManager(
    SuggestionHandlerInterface* suggestion_handler)
    : suggestion_handler_(suggestion_handler) {}

AutocorrectManager::~AutocorrectManager() = default;

void AutocorrectManager::HandleAutocorrect(const gfx::Range autocorrect_range,
                                           const std::u16string& original_text,
                                           const std::u16string& current_text) {
  // TODO(crbug/1111135): call setAutocorrectTime() (for metrics)
  // TODO(crbug/1111135): record metric (coverage)
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    AcceptOrClearPendingAutocorrect();
    return;
  }

  in_diacritical_autocorrect_session_ =
      IsCurrentInputMethodExperimentalMultilingual() &&
      diacritics_insensitive_string_comparator_.Equal(original_text,
                                                      current_text);

  if (pending_autocorrect_.has_value()) {
    AcceptOrClearPendingAutocorrect();
  }

  input_context->SetAutocorrectRange(autocorrect_range);  // show underline

  if (autocorrect_range.is_empty()) {
    return;
  }

  bool virtual_keyboard_visible =
      ChromeKeyboardControllerClient::HasInstance() &&
      ChromeKeyboardControllerClient::Get()->is_keyboard_visible();

  pending_autocorrect_ = AutocorrectManager::PendingAutocorrectState(
      /*original_text=*/original_text, /*start_time=*/base::TimeTicks::Now(),
      /*virtual_keyboard_visible=*/virtual_keyboard_visible);

  LogAssistiveAutocorrectAction(AutocorrectActions::kUnderlined);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectUnderlined);
}

void AutocorrectManager::LogAssistiveAutocorrectAction(
    AutocorrectActions action) {
  // TODO(b/161490813): Add a new metric to measure the impact of new changes.
  //   The new metric should have separate buckets for vk and pk.
  base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Actions",
                                action);

  if (pending_autocorrect_.has_value() &&
      pending_autocorrect_->virtual_keyboard_visible) {
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
  // OnKeyEvent is only used for interacting with the undo UI.
  if (!pending_autocorrect_.has_value() ||
      !pending_autocorrect_->undo_window_visible ||
      event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }

  if (event.code() == ui::DomCode::ARROW_UP) {
    HighlightUndoButton();
    return true;
  }
  if (event.code() == ui::DomCode::ENTER &&
      pending_autocorrect_->undo_button_highlighted) {
    UndoAutocorrect();
    return true;
  }

  return false;
}

void AutocorrectManager::OnSurroundingTextChanged(const std::u16string& text,
                                                  const int cursor_pos,
                                                  const int anchor_pos) {
  if (error_on_hiding_undo_window_) {
    HideUndoWindow();
  }
  if (!pending_autocorrect_.has_value()) {
    return;
  }

  std::string error;
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();

  // Null input context invalidates the range so consider the pending
  // range as implicitly rejected/cleared.
  if (!input_context) {
    AcceptOrClearPendingAutocorrect();
    return;
  }

  const gfx::Range range = input_context->GetAutocorrectRange();
  const uint32_t cursor_pos_unsigned
      = base::checked_cast<uint32_t>(cursor_pos);

  // If it is the first call of the event after handling autocorrect range,
  // initialize the variables and do not process the empty range as it is
  // potentially stale.
  if (pending_autocorrect_->num_inserted_chars < 0) {
    pending_autocorrect_->num_inserted_chars = 0;
  } else if (range.is_empty()) {
    // If it is not the first call and the range is empty, then it means the
    // user interaction has cleared the range.
    AcceptOrClearPendingAutocorrect();
    return;
  } else if (text.length() > pending_autocorrect_->text_length) {
    // TODO(b/161490813): Fix double counting of emojis and some CJK chars.

    // Count characters added between two calls of the event.
    pending_autocorrect_->num_inserted_chars += text.length() -
        pending_autocorrect_->text_length;
  }
  pending_autocorrect_->text_length = text.length();

  // If the number of added characters after setting the pending range is above
  // the threshold, then accept the pending range.
  if (pending_autocorrect_->num_inserted_chars >=
      kDistanceUntilUnderlineHides) {
    AcceptOrClearPendingAutocorrect();
    return;
  }

  // If cursor is inside autocorrect range (inclusive), show undo window and
  // record relevant metrics.
  if (cursor_pos_unsigned >= range.start() &&
      cursor_pos_unsigned <= range.end() && cursor_pos == anchor_pos) {
    ShowUndoWindow(range, text);
  } else {
    // Ensure undo window is hidden when cursor is not inside the autocorrect
    // range.
    HideUndoWindow();
  }
}

void AutocorrectManager::OnFocus(int context_id) {
  if (base::FeatureList::IsEnabled(ash::features::kImeRuleConfig)) {
    GetTextFieldContextualInfo(
        base::BindOnce(&AutocorrectManager::OnTextFieldContextualInfoChanged,
                       base::Unretained(this)));
  }

  context_id_ = context_id;
  ProcessTextFieldChange();
}

void AutocorrectManager::OnBlur() {
  ProcessTextFieldChange();
}

void AutocorrectManager::ProcessTextFieldChange() {
  ui::IMEInputContextHandlerInterface* input_context =
    ui::IMEBridge::Get()->GetInputContextHandler();

  // Clear autocorrect range if any.
  if (input_context) {
    HideUndoWindow();
    input_context->SetAutocorrectRange(gfx::Range());
  }

  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectAction(
        AutocorrectActions::kUserExitedTextFieldWithUnderline);
    pending_autocorrect_.reset();
  }
}

void AutocorrectManager::UndoAutocorrect() {
  if (!pending_autocorrect_.has_value()) {
    return;
  }

  HideUndoWindow();

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  const gfx::Range autocorrect_range = input_context->GetAutocorrectRange();

  if (input_context->HasCompositionText()) {
    input_context->SetComposingRange(autocorrect_range.start(),
                                     autocorrect_range.end(), {});
    input_context->CommitText(
        pending_autocorrect_->original_text,
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
        pending_autocorrect_->original_text,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  LogAssistiveAutocorrectAction(AutocorrectActions::kReverted);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectReverted);
  RecordAssistiveSuccess(AssistiveType::kAutocorrectReverted);
  LogAssistiveAutocorrectDelay(
    base::TimeTicks::Now() - pending_autocorrect_->start_time);

  pending_autocorrect_.reset();
}

void AutocorrectManager::ShowUndoWindow(
  gfx::Range range, const std::u16string& text) {
  if (!pending_autocorrect_.has_value() ||
      pending_autocorrect_->undo_window_visible) {
    return;
  }

  std::string error;
  const std::u16string autocorrected_text =
      text.substr(range.start(), range.length());
  AssistiveWindowProperties properties;
  properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
  properties.visible = true;
  properties.announce_string = l10n_util::GetStringFUTF16(
      IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN,
      pending_autocorrect_->original_text,
      autocorrected_text);
  suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                    &error);

  if (!error.empty()) {
    LOG(ERROR) << "Failed to show autocorrect undo window.";
    return;
  }

  // Showing a new undo window overrides the current shown undo window. So
  // there is no need to first trying to hide the previous one.
  error_on_hiding_undo_window_ = false;

  if (!pending_autocorrect_->window_shown_logged) {
    LogAssistiveAutocorrectAction(AutocorrectActions::kWindowShown);
    RecordAssistiveCoverage(AssistiveType::kAutocorrectWindowShown);
    pending_autocorrect_->window_shown_logged = true;
  }

  pending_autocorrect_->undo_button_highlighted = false;
  pending_autocorrect_->undo_window_visible = true;
}

void AutocorrectManager::HideUndoWindow() {
  if (!error_on_hiding_undo_window_ &&
      (!pending_autocorrect_.has_value() ||
       !pending_autocorrect_->undo_window_visible)) {
    return;
  }

  std::string error;
  AssistiveWindowProperties properties;
  properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
  properties.visible = false;
  suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                    &error);

  if (!error.empty()) {
    LOG(ERROR) << "Failed to hide autocorrect undo window.";
    error_on_hiding_undo_window_ = true;
    return;
  }

  error_on_hiding_undo_window_ = false;

  if (pending_autocorrect_.has_value()) {
    pending_autocorrect_->undo_button_highlighted = false;
    pending_autocorrect_->undo_window_visible = false;
  }
}

void AutocorrectManager::HighlightUndoButton() {
  if (!pending_autocorrect_.has_value() ||
      !pending_autocorrect_->undo_window_visible ||
      pending_autocorrect_->undo_button_highlighted) {
    return;
  }

  std::string error;
  ui::ime::AssistiveWindowButton button = ui::ime::AssistiveWindowButton();
  button.id = ui::ime::ButtonId::kUndo;
  button.window_type = ui::ime::AssistiveWindowType::kUndoWindow;
  button.announce_string = l10n_util::GetStringFUTF16(
      IDS_SUGGESTION_AUTOCORRECT_UNDO_BUTTON,
      pending_autocorrect_->original_text);
  suggestion_handler_->SetButtonHighlighted(context_id_, button, true,
                                            &error);

  if (!error.empty()) {
    LOG(ERROR) << "Failed to highlight undo button.";
    return;
  }

  pending_autocorrect_->undo_button_highlighted = true;
}

void AutocorrectManager::AcceptOrClearPendingAutocorrect() {
  if (!pending_autocorrect_.has_value()) {
    return;
  }

  // TODO(b/161490813): Record delay metric.
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();

  // Non-empty autocorrect range means that the user has not modified
  // autocorrect suggestion to invalidate it. So, it is considered as accepted.
  if (input_context && !input_context->GetAutocorrectRange().is_empty()) {
    input_context->SetAutocorrectRange(gfx::Range()); // clear underline
    LogAssistiveAutocorrectAction(
      AutocorrectActions::kUserAcceptedAutocorrect);
  } else {
    LogAssistiveAutocorrectAction(
      AutocorrectActions::kUserActionClearedUnderline);
  }
  HideUndoWindow();
  pending_autocorrect_.reset();
}

void AutocorrectManager::OnTextFieldContextualInfoChanged(
    const TextFieldContextualInfo& info) {
  disabled_by_rule_ =
      ImeRulesConfig::GetInstance()->IsAutoCorrectDisabled(info);
}

bool AutocorrectManager::DisabledByRule() {
  return disabled_by_rule_;
}

AutocorrectManager::PendingAutocorrectState::PendingAutocorrectState(
    const std::u16string& original_text,
    const base::TimeTicks& start_time,
    bool virtual_keyboard_visible)
    : original_text(original_text),
      start_time(start_time),
      virtual_keyboard_visible(virtual_keyboard_visible) {}

AutocorrectManager::PendingAutocorrectState::PendingAutocorrectState(
  const PendingAutocorrectState& other) = default;

AutocorrectManager::PendingAutocorrectState::~PendingAutocorrectState() =
    default;

}  // namespace input_method
}  // namespace ash
