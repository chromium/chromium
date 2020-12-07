// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_AUTOCORRECT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_AUTOCORRECT_MANAGER_H_

#include <string>

#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_base.h"

namespace chromeos {

// Implements functionality for chrome.input.ime.autocorrect() extension API.
// This function shows UI to indicate that autocorrect has happened and allows
// it to be undone easily.
// TODO(b/171920749): Add unit tests.
class AutocorrectManager {
 public:
  // Engine is used to interact with the text field, and is assumed to be
  // valid for the entire lifetime of the autocorrect manager.
  explicit AutocorrectManager(InputMethodEngine* engine);

  AutocorrectManager(const AutocorrectManager&) = delete;
  AutocorrectManager& operator=(const AutocorrectManager&) = delete;

  // Called by input method engine on autocorrect to initially show underline.
  // Needs to be called after the autocorrected text (corrected_word, offset by
  // start_index code points in SurroundingInfo) has been committed.
  void MarkAutocorrectRange(const std::string& corrected_word,
                            const std::string& typed_word,
                            int start_index);
  // To hide the underline after enough keypresses, this class intercepts
  // keystrokes. Returns whether the keypress has now been handled.
  bool OnKeyEvent(const InputMethodEngineBase::KeyboardEvent& event);
  // Indicates a new text field is focused, used to save context ID.
  void OnFocus(int context_id);
  // To show the undo window when cursor is in an autocorrected word, this class
  // is notified of surrounding text changes.
  void OnSurroundingTextChanged(const base::string16& text,
                                int cursor_pos,
                                int anchor_pos);
  void UndoAutocorrect();

 private:
  void ClearUnderline();

  int key_presses_until_underline_hide_ = 0;
  int context_id_ = -1;
  InputMethodEngine* const engine_;
  std::string last_typed_word_;
  std::string last_corrected_word_;
  bool window_visible = false;
  bool button_highlighted = false;
  base::TimeTicks autocorrect_time_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_AUTOCORRECT_MANAGER_H_:w
