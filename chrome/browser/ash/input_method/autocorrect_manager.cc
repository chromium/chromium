// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_manager.h"

#include "ash/constants/ash_features.h"
#include "base/callback_helpers.h"
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
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

namespace {

bool IsVkAutocorrect() {
  return ChromeKeyboardControllerClient::HasInstance() &&
         ChromeKeyboardControllerClient::Get()->is_keyboard_visible();
}

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

void LogAssistiveAutocorrectActionLatency(AutocorrectActions action,
                                          base::TimeDelta time_delta,
                                          bool virtual_keyboard_visible) {
  switch (action) {
    case AutocorrectActions::kUnderlined:
    case AutocorrectActions::kWindowShown:
      // Skip non-terminal actions.
      return;
    case AutocorrectActions::kUserAcceptedAutocorrect:
      base::UmaHistogramMediumTimes(
          "InputMethod.Assistive.AutocorrectV2.Latency.Accept", time_delta);
      break;
    case AutocorrectActions::kReverted:
    case AutocorrectActions::kUserActionClearedUnderline:
      base::UmaHistogramMediumTimes(
          "InputMethod.Assistive.AutocorrectV2.Latency.Reject", time_delta);
      break;
    case AutocorrectActions::kUserExitedTextFieldWithUnderline:
      base::UmaHistogramMediumTimes(
          "InputMethod.Assistive.AutocorrectV2.Latency.ExitField", time_delta);
      break;
    default:
      LOG(ERROR) << "Invalid AutocorrectActions: action=" << (int)action;
      return;
  }

  // Record the duration of the pending autocorrect for VK and PK.
  if (virtual_keyboard_visible) {
    base::UmaHistogramMediumTimes(
        "InputMethod.Assistive.AutocorrectV2.Latency.VkPending", time_delta);
  } else {
    base::UmaHistogramMediumTimes(
        "InputMethod.Assistive.AutocorrectV2.Latency.PkPending", time_delta);
  }
}

void LogAssistiveAutocorrectInternalState(
    AutocorrectInternalStates internal_state) {
  if (IsVkAutocorrect()) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.Internal.VkState", internal_state);
  } else {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.Internal.PkState", internal_state);
  }
}

void RecordAssistiveCoverage(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Coverage", type);
}

void RecordAssistiveSuccess(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Success", type);
}

bool CouldTriggerAutocorrectWithSurroundingText(const std::u16string& text,
                                                size_t cursor_pos,
                                                size_t anchor_pos) {
  // TODO(b/161490813): Do not count cases that autocorrect is disabled.
  //    Currently, there are different logics in different places that disable
  //    autocorrect based on settings, domain and text field attributes.
  //    Ideally, all the cases that autocorrect is disabled on a text field
  //    must not be counted here.
  return cursor_pos == anchor_pos && cursor_pos == text.size() &&
         text.size() >= 2 && base::IsAsciiWhitespace(text.back()) &&
         !base::IsAsciiWhitespace(text[text.size() - 2]);
}

bool IsAutocorrectSuggestionInSurroundingText(
    const std::u16string& surrounding_text,
    const gfx::Range& autocorrect_range,
    const std::u16string& suggested_text) {
  if (autocorrect_range.is_empty() ||
      suggested_text.length() != autocorrect_range.length() ||
      autocorrect_range.end() > surrounding_text.length()) {
    return false;
  }

  return surrounding_text.substr(autocorrect_range.start(),
                                 autocorrect_range.length()) == suggested_text;
}

constexpr int kDistanceUntilUnderlineHides = 3;
constexpr int kMaxValidationTries = 4;

}  // namespace

AutocorrectManager::AutocorrectManager(
    SuggestionHandlerInterface* suggestion_handler)
    : suggestion_handler_(suggestion_handler) {}

AutocorrectManager::~AutocorrectManager() = default;

