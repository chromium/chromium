// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/multi_word_suggester.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

using TextSuggestion = ::chromeos::ime::TextSuggestion;
using TextSuggestionType = ::chromeos::ime::TextSuggestionType;

absl::optional<TextSuggestion> GetMultiWordSuggestion(
    const std::vector<TextSuggestion>& suggestions) {
  if (suggestions.empty())
    return absl::nullopt;
  if (suggestions[0].type == TextSuggestionType::kMultiWord) {
    // There should only ever be one multi word suggestion given at a time.
    DCHECK_EQ(suggestions.size(), 1);
    return suggestions[0];
  }
  return absl::nullopt;
}

}  // namespace

MultiWordSuggester::MultiWordSuggester(
    SuggestionHandlerInterface* suggestion_handler)
    : suggestion_handler_(suggestion_handler) {}

MultiWordSuggester::~MultiWordSuggester() = default;

void MultiWordSuggester::OnFocus(int context_id) {
  focused_context_id_ = context_id;
}

void MultiWordSuggester::OnBlur() {
  focused_context_id_ = 0;
}

void MultiWordSuggester::OnExternalSuggestionsUpdated(
    const std::vector<TextSuggestion>& suggestions) {
  auto multi_word_suggestion = GetMultiWordSuggestion(suggestions);
  if (multi_word_suggestion) {
    DisplaySuggestion(multi_word_suggestion.value());
  }
}

SuggestionStatus MultiWordSuggester::HandleKeyEvent(const ui::KeyEvent& event) {
  if (!suggestion_shown_)
    return SuggestionStatus::kNotHandled;

  switch (event.code()) {
    case ui::DomCode::TAB:
      AcceptSuggestion();
      return SuggestionStatus::kAccept;
    default:
      return SuggestionStatus::kNotHandled;
  }
}

bool MultiWordSuggester::Suggest(const std::u16string& text) {
  return false;
}

bool MultiWordSuggester::AcceptSuggestion(size_t index) {
  std::string error;
  suggestion_handler_->AcceptSuggestion(focused_context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: failed to accept suggestion - " << error;
    return false;
  }

  suggestion_shown_ = false;
  return true;
}

void MultiWordSuggester::DismissSuggestion() {
  std::string error;
  suggestion_handler_->DismissSuggestion(focused_context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to dismiss suggestion - " << error;
    return;
  }

  suggestion_shown_ = false;
}

AssistiveType MultiWordSuggester::GetProposeActionType() {
  return AssistiveType::kMultiWordCompletion;
}

bool MultiWordSuggester::HasSuggestions() {
  return false;
}

std::vector<TextSuggestion> MultiWordSuggester::GetSuggestions() {
  return {};
}

void MultiWordSuggester::DisplaySuggestion(const TextSuggestion& suggestion) {
  ui::ime::SuggestionDetails details;
  details.text = base::UTF8ToUTF16(suggestion.text);

  // TODO(crbug/1146266): Handle completion candidates where part of the
  // suggested text is already written by the user.
  details.confirmed_length = 0;

  // TODO(crbug/1146266): Handle the suggestion ui annotation.
  details.show_annotation = false;

  // TODO(crbug/1146266): Add required pref counter to hide settings link.
  details.show_setting_link = false;

  std::string error;
  suggestion_handler_->SetSuggestion(focused_context_id_, details, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to show suggestion in assistive framework"
               << " - " << error;
  }

  suggestion_shown_ = true;
}

}  // namespace chromeos
