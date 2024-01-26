// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTER_H_

#include <string>

#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestions_source.h"

namespace ash {
namespace input_method {

// A generic agent to suggest when the user types, and adopt or dismiss the
// suggestion according to the user action.
class Suggester : public SuggestionsSource {
 public:
  // Called when a text field gains focus, and suggester starts working.
  virtual void OnFocus(int context_id) = 0;

  // Called when a text field loses focus, and suggester stops working.
  virtual void OnBlur() = 0;

  // Called when suggestions are generated outside of the assisitve framework.
  virtual void OnExternalSuggestionsUpdated(
      const std::vector<ime::AssistiveSuggestion>& suggestions,
      const std::optional<ime::SuggestionsTextContext>& context) = 0;

  // Called when suggestion is being shown.
  // Returns SuggestionStatus as suggester handles the event.
  virtual SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) = 0;

  // Attempts to display a suggestion on surrounding text change. Returns if
  // suggestion was displayed according to the surrounding text information.
  virtual bool TrySuggestWithSurroundingText(const std::u16string& text,
                                             gfx::Range selection_range) = 0;

  // Accepts the suggestion at a given index, index can be made default if
  // unnecessary. Returns true if suggestion is accepted successfully.
  virtual bool AcceptSuggestion(size_t index) = 0;

  virtual void DismissSuggestion() = 0;

  // Return the propose assistive action type.
  virtual AssistiveType GetProposeActionType() = 0;

  // Is the suggester currently suggesting a candidate to the user?
  virtual bool HasSuggestions() = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTER_H_
