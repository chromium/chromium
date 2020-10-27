// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/autocorrect_manager.h"

#include "base/strings/utf_string_conversions.h"

namespace chromeos {

const int kKeysUntilAutocorrectWindowHides = 4;

AutocorrectManager::AutocorrectManager(InputMethodEngineBase* engine)
    : engine_(engine) {}

void AutocorrectManager::MarkAutocorrectRange(const std::string& corrected_word,
                                              int start_index) {
  // TODO(crbug/1111135): call setAutocorrectTime() (for metrics)
  // TODO(crbug/1111135): record metric (coverage)
  key_presses_until_underline_hide_ = kKeysUntilAutocorrectWindowHides;
  ClearUnderline();
  if (context_id_ != -1) {
    std::string error;
    // TODO(crbug/1111135): error handling
    engine_->SetAutocorrectRange(context_id_, base::UTF8ToUTF16(corrected_word),
                                 start_index, start_index, &error);
  }
}

void AutocorrectManager::OnKeyEvent() {
  if (key_presses_until_underline_hide_ < 0) {
    return;
  }
  --key_presses_until_underline_hide_;
  if (key_presses_until_underline_hide_ < 1) {
    ClearUnderline();
  }
}

void AutocorrectManager::ClearUnderline() {
  // TODO(crbug/1111135): error handling
  std::string error;
  engine_->SetAutocorrectRange(
      context_id_,
      /*autocorrect_text=*/base::string16(),
      /*start=*/0, /*end=*/std::numeric_limits<uint32_t>::max(), &error);
}

void AutocorrectManager::OnFocus(int context_id) {
  context_id_ = context_id;
}

}  // namespace chromeos
