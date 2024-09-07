// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/multi_word_suggester.h"

#include <cmath>
#include <optional>
#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ui/ash/input_method/suggestion_details.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;
using ime::AssistiveSuggestionType;
using ime::SuggestionsTextContext;

// Used for UmaHistogramExactLinear, should remain <= 101.
constexpr size_t kMaxSuggestionLength = 101;
constexpr size_t kMinimumNumberOfCharsToProduceSuggestion = 3;
constexpr char kMultiWordFirstAcceptTimeDays[] = "multi_word_first_accept";
constexpr char16_t kSuggestionShownMessage[] =
    u"predictive writing candidate shown, press down to select or "
    u"press tab to accept";
constexpr char kSuggestionSelectedMessage[] =
    "predictive writing candidate selected, candidate text is %s, "
    "press tab to accept or press up to deselect";
constexpr char16_t kSuggestionAcceptedMessage[] =
    u"predictive writing candidate inserted";
constexpr char16_t kSuggestionDismissedMessage[] =
    u"predictive writing candidate dismissed";

std::optional<AssistiveSuggestion> GetMultiWordSuggestion(
    const std::vector<AssistiveSuggestion>& suggestions) {
  if (suggestions.empty()) {
    return std::nullopt;
  }
  if (suggestions[0].type == AssistiveSuggestionType::kMultiWord) {
    // There should only ever be one multi word suggestion given at a time.
    DCHECK_EQ(suggestions.size(), 1u);
    return suggestions[0];
  }
  return std::nullopt;
}

size_t CalculateConfirmedLength(const std::u16string& surrounding_text,
                                const std::u16string& suggestion_text) {
  if (surrounding_text.empty() || suggestion_text.empty()) {
    return 0;
  }

  for (size_t i = suggestion_text.length(); i >= 1; i--) {
    if (base::EndsWith(surrounding_text, suggestion_text.substr(0, i))) {
      return i;
    }
  }

  return 0;
}

MultiWordSuggestionType ToSuggestionType(
    const ime::AssistiveSuggestionMode& suggestion_mode) {
  switch (suggestion_mode) {
    case ime::AssistiveSuggestionMode::kCompletion:
      return MultiWordSuggestionType::kCompletion;
    case ime::AssistiveSuggestionMode::kPrediction:
      return MultiWordSuggestionType::kPrediction;
    default:
      return MultiWordSuggestionType::kUnknown;
  }
}

void RecordTimeToAccept(base::TimeDelta delta) {
  base::UmaHistogramTimes("InputMethod.Assistive.TimeToAccept.MultiWord",
                          delta);
}

void RecordTimeToDismiss(base::TimeDelta delta) {
  base::UmaHistogramTimes("InputMethod.Assistive.TimeToDismiss.MultiWord",
                          delta);
}

void RecordSuggestionLength(size_t suggestion_length) {
  base::UmaHistogramExactLinear(
      "InputMethod.Assistive.MultiWord.SuggestionLength", suggestion_length,
      kMaxSuggestionLength);
}

void RecordCouldPossiblyShowSuggestion(
    const ime::AssistiveSuggestionMode& suggestion_mode) {
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.MultiWord.CouldPossiblyShowSuggestion",
      ToSuggestionType(suggestion_mode));
}

void RecordImplicitAcceptance(
    const ime::AssistiveSuggestionMode& suggestion_mode) {
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.MultiWord.ImplicitAcceptance",
      ToSuggestionType(suggestion_mode));
}

void RecordImplicitRejection(
    const ime::AssistiveSuggestionMode& suggestion_mode) {
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.MultiWord.ImplicitRejection",
      ToSuggestionType(suggestion_mode));
}

void RecordMultiWordSuggestionState(const MultiWordSuggestionState& state,
                                    const ime::AssistiveSuggestionMode& mode) {
  const std::string histogram =
      mode == ime::AssistiveSuggestionMode::kCompletion
          ? "InputMethod.Assistive.MultiWord.SuggestionState.Completion"
          : "InputMethod.Assistive.MultiWord.SuggestionState.Prediction";
  base::UmaHistogramEnumeration(histogram, state);
}

