// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/autocorrect_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_input_context_handler_interface.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to match ImeAutocorrectActions
// in enums.xml.
enum class AutocorrectActions {
  kWindowShown = 0,
  kUnderlined = 1,
  kMaxValue = kUnderlined,
};

void LogAssistiveAutocorrectAction(AutocorrectActions action) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Actions",
                                action);
}

}  // namespace

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

void LogAssistiveAutocorrectAction(AutocorrectActions action) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Actions",
                                action);
}

constexpr int kKeysUntilAutocorrectWindowHides = 4;

}  // namespace

AutocorrectManager::AutocorrectManager(InputMethodEngine* engine)
    : engine_(engine) {}

void AutocorrectManager::MarkAutocorrectRange(const std::string& corrected_word,
                                              const std::string& typed_word,
                                              int start_index) {
  // TODO(crbug/1111135): call setAutocorrectTime() (for metrics)
  // TODO(crbug/1111135): record metric (coverage)
  last_typed_word_ = typed_word;
  last_corrected_word_ = corrected_word;
  key_presses_until_underline_hide_ = kKeysUntilAutocorrectWindowHides;
  ClearUnderline();

  if (context_id_ != -1) {
    engine_->SetAutocorrectRange(base::UTF8ToUTF16(corrected_word), start_index,
                                 start_index + corrected_word.length());
    autocorrect_time_ = base::TimeTicks::Now();
    LogAssistiveAutocorrectAction(AutocorrectActions::kUnderlined);
  }
}

bool AutocorrectManager::OnKeyEvent(
    const InputMethodEngineBase::KeyboardEvent& event) {
  if (event.type != "keydown") {
    return false;
  }
  if (event.key == "Up" && window_visible) {
    std::string error;
    auto button = ui::ime::AssistiveWindowButton();
    button.id = ui::ime::ButtonId::kUndo;
    button.window_type = ui::ime::AssistiveWindowType::kUndoWindow;
    button.announce_string =
        l10n_util::GetStringFUTF8(IDS_SUGGESTION_AUTOCORRECT_UNDO_BUTTON,
                                  base::UTF8ToUTF16(last_typed_word_));
    engine_->SetButtonHighlighted(context_id_, button, true, &error);
    button_highlighted = true;
    return true;
  }
  if (event.key == "Enter" && window_visible && button_highlighted) {
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
  engine_->SetAutocorrectRange(/*autocorrect text=*/base::string16(),
                               /*start=*/0,
                               /*end=*/std::numeric_limits<uint32_t>::max());
  // TODO(b/171924347): expose engine->clearAutocorrectRange() and use it here.
}

void AutocorrectManager::OnSurroundingTextChanged(const base::string16& text,
                                                  const int cursor_pos,
                                                  const int anchpr_pos) {
  std::string error;
  const gfx::Range range = engine_->GetAutocorrectRange();
  if (!range.is_empty() && cursor_pos >= range.start() &&
      cursor_pos <= range.end()) {
    if (!window_visible) {
      chromeos::AssistiveWindowProperties properties;
      properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
      properties.visible = true;
      properties.announce_string = l10n_util::GetStringFUTF8(
          IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN,
          base::UTF8ToUTF16(last_typed_word_),
          base::UTF8ToUTF16(last_corrected_word_));
      window_visible = true;
      button_highlighted = false;
      engine_->SetAssistiveWindowProperties(context_id_, properties, &error);
      LogAssistiveAutocorrectAction(AutocorrectActions::kWindowShown);
    }
    key_presses_until_underline_hide_ = kKeysUntilAutocorrectWindowHides;
  } else if (window_visible) {
    chromeos::AssistiveWindowProperties properties;
    properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    properties.visible = false;
    window_visible = false;
    button_highlighted = false;
    engine_->SetAssistiveWindowProperties(context_id_, properties, &error);
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
  engine_->SetAssistiveWindowProperties(context_id_, properties, &error);

  const gfx::Range range = engine_->GetAutocorrectRange();
  const ui::SurroundingTextInfo surrounding_text =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetSurroundingTextInfo();
  // TODO(crbug/1111135): Can we get away with deleting less text?
  // This will not quite work properly if there is text actually highlighted,
  // and cursor is at end of the highlight block, but no easy way around it.
  // First delete everything before cursor.
  engine_->DeleteSurroundingText(
      context_id_, -static_cast<int>(surrounding_text.selection_range.start()),
      surrounding_text.surrounding_text.length(), &error);
  // Submit the text after the cursor in composition mode to leave the cursor at
  // the start
  engine_->SetComposition(
      context_id_,
      base::UTF16ToUTF8(surrounding_text.surrounding_text.substr(range.end()))
          .c_str(),
      /*selection_start=*/0, /*selection_end=*/0, /*cursor=*/0, /*segments=*/{},
      &error);
  engine_->FinishComposingText(context_id_, &error);
  // Insert the text before the cursor - now there should be the correct text
  // and the cursor position will not have changed.
  engine_->CommitText(
      context_id_,
      (base::UTF16ToUTF8(
           surrounding_text.surrounding_text.substr(0, range.start())) +
       last_typed_word_)
          .c_str(),
      &error);
  LogAssistiveAutocorrectAction(AutocorrectActions::kReverted);
  base::UmaHistogramMediumTimes("InputMethod.Assistive.Autocorrect.Delay",
                                (base::TimeTicks::Now() - autocorrect_time_));
}

}  // namespace chromeos
