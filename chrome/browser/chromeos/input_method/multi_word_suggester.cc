// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/multi_word_suggester.h"

#include <cmath>
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

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

int CalculateNumberMatchingChars(const std::u16string& first,
                                 const std::u16string& second) {
  int matching_character_count = 0;
  for (int i = 0; i < first.size() && i < second.size(); i++) {
    if (first[i] != second[i])
      break;
    matching_character_count++;
  }
  return matching_character_count;
}

std::u16string ExtractFinalWord(const std::u16string& text) {
  size_t last_space_index = text.rfind(u' ');
  size_t offset =
      last_space_index == std::u16string::npos ? 0 : last_space_index + 1;
  return text.substr(offset);
}

int CalculateDismissedAccuracy(const LastKnownSuggestionState& state) {
  size_t confirmed_text_end_pos = state.start_pos + state.confirmed_length;
  size_t num_chars_accurately_predicted =
      confirmed_text_end_pos >= state.predicted_text_start_pos
          ? confirmed_text_end_pos - state.predicted_text_start_pos
          : 0;
  double accuracy = static_cast<double>(num_chars_accurately_predicted) /
                    state.predicted_text_length;
  return std::round(accuracy * 100);
}

void RecordTimeToAccept(base::TimeDelta delta) {
  base::UmaHistogramTimes("InputMethod.Assistive.TimeToAccept.MultiWord",
                          delta);
}

void RecordTimeToDismiss(base::TimeDelta delta) {
  base::UmaHistogramTimes("InputMethod.Assistive.TimeToDismiss.MultiWord",
                          delta);
}

void RecordDismissedAccuracy(int percentage) {
  base::UmaHistogramPercentage(
      "InputMethod.Assistive.DismissedAccuracy.MultiWord", percentage);
}

}  // namespace

MultiWordSuggester::MultiWordSuggester(
    SuggestionHandlerInterface* suggestion_handler)
    : suggestion_handler_(suggestion_handler) {}

MultiWordSuggester::~MultiWordSuggester() = default;

void MultiWordSuggester::OnFocus(int context_id) {
  focused_context_id_ = context_id;
  ResetSuggestionState();
  ResetTextState();
}

void MultiWordSuggester::OnBlur() {
  focused_context_id_ = 0;
  ResetSuggestionState();
  ResetTextState();
}

void MultiWordSuggester::OnSurroundingTextChanged(const std::u16string& text,
                                                  size_t cursor_pos,
                                                  size_t anchor_pos) {
  bool cursor_at_end_of_text =
      cursor_pos == anchor_pos && cursor_pos == text.length();

  text_state_ = LastKnownTextState{
      .text = text, .cursor_at_end_of_text = cursor_at_end_of_text};
}

void MultiWordSuggester::OnExternalSuggestionsUpdated(
    const std::vector<TextSuggestion>& suggestions) {
  if (suggestion_state_ || !text_state_.cursor_at_end_of_text)
    return;

  absl::optional<TextSuggestion> multi_word_suggestion =
      GetMultiWordSuggestion(suggestions);

  if (multi_word_suggestion) {
    auto suggestion = multi_word_suggestion.value();
    auto suggestion_text = base::UTF8ToUTF16(suggestion.text);
    auto final_word = ExtractFinalWord(text_state_.text);
    int confirmed_length =
        suggestion.mode == TextSuggestionMode::kCompletion
            ? CalculateNumberMatchingChars(suggestion_text, final_word)
            : 0;

    DisplaySuggestion(suggestion_text, confirmed_length);

    size_t start_pos = text_state_.text.length() >= confirmed_length
                           ? text_state_.text.length() - confirmed_length
                           : 0;
    size_t predicted_text_start_pos = start_pos + confirmed_length;
    size_t predicted_text_length = suggestion_text.length() - confirmed_length;

    suggestion_state_ = LastKnownSuggestionState{
        .start_pos = start_pos,
        .text = suggestion_text,
        .confirmed_length = static_cast<size_t>(confirmed_length),
        .predicted_text_start_pos = predicted_text_start_pos,
        .predicted_text_length = predicted_text_length,
        .suggestion_mode = suggestion.mode,
        .time_shown_to_user = base::TimeTicks::Now()};
  }
}