std::optional<int> GetTimeFirstAcceptedSuggestion(Profile* profile) {
  ScopedDictPrefUpdate update(profile->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  auto value = update->FindInt(kMultiWordFirstAcceptTimeDays);
  if (value.has_value()) {
    return value.value();
  }
  return std::nullopt;
}

void SetTimeFirstAcceptedSuggestion(Profile* profile) {
  ScopedDictPrefUpdate update(profile->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  auto time_since_epoch = base::Time::Now() - base::Time::UnixEpoch();
  update->Set(kMultiWordFirstAcceptTimeDays, time_since_epoch.InDaysFloored());
}

bool ShouldShowTabGuide(Profile* profile) {
  auto time_first_accepted = GetTimeFirstAcceptedSuggestion(profile);
  if (!time_first_accepted) {
    return true;
  }

  base::TimeDelta first_accepted = base::Days(*time_first_accepted);
  base::TimeDelta time_since_epoch =
      base::Time::Now() - base::Time::UnixEpoch();
  return (time_since_epoch - first_accepted) <= base::Days(7);
}

bool CouldSuggestWithSurroundingText(std::u16string_view text,
                                     const gfx::Range selection_range) {
  return selection_range.is_empty() && selection_range.end() == text.size() &&
         text.size() >= kMinimumNumberOfCharsToProduceSuggestion;
}

bool u16_isalpha(char16_t ch) {
  return (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z');
}

bool WouldBeInCompletionMode(std::u16string_view text) {
  return !text.empty() && u16_isalpha(text.back());
}

// TODO(crbug/1146266): Add DismissedAccuracy metric back in.

}  // namespace

MultiWordSuggester::MultiWordSuggester(
    SuggestionHandlerInterface* suggestion_handler,
    Profile* profile)
    : suggestion_handler_(suggestion_handler), state_(this), profile_(profile) {
  suggestion_button_.id = ui::ime::ButtonId::kSuggestion;
  suggestion_button_.window_type =
      ash::ime::AssistiveWindowType::kMultiWordSuggestion;
  suggestion_button_.suggestion_index = 0;
}

MultiWordSuggester::~MultiWordSuggester() = default;

void MultiWordSuggester::OnFocus(int context_id) {
  // Some parts of the code reserve negative/zero context_id for unfocused
  // context. As a result we should make sure it is not being erroneously set to
  // a negative number, and cause unexpected behaviour.
  DCHECK(context_id > 0);
  focused_context_id_ = context_id;
  state_.ResetSuggestion();
}

void MultiWordSuggester::OnBlur() {
  focused_context_id_ = std::nullopt;
  state_.ResetSuggestion();
}

void MultiWordSuggester::OnSurroundingTextChanged(
    const std::u16string& text,
    const gfx::Range selection_range) {
  if (CouldSuggestWithSurroundingText(text, selection_range) &&
      !state_.IsSuggestionShowing()) {
    RecordCouldPossiblyShowSuggestion(
        WouldBeInCompletionMode(text)
            ? ime::AssistiveSuggestionMode::kCompletion
            : ime::AssistiveSuggestionMode::kPrediction);
  }

  const uint32_t cursor_pos = selection_range.start();
  auto surrounding_text = SuggestionState::SurroundingText{
      .text = text,
      .cursor_pos = cursor_pos,
      .cursor_at_end_of_text =
          (selection_range.is_empty() && cursor_pos == text.length())};
  state_.UpdateSurroundingText(surrounding_text);
  DisplaySuggestionIfAvailable();
}

void MultiWordSuggester::OnExternalSuggestionsUpdated(
    const std::vector<AssistiveSuggestion>& suggestions,
    const std::optional<SuggestionsTextContext>& context) {
  if (state_.IsSuggestionShowing() || !state_.IsCursorAtEndOfText()) {
    return;
  }

  std::optional<AssistiveSuggestion> multi_word_suggestion =
      GetMultiWordSuggestion(suggestions);

  if (!multi_word_suggestion) {
    state_.UpdateState(SuggestionState::State::kNoSuggestionShown);
    return;
  }

  if (auto suggestion_length = multi_word_suggestion->text.size();
      suggestion_length < kMaxSuggestionLength) {
    RecordSuggestionLength(suggestion_length);
  }

  auto suggestion = SuggestionState::Suggestion{
      .mode = multi_word_suggestion->mode,
      .text = base::UTF8ToUTF16(multi_word_suggestion->text),
      .time_first_shown = base::TimeTicks::Now()};

  if (context) {
    auto suggestion_state = state_.ValidateSuggestion(suggestion, *context);
    RecordMultiWordSuggestionState(suggestion_state, suggestion.mode);
    if (suggestion_state != MultiWordSuggestionState::kValid) {
      return;
    }
  }

  state_.UpdateSuggestion(/*suggestion=*/suggestion,
                          /*new_tracking_behavior=*/context.has_value());
  DisplaySuggestionIfAvailable();
}

SuggestionStatus MultiWordSuggester::HandleKeyEvent(const ui::KeyEvent& event) {
  if (!state_.IsSuggestionShowing()) {
    return SuggestionStatus::kNotHandled;
  }

  switch (event.code()) {
    case ui::DomCode::TAB:
      AcceptSuggestion();
      return SuggestionStatus::kAccept;
    case ui::DomCode::ARROW_DOWN:
      if (state_.IsSuggestionHighlighted()) {
        return SuggestionStatus::kNotHandled;
      }
      state_.ToggleSuggestionHighlight();
      SetSuggestionHighlight(true);
      return SuggestionStatus::kBrowsing;
    case ui::DomCode::ARROW_UP:
      if (!state_.IsSuggestionHighlighted()) {
        return SuggestionStatus::kNotHandled;
      }
      state_.ToggleSuggestionHighlight();
      SetSuggestionHighlight(false);
      return SuggestionStatus::kBrowsing;
    case ui::DomCode::ENTER:
      if (!state_.IsSuggestionHighlighted()) {
        return SuggestionStatus::kNotHandled;
      }
      AcceptSuggestion();
      return SuggestionStatus::kAccept;
    default:
      return SuggestionStatus::kNotHandled;
  }
}

bool MultiWordSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    const gfx::Range selection_range) {
  // MultiWordSuggester does not trigger a suggest based on surrounding text
  // events. It only triggers suggestions OnExternalSuggestionsUpdated.
  //
  // Hence we should return whether the current suggestion is showing from
  // internal state.
  return state_.IsSuggestionShowing();
}

bool MultiWordSuggester::AcceptSuggestion(size_t index) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to accept suggestion. No context id.";
    return false;
  }

  std::string error;
  suggestion_handler_->AcceptSuggestion(*focused_context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to accept suggestion - " << error;
    return false;
  }

  auto suggestion = state_.GetSuggestion();
  if (suggestion) {
    RecordTimeToAccept(base::TimeTicks::Now() - suggestion->time_first_shown);
  }

  if (!GetTimeFirstAcceptedSuggestion(profile_)) {
    SetTimeFirstAcceptedSuggestion(profile_);
  }

  state_.UpdateState(SuggestionState::State::kSuggestionAccepted);
  state_.ResetSuggestion();
  return true;
}

