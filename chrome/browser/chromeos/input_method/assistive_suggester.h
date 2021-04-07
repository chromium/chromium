// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/input_method/emoji_suggester.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_base.h"
#include "chrome/browser/chromeos/input_method/personal_info_suggester.h"
#include "chrome/browser/chromeos/input_method/suggester.h"
#include "chrome/browser/chromeos/input_method/suggestion_enums.h"

namespace chromeos {

// An agent to suggest assistive information when the user types, and adopt or
// dismiss the suggestion according to the user action.
class AssistiveSuggester {
 public:
  AssistiveSuggester(InputMethodEngine* engine, Profile* profile);

  bool IsAssistiveFeatureEnabled();

  // Called when a text field gains focus, and suggester starts working.
  void OnFocus(int context_id);

  // Called when a text field loses focus, and suggester stops working.
  void OnBlur();

  // Checks the text before cursor, emits metric if any assistive prefix is
  // matched.
  void RecordAssistiveMatchMetrics(const std::u16string& text,
                                   int cursor_pos,
                                   int anchor_pos);

  // Called when a surrounding text is changed.
  // Returns true if it changes the surrounding text, e.g. a suggestion is
  // generated or dismissed.
  bool OnSurroundingTextChanged(const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos);

  // Called when the user pressed a key.
  // Returns true if suggester handles the event and it should stop propagate.
  bool OnKeyEvent(const ui::KeyEvent& event);

  // Accepts the suggestion at a given index if a suggester is currently active.
  void AcceptSuggestion(size_t index);

  // Check if suggestion is being shown.
  bool IsSuggestionShown();

  // Captures any suggestions currently showing, if there are none then an empty
  // vector is returned.
  std::vector<std::u16string> GetSuggestions();

  EmojiSuggester* get_emoji_suggester_for_testing() {
    return &emoji_suggester_;
  }

 private:
  // Returns if any suggestion text should be displayed according to the
  // surrounding text information.
  bool Suggest(const std::u16string& text, int cursor_pos, int anchor_pos);

  void DismissSuggestion();

  bool IsAssistPersonalInfoEnabled();

  bool IsEmojiSuggestAdditionEnabled();

  void RecordAssistiveMatchMetricsForAction(AssistiveType action);

  // Only the first applicable reason in DisabledReason enum is returned.
  DisabledReason GetDisabledReasonForEmoji();

  // Only the first applicable reason in DisabledReason enum is returned.
  DisabledReason GetDisabledReasonForPersonalInfo();

  bool IsActionEnabled(AssistiveType action);

  Profile* profile_;
  PersonalInfoSuggester personal_info_suggester_;
  EmojiSuggester emoji_suggester_;

  // ID of the focused text field, 0 if none is focused.
  int context_id_ = -1;

  // The current suggester in use, nullptr means no suggestion is shown.
  Suggester* current_suggester_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
