// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_GRAMMAR_MANAGER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_GRAMMAR_MANAGER_H_

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/input_method/grammar_service_client.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/text_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/events/event.h"

namespace ash {
namespace input_method {

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
  void OnFocus(int context_id,
               SpellcheckMode spellcheck_mode = SpellcheckMode::kUnspecified);

  // This class intercepts keystrokes when the grammar suggestion pop up is
  // displayed. Returns whether the keypress has been handled.
  bool OnKeyEvent(const ui::KeyEvent& event);

  // Sends grammar check request to ml service or display existing grammar
  // suggestion based on the surrounding text changes and cursor changes.
  void OnSurroundingTextChanged(const std::u16string& text,
                                gfx::Range selection_range);

  // Dismisses the suggestion window and replaces the incorrect grammar fragment
  // with the suggestion.
  void AcceptSuggestion();

  void IgnoreSuggestion();

 private:
  // Sends grammar check request to ml service or display existing grammar
  // suggestion based on the surrounding text changes and cursor changes.
  // Returns true is grammar suggestion window should show.
  bool HandleSurroundingTextChange(const std::u16string& text,
                                   gfx::Range selection_range);

  void Check(const Sentence& sentence);

  void OnGrammarCheckDone(const Sentence& sentence,
                          bool success,
                          const std::vector<ui::GrammarFragment>& results);

  void DismissSuggestion();

  void SetButtonHighlighted(const ui::ime::AssistiveWindowButton& button,
                            bool highlighted);

  raw_ptr<Profile> profile_;
  std::unique_ptr<GrammarServiceClient> grammar_client_;
  raw_ptr<SuggestionHandlerInterface> suggestion_handler_;
  int context_id_ = 0;
  bool new_to_context_ = true;
  std::u16string current_text_;
  base::OneShotTimer delay_timer_;
  ui::GrammarFragment current_fragment_;
  ui::ime::AssistiveWindowButton suggestion_button_;
  const ui::ime::AssistiveWindowButton ignore_button_;
  bool suggestion_shown_ = false;
  ui::ime::ButtonId highlighted_button_ = ui::ime::ButtonId::kNone;
  Sentence current_sentence_;
  Sentence last_sentence_;
  SpellcheckMode spellcheck_mode_ = SpellcheckMode::kUnspecified;
  std::unordered_map<std::u16string, std::unordered_set<uint64_t>>
      ignored_marker_hashes_;
  std::unordered_set<uint64_t> recorded_marker_hashes_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_GRAMMAR_MANAGER_H_
