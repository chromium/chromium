// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ash/input_method/assistive_window_controller.h"
#include "chrome/browser/ash/input_method/diacritics_insensitive_string_comparator.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"

namespace ash {
namespace input_method {

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
  // the `current_text` contents.
  // NOTE: Technically redundant to require client to supply `current_text` as
  // AutocorrectManager can retrieve it from current text editing state known to
  // IMF. However, due to async situation between browser-process IMF and
  // render-process TextInputClient, it may just get a stale value that way.
  // TODO(crbug/1194424): Remove technically redundant `current_text` param
  // to avoid situation with multiple conflicting sources of truth.
  void HandleAutocorrect(gfx::Range autocorrect_range,
                         const std::u16string& original_text,
                         const std::u16string& current_text);

  // To hide the underline after enough keypresses, this class intercepts
  // keystrokes. Returns whether the keypress has now been handled.
  bool OnKeyEvent(const ui::KeyEvent& event);

  // Indicates a new text field is focused, used to save context ID.
  void OnFocus(int context_id);

  // To show the undo window when cursor is in an autocorrected word, this class
  // is notified of surrounding text changes.
  void OnSurroundingTextChanged(const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos);

  void UndoAutocorrect();

  // Whether auto correction is disabled by some rule.
  bool DisabledByRule();

 private:
  void LogAssistiveAutocorrectAction(AutocorrectActions action);

  void OnTextFieldContextualInfoChanged(const TextFieldContextualInfo& info);

  // Forces to accept or clear a pending autocorrect suggestion if any. If the
  // autocorrect range is empty, it means the user interacted with the
  // pending autocorrect suggestion and made it invalid, so it considers
  // the autocorrect suggestion as "cleared". Otherwise, it considers the
  // autocorrect suggestion as "accepted". For the both cases, relevant
  // metrics are recorded, state variables are reset and autocorrect range is
  // set to empty.
  void AcceptOrClearPendingAutocorrect();

  // Hides undo window if there is any visible.
  void HideUndoWindow();

  // Shows undo window and record the relevant metric if undo window is
  // not already visible.
  void ShowUndoWindow(gfx::Range range, const std::u16string& text);

  // Highlights undo button of undo window if it is visible.
  void HighlightUndoButton();

  SuggestionHandlerInterface* suggestion_handler_;
  int context_id_ = 0;
  int key_presses_until_underline_hide_ = 0;
  std::u16string original_text_;
  bool window_visible_ = false;
  bool button_highlighted_ = false;
  base::TimeTicks autocorrect_time_;

  // Stores the state where there is a pending/unprocessed autocorrect
  // suggestion. The state is kept to avoid issue where InputContext returns
  // stale autocorrect range.
  bool autocorrect_pending_ = false;

  DiacriticsInsensitiveStringComparator
      diacritics_insensitive_string_comparator_;
  bool in_diacritical_autocorrect_session_ = false;

  bool disabled_by_rule_ = false;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_:w