SuggestionStatus MultiWordSuggester::HandleKeyEvent(const ui::KeyEvent& event) {
  if (!suggestion_state_)
    return SuggestionStatus::kNotHandled;

  switch (event.code()) {
    case ui::DomCode::TAB:
      AcceptSuggestion();
      return SuggestionStatus::kAccept;
    default:
      return SuggestionStatus::kNotHandled;
  }
}

bool MultiWordSuggester::Suggest(const std::u16string& text,
                                 size_t cursor_pos,
                                 size_t anchor_pos) {
  if (!suggestion_state_ || cursor_pos != text.length() ||
      suggestion_state_->start_pos > text.length())
    return false;

  auto last_suggestion_shown = suggestion_state_.value();
  auto possibly_confirmed_text =
      last_suggestion_shown.start_pos < text.length() &&
              last_suggestion_shown.start_pos >= 0
          ? text.substr(last_suggestion_shown.start_pos)
          : base::EmptyString16();
  bool matches_last_suggestion =
      base::StartsWith(last_suggestion_shown.text, possibly_confirmed_text,
                       base::CompareCase::INSENSITIVE_ASCII);

  if (matches_last_suggestion) {
    int confirmed_length = possibly_confirmed_text.length();
    DisplaySuggestion(last_suggestion_shown.text, confirmed_length);
    suggestion_state_->confirmed_length = confirmed_length;
    return true;
  }

  return false;
}

bool MultiWordSuggester::AcceptSuggestion(size_t index) {
  std::string error;
  suggestion_handler_->AcceptSuggestion(focused_context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: failed to accept suggestion - " << error;
    return false;
  }

  if (suggestion_state_) {
    RecordTimeToAccept(base::TimeTicks::Now() -
                       suggestion_state_->time_shown_to_user);
  }

  ResetSuggestionState();
  return true;
}

void MultiWordSuggester::DismissSuggestion() {
  std::string error;
  suggestion_handler_->DismissSuggestion(focused_context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to dismiss suggestion - " << error;
    return;
  }

  if (suggestion_state_) {
    RecordTimeToDismiss(base::TimeTicks::Now() -
                        suggestion_state_->time_shown_to_user);
    RecordDismissedAccuracy(
        CalculateDismissedAccuracy(suggestion_state_.value()));
  }

  ResetSuggestionState();
}

AssistiveType MultiWordSuggester::GetProposeActionType() {
  if (!suggestion_state_)
    return AssistiveType::kGenericAction;

  AssistiveType multi_word_type =
      suggestion_state_->suggestion_mode == TextSuggestionMode::kCompletion
          ? AssistiveType::kMultiWordCompletion
          : AssistiveType::kMultiWordPrediction;

  return multi_word_type;
}

bool MultiWordSuggester::HasSuggestions() {
  return false;
}

std::vector<TextSuggestion> MultiWordSuggester::GetSuggestions() {
  return {};
}

void MultiWordSuggester::DisplaySuggestion(const std::u16string& text,
                                           int confirmed_length) {
  ui::ime::SuggestionDetails details;
  details.text = text;
  details.show_accept_annotation = false;
  details.show_quick_accept_annotation = true;
  details.confirmed_length = confirmed_length;
  details.show_setting_link = false;

  std::string error;
  suggestion_handler_->SetSuggestion(focused_context_id_, details, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to show suggestion in assistive framework"
               << " - " << error;
  }
}

void MultiWordSuggester::ResetSuggestionState() {
  suggestion_state_ = absl::nullopt;
}

void MultiWordSuggester::ResetTextState() {
  text_state_ = LastKnownTextState{
      .text = u"",
      .cursor_at_end_of_text = true,
  };
}

}  // namespace chromeos