void MultiWordSuggester::DismissSuggestion() {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to dismiss suggestion. No context id.";
    return;
  }
  std::string error;
  suggestion_handler_->DismissSuggestion(*focused_context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to dismiss suggestion - " << error;
    return;
  }

  auto suggestion = state_.GetSuggestion();
  if (suggestion) {
    RecordTimeToDismiss(base::TimeTicks::Now() - suggestion->time_first_shown);
  }

  state_.UpdateState(SuggestionState::State::kSuggestionDismissed);
  state_.ResetSuggestion();
}

AssistiveType MultiWordSuggester::GetProposeActionType() {
  return state_.GetLastSuggestionType();
}

bool MultiWordSuggester::HasSuggestions() {
  return state_.GetSuggestion().has_value();
}

std::vector<AssistiveSuggestion> MultiWordSuggester::GetSuggestions() {
  auto suggestion = state_.GetSuggestion();
  if (!suggestion) {
    return {};
  }
  return {
      AssistiveSuggestion{.mode = suggestion->mode,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = base::UTF16ToUTF8(suggestion->text),
                          .confirmed_length = suggestion->confirmed_length}};
}

void MultiWordSuggester::DisplaySuggestionIfAvailable() {
  auto suggestion_to_display = state_.GetSuggestion();
  if (suggestion_to_display.has_value()) {
    DisplaySuggestion(*suggestion_to_display);
  }
}

void MultiWordSuggester::DisplaySuggestion(
    const SuggestionState::Suggestion& suggestion) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to show suggestion. No context id.";
    return;
  }
  ui::ime::SuggestionDetails details;
  details.text = suggestion.text;
  details.show_accept_annotation = false;
  details.show_quick_accept_annotation = ShouldShowTabGuide(profile_);
  details.confirmed_length = suggestion.confirmed_length;
  details.show_setting_link = false;

  suggestion_button_.announce_string = base::UTF8ToUTF16(base::StringPrintf(
      kSuggestionSelectedMessage, base::UTF16ToUTF8(details.text).c_str()));

  std::string error;
  suggestion_handler_->SetSuggestion(*focused_context_id_, details, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to show suggestion in assistive framework"
               << " - " << error;
  }
}

