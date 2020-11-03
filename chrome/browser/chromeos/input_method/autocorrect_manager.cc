// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/autocorrect_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"

namespace chromeos {

constexpr int kKeysUntilAutocorrectWindowHides = 4;

AutocorrectManager::AutocorrectManager(InputMethodEngine* engine)
    : engine_(engine) {}

void AutocorrectManager::MarkAutocorrectRange(const std::string& corrected_word,
                                              int start_index) {
  // TODO(crbug/1111135): call setAutocorrectTime() (for metrics)
  // TODO(crbug/1111135): record metric (coverage)
  key_presses_until_underline_hide_ = kKeysUntilAutocorrectWindowHides;
  ClearUnderline();
  if (context_id_ != -1) {
    engine_->SetAutocorrectRange(base::UTF8ToUTF16(corrected_word), start_index,
                                 start_index + corrected_word.length());
  }
}

void AutocorrectManager::OnKeyEvent(
    const InputMethodEngineBase::KeyboardEvent& event) {
  if (event.type != "keydown") {
    return;
  }
  if (key_presses_until_underline_hide_ > 0) {
    --key_presses_until_underline_hide_;
  }
  if (key_presses_until_underline_hide_ == 0) {
    ClearUnderline();
  }
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
    chromeos::AssistiveWindowProperties properties;
    properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    properties.visible = true;

    engine_->SetAssistiveWindowProperties(context_id_, properties, &error);
    key_presses_until_underline_hide_ = kKeysUntilAutocorrectWindowHides;
  } else {
    chromeos::AssistiveWindowProperties properties;
    properties.type = ui::ime::AssistiveWindowType::kUndoWindow;
    properties.visible = false;

    engine_->SetAssistiveWindowProperties(context_id_, properties, &error);
  }
}

void AutocorrectManager::OnFocus(int context_id) {
  context_id_ = context_id;
}

}  // namespace chromeos
