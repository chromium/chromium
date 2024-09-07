// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_FAKE_SUGGESTION_HANDLER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_FAKE_SUGGESTION_HANDLER_H_

#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ui/ash/input_method/suggestion_details.h"

namespace ash {
namespace input_method {

// Fake suggestion handler used for testing.
//
// TODO(crbug/1201529): This class has borrowed heavily from the
// `TestSuggestionHandler` class in personal_info_suggester_unittest.cc. That
// class included a number of testing assertions within the fake whereas this
// class does not. In future CLs we should remove the `TestSuggestionHandler`
// class from personal_info_suggester_unittest.cc and replace it with this
// class.
class FakeSuggestionHandler : public SuggestionHandlerInterface {
 public:
  FakeSuggestionHandler();
  ~FakeSuggestionHandler() override;

  // SuggestionHandlerInterface overrides
  bool DismissSuggestion(int context_id, std::string* error) override;
  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override;
  bool AcceptSuggestion(int context_id, std::string* error) override;
  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override;
  bool SetButtonHighlighted(int context_id,
                            const ui::ime::AssistiveWindowButton& button,
                            bool highlighted,
                            std::string* error) override;
  void ClickButton(const ui::ime::AssistiveWindowButton& button) override;
  bool AcceptSuggestionCandidate(int context_id,
                                 const std::u16string& candidate,
                                 size_t delete_previous_utf16_len,
                                 bool use_replace_surrounding_text,
                                 std::string* error) override;
  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override;
  void Announce(const std::u16string& message) override;

  // Test getters
  int GetContextId() { return context_id_; }
  std::u16string GetSuggestionText() { return suggestion_text_; }
  std::u16string GetAcceptedSuggestionText() {
    return accepted_suggestion_text_;
  }
  size_t GetConfirmedLength() { return confirmed_length_; }
  size_t GetDeletePreviousUtf16Len() { return delete_previous_utf16_len_; }
  bool GetShowingSuggestion() { return showing_suggestion_; }
  bool GetAcceptedSuggestion() { return accepted_suggestion_; }
  bool GetDismissedSuggestion() { return dismissed_suggestion_; }
  bool GetHighlightedSuggestion() { return highlighted_suggestion_; }
  ui::ime::AssistiveWindowButton GetHighlightedButton() {
    return highlighted_button_;
  }
  std::vector<std::u16string> GetAnnouncements() { return announcements_; }
  std::vector<std::string> GetLastOnSuggestionChangedEventSuggestions() {
    return last_on_suggestion_changed_event_suggestions_;
  }
  ui::ime::SuggestionDetails GetLastSuggestionDetails() {
    return last_suggestion_details_;
  }
  ui::ime::ButtonId GetLastClickedButton() { return last_clicked_button_; }

 private:
  int context_id_ = 0;
  std::u16string suggestion_text_;
  std::u16string accepted_suggestion_text_;
  size_t confirmed_length_ = 0;
  size_t delete_previous_utf16_len_ = 0;
  bool showing_suggestion_ = false;
  bool accepted_suggestion_ = false;
  bool dismissed_suggestion_ = false;
  bool highlighted_suggestion_ = false;
  ui::ime::AssistiveWindowButton highlighted_button_;
  std::vector<std::u16string> announcements_;
  std::vector<std::string> last_on_suggestion_changed_event_suggestions_;
  ui::ime::SuggestionDetails last_suggestion_details_;
  ui::ime::ButtonId last_clicked_button_ = ui::ime::ButtonId::kNone;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_FAKE_SUGGESTION_HANDLER_H_