void AutocorrectManager::HandleAutocorrect(const gfx::Range autocorrect_range,
                                           const std::u16string& original_text,
                                           const std::u16string& current_text) {
  ++num_handled_autocorrect_in_text_field_;

  if (DisabledByRule()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kHandleSuggestionInDenylistedApp);
  }

  // TODO(crbug/1111135): call setAutocorrectTime() (for metrics)
  // TODO(crbug/1111135): record metric (coverage)
  ui::TextInputTarget* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kHandleNoInputContext);
    AcceptOrClearPendingAutocorrect();
    return;
  }

  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kHandleUnclearedRange);
    AcceptOrClearPendingAutocorrect();
  }

  if (autocorrect_range.is_empty() ||
      autocorrect_range.length() != current_text.length() ||
      original_text.empty()) {
    if (autocorrect_range.is_empty()) {
      LogAssistiveAutocorrectInternalState(
          AutocorrectInternalStates::kHandleEmptyRange);
    } else {
      LogAssistiveAutocorrectInternalState(
          AutocorrectInternalStates::kHandleInvalidArgs);
    }
    input_context->SetAutocorrectRange(gfx::Range(), base::DoNothing());
    return;
  }

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kHandleSetRange);
  input_context->SetAutocorrectRange(
      autocorrect_range,
      base::BindOnce(&AutocorrectManager::ProcessSetAutocorrectRangeDone,
                     weak_ptr_factory_.GetWeakPtr(), autocorrect_range,
                     original_text, current_text));  // show underline
}

void AutocorrectManager::ProcessSetAutocorrectRangeDone(
    const gfx::Range& autocorrect_range,
    const std::u16string& original_text,
    const std::u16string& current_text,
    bool set_range_success) {
  if (!set_range_success) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kErrorSetRange);
    return;
  }

  in_diacritical_autocorrect_session_ =
      IsCurrentInputMethodExperimentalMultilingual() &&
      diacritics_insensitive_string_comparator_.Equal(original_text,
                                                      current_text);

  pending_autocorrect_ = AutocorrectManager::PendingAutocorrectState(
      /*original_text=*/original_text, /*suggested_text=*/current_text,
      /*start_time=*/base::TimeTicks::Now(),
      /*virtual_keyboard_visible=*/IsVkAutocorrect());

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kUnderlineShown);

  LogAssistiveAutocorrectAction(AutocorrectActions::kUnderlined);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectUnderlined);
}

void AutocorrectManager::LogAssistiveAutocorrectAction(
    AutocorrectActions action) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Actions",
                                action);

  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectActionLatency(
        action, base::TimeTicks::Now() - pending_autocorrect_->start_time,
        pending_autocorrect_->virtual_keyboard_visible);
  }

  if (pending_autocorrect_.has_value() &&
      pending_autocorrect_->virtual_keyboard_visible) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.Autocorrect.Actions.VK", action);
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.Actions.VK", action);
  } else {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.Actions.PK", action);
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

  if (CouldTriggerAutocorrectWithSurroundingText(text, cursor_pos,
                                                 anchor_pos)) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kCouldTriggerAutocorrect);
  }

  if (!pending_autocorrect_.has_value()) {
    return;
  }

  std::string error;
  ui::TextInputTarget* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();

  // Null input context invalidates the range so consider the pending
  // range as implicitly rejected/cleared.
  if (!input_context) {
    AcceptOrClearPendingAutocorrect();
    return;
  }

  if (!pending_autocorrect_->is_validated) {
    // Validate that the surrounding text matches with pending autocorrect
    // suggestion. Because of delays in update of surrounding text and
    // autocorrect range, the validation waits until all these information are
    // matching with each others (a.k.a. updated). This is necessary for
    // implementation of autocorrect interactions such as implicit acceptance.
    pending_autocorrect_->is_validated =
        IsAutocorrectSuggestionInSurroundingText(
            text, input_context->GetAutocorrectRange(),
            pending_autocorrect_->suggested_text);
    pending_autocorrect_->validation_tries++;

    if (!pending_autocorrect_->is_validated) {
      // Clear suggestion if multiple trials of validation fails.
      // This is a guard to prevent unwanted situation that can keep
      // autocorrect suggestion pending forever.
      if (pending_autocorrect_->validation_tries >= kMaxValidationTries) {
        AcceptOrClearPendingAutocorrect();
      }
      return;
    }
  }

  const gfx::Range range = input_context->GetAutocorrectRange();
  const uint32_t cursor_pos_unsigned
      = base::checked_cast<uint32_t>(cursor_pos);

  // If range is empty, it means user has mutated suggestion. So, clear range
  // and consider autocorrect suggestion as implicitly rejected.
  if (range.is_empty()) {
    AcceptOrClearPendingAutocorrect();
    return;
  }

  // If it is the first call of the event after handling autocorrect range,
  // initialize the variables and do not process the empty range as it is
  // potentially stale.
  if (pending_autocorrect_->num_inserted_chars < 0) {
    pending_autocorrect_->num_inserted_chars = 0;
  } else if (text.length() > pending_autocorrect_->text_length) {
    // TODO(b/161490813): Fix double counting of emojis and some CJK chars.
    // TODO(b/161490813): Fix logic for text replace.

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

  num_handled_autocorrect_in_text_field_ = 0;

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kOnFocusEvent);
  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kOnFocusEventWithPendingSuggestion);
  }

  context_id_ = context_id;
  ProcessTextFieldChange();
}

