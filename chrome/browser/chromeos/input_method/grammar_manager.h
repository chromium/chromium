// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_MANAGER_H_

#include <string>

#include "base/timer/timer.h"
#include "chrome/browser/chromeos/input_method/grammar_service_client.h"
#include "chrome/browser/chromeos/input_method/suggestion_handler_interface.h"
#include "chrome/browser/chromeos/input_method/text_utils.h"
#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/events/event.h"

namespace chromeos {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to match ImeGrammarActions
// in enums.xml.
enum class GrammarActions {
  kUnderlined = 0,
  kWindowShown = 1,
  kAccepted = 2,
  kIgnored = 3,
  kMaxValue = kIgnored,
};

// Sends grammar check requests to ml service and upon receiving the grammar
// suggestions, shows UI to indiciate the suggestions and allows users to
// accept or dismiss the suggestions.
class GrammarManager {
 public:
  GrammarManager(Profile* profile,
                 std::unique_ptr<GrammarServiceClient> grammar_client,
                 SuggestionHandlerInterface* suggestion_handler);
  GrammarManager(const GrammarManager&) = delete;
  GrammarManager& operator=(const GrammarManager&) = delete;
  ~GrammarManager();

  // Check if the chromium flag for on-device grammar check is enabled.
  bool IsOnDeviceGrammarEnabled();

  // Indicates a new text field is focused, used to save context ID.
  void OnFocus(int context_id);

  // This class intercepts keystrokes when the grammar suggestion pop up is
  // displayed. Returns whether the keypress has been handled.
  bool OnKeyEvent(const ui::KeyEvent& event);

  // Sends grammar check request to ml service or display existing grammar
  // suggestion based on the surrounding text changes and cursor changes.
  void OnSurroundingTextChanged(const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos);

  // Dismisses the suggestion window and replaces the incorrect grammar fragment
  // with the suggestion.
  void AcceptSuggestion();

  void IgnoreSuggestion();

 private:
  void Check(const text_utils::Sentence& sentence);

  void OnGrammarCheckDone(
      const text_utils::Sentence& sentence,
      bool success,
      const std::vector<ui::GrammarFragment>& results) const;

  void DismissSuggestion();

  void SetButtonHighlighted(const ui::ime::AssistiveWindowButton& button);

  Profile* profile_;
  std::unique_ptr<GrammarServiceClient> grammar_client_;
  SuggestionHandlerInterface* suggestion_handler_;
  int context_id_ = 0;
  std::u16string last_text_;
  base::OneShotTimer delay_timer_;
  ui::GrammarFragment current_fragment_;
  const ui::ime::AssistiveWindowButton suggestion_button_;
  const ui::ime::AssistiveWindowButton ignore_button_;
  bool suggestion_shown_ = false;
  ui::ime::ButtonId highlighted_button_ = ui::ime::ButtonId::kNone;
  text_utils::Sentence last_sentence_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_MANAGER_H_
