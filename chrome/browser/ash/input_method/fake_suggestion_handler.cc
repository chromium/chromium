// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"

#include "base/strings/string_util.h"

namespace ash {
namespace input_method {

FakeSuggestionHandler::FakeSuggestionHandler() = default;

FakeSuggestionHandler::~FakeSuggestionHandler() = default;

bool FakeSuggestionHandler::DismissSuggestion(int context_id,
                                              std::string* error) {
  showing_suggestion_ = false;
  dismissed_suggestion_ = true;
  suggestion_text_ = u"";
  confirmed_length_ = 0;
  return true;
}

bool FakeSuggestionHandler::SetSuggestion(
    int context_id,
    const ui::ime::SuggestionDetails& details,
    std::string* error) {
  showing_suggestion_ = true;
  context_id_ = context_id;
  suggestion_text_ = details.text;
  confirmed_length_ = details.confirmed_length;
  last_suggestion_details_ = details;
  return true;
}

bool FakeSuggestionHandler::AcceptSuggestion(int context_id,
                                             std::string* error) {
  showing_suggestion_ = false;
  accepted_suggestion_ = true;
  accepted_suggestion_text_ = suggestion_text_;
  suggestion_text_ = u"";
  confirmed_length_ = 0;
  return true;
}

void FakeSuggestionHandler::OnSuggestionsChanged(
    const std::vector<std::string>& suggestions) {
  last_on_suggestion_changed_event_suggestions_ = suggestions;
}

bool FakeSuggestionHandler::SetButtonHighlighted(
    int context_id,
    const ui::ime::AssistiveWindowButton& button,
    bool highlighted,
    std::string* error) {
  highlighted_suggestion_ = highlighted;
  highlighted_button_ = button;
  return false;
}

void FakeSuggestionHandler::ClickButton(
    const ui::ime::AssistiveWindowButton& button) {
  last_clicked_button_ = button.id;
}

bool FakeSuggestionHandler::AcceptSuggestionCandidate(
    int context_id,
    const std::u16string& candidate,
    size_t delete_previous_utf16_len,
    bool use_replace_surrounding_text,
    std::string* error) {
  showing_suggestion_ = false;
  accepted_suggestion_ = true;
  accepted_suggestion_text_ = candidate;
  delete_previous_utf16_len_ = delete_previous_utf16_len;
  suggestion_text_ = u"";
  confirmed_length_ = 0;
  return true;
}

bool FakeSuggestionHandler::SetAssistiveWindowProperties(
    int context_id,
    const AssistiveWindowProperties& assistive_window,
    std::string* error) {
  if (!assistive_window.announce_string.empty()) {
    announcements_.push_back(assistive_window.announce_string);
  }

  if (assistive_window.visible) {
    context_id_ = context_id;
    showing_suggestion_ = true;
    suggestion_text_ = base::JoinString(assistive_window.candidates, u";");
  } else {
    showing_suggestion_ = false;
  }
  return true;
}

void FakeSuggestionHandler::Announce(const std::u16string& message) {
  announcements_.push_back(message);
}

}  // namespace input_method
}  // namespace ash