void AutocorrectManager::OnBlur() {
  LogAssistiveAutocorrectInternalState(AutocorrectInternalStates::kOnBlurEvent);

  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kOnBlurEventWithPendingSuggestion);
  }

  if (num_handled_autocorrect_in_text_field_ > 0) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kTextFieldEditsWithAtLeastOneSuggestion);
  }

  ProcessTextFieldChange();
}

void AutocorrectManager::ProcessTextFieldChange() {
  ui::TextInputTarget* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();

  // Clear autocorrect range if any.
  if (input_context) {
    HideUndoWindow();
    input_context->SetAutocorrectRange(gfx::Range(), base::DoNothing());
  }

  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectAction(
        AutocorrectActions::kUserExitedTextFieldWithUnderline);
    pending_autocorrect_.reset();
  }
}

void AutocorrectManager::UndoAutocorrect() {
  if (!pending_autocorrect_.has_value() ||
      !pending_autocorrect_->is_validated) {
    return;
  }

  HideUndoWindow();

  ui::TextInputTarget* input_context =
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
      !pending_autocorrect_->is_validated ||
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

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kShowUndoWindow);

  if (!error.empty()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kErrorShowUndoWindow);
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

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kHideUndoWindow);

  if (!error.empty()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kErrorHideUndoWindow);
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

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kHighlightUndoWindow);

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

  ui::TextInputTarget* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kSuggestionResolved);

  if (!pending_autocorrect_->is_validated) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kErrorRangeNotValidated);
    LogAssistiveAutocorrectAction(
        AutocorrectActions::kUserActionClearedUnderline);
  } else if (input_context &&
             !input_context->GetAutocorrectRange().is_empty()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kSuggestionAccepted);
    // Non-empty autocorrect range means that the user has not modified
    // autocorrect suggestion to invalidate it. So, it is considered as
    // accepted.
    LogAssistiveAutocorrectAction(
      AutocorrectActions::kUserAcceptedAutocorrect);
  } else {
    if (!input_context) {
      LogAssistiveAutocorrectInternalState(
          AutocorrectInternalStates::kNoInputContext);
    }
    LogAssistiveAutocorrectAction(
      AutocorrectActions::kUserActionClearedUnderline);
  }

  if (input_context) {
    input_context->SetAutocorrectRange(gfx::Range(),
                                       base::DoNothing());  // clear underline
  }

  HideUndoWindow();
  pending_autocorrect_.reset();
}

void AutocorrectManager::OnTextFieldContextualInfoChanged(
    const TextFieldContextualInfo& info) {
  disabled_by_rule_ =
      ImeRulesConfig::GetInstance()->IsAutoCorrectDisabled(info);
  if (disabled_by_rule_) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kAppIsInDenylist);
  }
}

bool AutocorrectManager::DisabledByRule() {
  return disabled_by_rule_;
}

AutocorrectManager::PendingAutocorrectState::PendingAutocorrectState(
    const std::u16string& original_text,
    const std::u16string& suggested_text,
    const base::TimeTicks& start_time,
    bool virtual_keyboard_visible)
    : original_text(original_text),
      suggested_text(suggested_text),
      start_time(start_time),
      virtual_keyboard_visible(virtual_keyboard_visible) {}

AutocorrectManager::PendingAutocorrectState::PendingAutocorrectState(
  const PendingAutocorrectState& other) = default;

AutocorrectManager::PendingAutocorrectState::~PendingAutocorrectState() =
    default;

}  // namespace input_method
}  // namespace ash
