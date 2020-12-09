// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_AUTOCORRECT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_AUTOCORRECT_MANAGER_H_

#include <string>

#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_base.h"
#include "chrome/browser/chromeos/input_method/suggestion_handler_interface.h"

namespace chromeos {

// Implements functionality for chrome.input.ime.autocorrect() extension API.
// This function shows UI to indicate that autocorrect has happened and allows
// it to be undone easily.
class AutocorrectManager {
 public:
  // `suggestion_handler_` must be alive for the duration of the lifetime of
  // this instance.
  explicit AutocorrectManager(SuggestionHandlerInterface* suggestion_handler);

  AutocorrectManager(const AutocorrectManager&) = delete;
  AutocorrectManager& operator=(const AutocorrectManager&) = delete;

  // Mark `autocorrect_range` with an underline. `autocorrect_range` is based on
  // the current text contents.
  void HandleAutocorrect(gfx::Range autocorrect_range,
                         const std::string& original_text);

  // To hide the underline after enough keypresses, this class intercepts
  // keystrokes. Returns whether the keypress has now been handled.
  bool OnKeyEvent(const ui::KeyEvent& event);

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

  SuggestionHandlerInterface* suggestion_handler_;
  int context_id_ = 0;
  int key_presses_until_underline_hide_ = 0;
  std::string original_text_;
  bool window_visible = false;
  bool button_highlighted = false;
  base::TimeTicks autocorrect_time_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_AUTOCORRECT_MANAGER_H_:w