void MultiWordSuggester::SetSuggestionHighlight(bool highlighted) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to set button highlighted. No context id.";
    return;
  }
  std::string error;
  suggestion_handler_->SetButtonHighlighted(
      *focused_context_id_, suggestion_button_, highlighted, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to set button highlighted. " << error;
  }
}

void MultiWordSuggester::Announce(const std::u16string& message) {
  if (suggestion_handler_) {
    suggestion_handler_->Announce(message);
  }
}

MultiWordSuggester::SuggestionState::SuggestionState(
    MultiWordSuggester* suggester)
    : suggester_(suggester) {}

MultiWordSuggester::SuggestionState::~SuggestionState() = default;

void MultiWordSuggester::SuggestionState::UpdateState(const State& state) {
  if (state == State::kPredictionSuggestionShown) {
    last_suggestion_type_ = AssistiveType::kMultiWordPrediction;
  }

  if (state == State::kCompletionSuggestionShown) {
    last_suggestion_type_ = AssistiveType::kMultiWordCompletion;
  }

  if (state_ == State::kNoSuggestionShown &&
      (state == State::kPredictionSuggestionShown ||
       state == State::kCompletionSuggestionShown)) {
    suggester_->Announce(kSuggestionShownMessage);
  }

  if ((state_ == State::kPredictionSuggestionShown ||
       state_ == State::kCompletionSuggestionShown ||
       state_ == State::kTrackingLastSuggestionShown) &&
      state == State::kSuggestionAccepted) {
    suggester_->Announce(kSuggestionAcceptedMessage);
  }

  if ((state_ == State::kPredictionSuggestionShown ||
       state_ == State::kCompletionSuggestionShown ||
       state_ == State::kTrackingLastSuggestionShown) &&
      state == State::kSuggestionDismissed) {
    suggester_->Announce(kSuggestionDismissedMessage);
  }

  state_ = state;
}

void MultiWordSuggester::SuggestionState::UpdateSurroundingText(
    const MultiWordSuggester::SuggestionState::SurroundingText&
        surrounding_text) {
  size_t prev_cursor_pos =
      surrounding_text_.has_value() ? surrounding_text_->cursor_pos : 0;

  surrounding_text_ = SurroundingText{
      .text = surrounding_text.text,
      .cursor_pos = surrounding_text.cursor_pos,
      .prev_cursor_pos = prev_cursor_pos,
      .cursor_at_end_of_text = surrounding_text.cursor_at_end_of_text};

  ReconcileSuggestionWithText();
}

void MultiWordSuggester::SuggestionState::UpdateSuggestion(
    const MultiWordSuggester::SuggestionState::Suggestion& suggestion,
    bool new_tracking_behavior) {
  suggestion_ = suggestion;
  suggestion_->original_surrounding_text_length =
      surrounding_text_ ? surrounding_text_->text.length() : 0;
  UpdateState(suggestion.mode == AssistiveSuggestionMode::kCompletion
                  ? State::kCompletionSuggestionShown
                  : State::kPredictionSuggestionShown);
  if (suggestion.mode == AssistiveSuggestionMode::kCompletion) {
    ReconcileSuggestionWithText();
  }
  if (new_tracking_behavior &&
      suggestion.mode == AssistiveSuggestionMode::kPrediction) {
    // With the new tracking behavior we are guaranteed that any new suggestion
    // is not stale, and thus can be simply appended to the current surrrounding
    // text. Therefore there is no need to reconcile with the current text and
    // we can transition straight to tracking mode.
    UpdateState(State::kTrackingLastSuggestionShown);
  }
}

MultiWordSuggestionState
MultiWordSuggester::SuggestionState::ValidateSuggestion(
    const MultiWordSuggester::SuggestionState::Suggestion& suggestion,
    const ime::SuggestionsTextContext& context) {
  if (!surrounding_text_) {
    return MultiWordSuggestionState::kOther;
  }

  // IME service works with UTF8 whereas here in Chromium surrounding text is
  // UTF16. The length of the surrounding text from the IME service was
  // calculated on a UTF8 string, so transforming context.last_n_chars to
  // UTF16 would invalidate the length sent from IME service.
  const std::string current_text = base::UTF16ToUTF8(surrounding_text_->text);
  size_t current_text_length = current_text.length();
  size_t text_length_when_suggested = context.surrounding_text_length;
  bool text_matches = base::EndsWith(current_text, context.last_n_chars);

  if (current_text_length == text_length_when_suggested && text_matches) {
    return MultiWordSuggestionState::kValid;
  }

  if (current_text_length == text_length_when_suggested && !text_matches) {
    return MultiWordSuggestionState::kStaleAndUserEditedText;
  }

  if (current_text_length < text_length_when_suggested) {
    return MultiWordSuggestionState::kStaleAndUserDeletedText;
  }

  if (current_text_length > text_length_when_suggested) {
    return CalculateConfirmedLength(surrounding_text_->text, suggestion.text) >
                   0
               ? MultiWordSuggestionState::kStaleAndUserAddedMatchingText
               : MultiWordSuggestionState::kStaleAndUserAddedDifferentText;
  }

  return MultiWordSuggestionState::kOther;
}

