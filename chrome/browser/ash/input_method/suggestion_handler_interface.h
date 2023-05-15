// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_HANDLER_INTERFACE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_HANDLER_INTERFACE_H_

#include <string>

namespace ui {
namespace ime {
struct AssistiveWindowButton;
struct SuggestionDetails;
}  // namespace ime
}  // namespace ui

namespace ash {
namespace input_method {

struct AssistiveWindowProperties;

// An interface to handler suggestion related calls from assistive suggester.
class SuggestionHandlerInterface {
 public:
  virtual ~SuggestionHandlerInterface() = default;

  // Dismiss suggestion window.
  virtual bool DismissSuggestion(int context_id, std::string* error) = 0;

  // Sets text and show suggestion window.
  // text - the full suggestion text.
  // confirmed_text - the confirmed text that the user has typed so far.
  // show_tab - whether to show "tab" in the suggestion window.
  virtual bool SetSuggestion(int context_id,
                             const ui::ime::SuggestionDetails& details,
                             std::string* error) = 0;

  // Commit the suggestion and hide the window.
  virtual bool AcceptSuggestion(int context_id, std::string* error) = 0;

  virtual void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) = 0;

  // Highlights or unhighlights a given assistive button based on the given
  // parameters. No-op if context_id doesn't match or engine is not active.
  virtual bool SetButtonHighlighted(
      int context_id,
      const ui::ime::AssistiveWindowButton& button,
      bool highlighted,
      std::string* error) = 0;

  // Click the given button in assitive window.
  virtual void ClickButton(const ui::ime::AssistiveWindowButton& button) = 0;

  virtual bool AcceptSuggestionCandidate(int context_id,
                                         const std::u16string& candidate,
                                         size_t delete_previous_utf16_len,
                                         bool use_replace_surrounding_text,
                                         std::string* error) = 0;

  // Shows/Hides given assistive window. No-op if context_id doesn't match or
  // engine is not active.
  virtual bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) = 0;

  // Announces a message to the user by emitting a live region change event.
  virtual void Announce(const std::u16string& message) = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_HANDLER_INTERFACE_H_