void MultiWordSuggester::SuggestionState::ReconcileSuggestionWithText() {
  if (!(suggestion_ && surrounding_text_)) {
    return;
  }

  size_t new_confirmed_length =
      CalculateConfirmedLength(surrounding_text_->text, suggestion_->text);

  // Save the calculated confirmed length on first showing of a completion
  // suggestion. This will be used later when determining if a suggestion
  // should be dismissed or not.
  auto initial_confirmed_length = state_ == State::kCompletionSuggestionShown
                                      ? new_confirmed_length
                                      : suggestion_->initial_confirmed_length;

  bool user_typed_suggestion =
      new_confirmed_length == suggestion_->text.length();

  // Are we still tracking the last suggestion shown to the user?
  //
  // TODO(b/279114189): Prediction suggestions are not dismissed correctly on
  //    first mismatched character typed, need to investigate.
  bool no_longer_tracking =
      state_ == State::kTrackingLastSuggestionShown &&
      ((new_confirmed_length == 0 ||
        new_confirmed_length < suggestion_->initial_confirmed_length) ||
       (new_confirmed_length == suggestion_->confirmed_length &&
        surrounding_text_->cursor_pos != surrounding_text_->prev_cursor_pos) ||
       user_typed_suggestion);

  bool user_has_typed_more = surrounding_text_->text.length() >
                             suggestion_->original_surrounding_text_length;
  if ((state_ == State::kPredictionSuggestionShown ||
       state_ == State::kTrackingLastSuggestionShown) &&
      new_confirmed_length == 0 && user_has_typed_more) {
    RecordImplicitRejection(suggestion_->mode);
  }

  if (no_longer_tracking && user_typed_suggestion) {
    RecordImplicitAcceptance(suggestion_->mode);
  }

  if (no_longer_tracking || !surrounding_text_->cursor_at_end_of_text) {
    UpdateState(State::kSuggestionDismissed);
    ResetSuggestion();
    return;
  }

  if (state_ == State::kPredictionSuggestionShown ||
      state_ == State::kCompletionSuggestionShown) {
    UpdateState(State::kTrackingLastSuggestionShown);
  }

  suggestion_ = Suggestion{.mode = suggestion_->mode,
                           .text = suggestion_->text,
                           .confirmed_length = new_confirmed_length,
                           .initial_confirmed_length = initial_confirmed_length,
                           .time_first_shown = suggestion_->time_first_shown,
                           .highlighted = suggestion_->highlighted,
                           .original_surrounding_text_length =
                               suggestion_->original_surrounding_text_length};
}

void MultiWordSuggester::SuggestionState::ToggleSuggestionHighlight() {
  if (!suggestion_) {
    return;
  }
  suggestion_->highlighted = !suggestion_->highlighted;
}

bool MultiWordSuggester::SuggestionState::IsSuggestionHighlighted() {
  if (!suggestion_) {
    return false;
  }
  return suggestion_->highlighted;
}

bool MultiWordSuggester::SuggestionState::IsSuggestionShowing() {
  return (state_ == State::kPredictionSuggestionShown ||
          state_ == State::kCompletionSuggestionShown ||
          state_ == State::kTrackingLastSuggestionShown);
}

bool MultiWordSuggester::SuggestionState::IsCursorAtEndOfText() {
  if (!surrounding_text_) {
    return false;
  }
  return surrounding_text_->cursor_at_end_of_text;
}

std::optional<MultiWordSuggester::SuggestionState::Suggestion>
MultiWordSuggester::SuggestionState::GetSuggestion() {
  return suggestion_;
}

void MultiWordSuggester::SuggestionState::ResetSuggestion() {
  suggestion_ = std::nullopt;
  UpdateState(State::kNoSuggestionShown);
}

AssistiveType MultiWordSuggester::SuggestionState::GetLastSuggestionType() {
  return last_suggestion_type_;
}

}  // namespace input_method
}  // namespace ash
