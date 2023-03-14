// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_ENUMS_H_

namespace ash {
namespace input_method {

// Must match with IMEAutocorrectActions in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectActions {
  kWindowShown = 0,
  kUnderlined = 1,
  kReverted = 2,
  kUserAcceptedAutocorrect = 3,
  kUserActionClearedUnderline = 4,
  kUserExitedTextFieldWithUnderline = 5,
  kInvalidRange = 6,
  kMaxValue = kInvalidRange,
};

// Must match with IMEAutocorrectCompatibilitySummary in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectCompatibilitySummary {
  kWindowShown = 0,
  kUnderlined = 1,
  kReverted = 2,
  kUserAcceptedAutocorrect = 3,
  kUserActionClearedUnderline = 4,
  kUserExitedTextFieldWithUnderline = 5,
  kInvalidRange = 6,
  kVeryFastAcceptedAutocorrect = 7,
  kFastAcceptedAutocorrect = 8,
  kVeryFastRejectedAutocorrect = 9,
  kFastRejectedAutocorrect = 10,
  kVeryFastExitField = 11,
  kFastExitField = 12,
  kMaxValue = kFastExitField,
};

// Must match with IMEAutocorrectInternalStates in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectInternalStates {
  // Autocorrect handles an empty range.
  kHandleEmptyRange = 0,
  // Autocorrect handles a new suggestion while the previous one is still
  // pending.
  kHandleUnclearedRange = 1,
  // Autocorrect handles a new suggestion while input context is not available.
  kHandleNoInputContext = 2,
  // Autocorrect is called with a range, text, and suggestion that do not
  // match.
  kHandleInvalidArgs = 3,
  // Autocorrect handler sets a range to TextInputClient.
  kHandleSetRange = 4,
  // Autocorrect suggestion is underlined.
  kUnderlineShown = 5,
  // Autocorrect suggestion is resolved by user interactions and not
  // error, exit field or undone.
  kSuggestionResolved = 6,
  // Autocorrect suggestion is accepted by user interaction.
  kSuggestionAccepted = 7,
  // Autocorrect is cleared because Input context is lost while having a
  // pending autocorrect.
  kNoInputContext = 8,
  // Autocorrect cannot set a range because TextInputClient does not support
  // setting a range.
  kErrorSetRange = 9,
  // Autocorrect fails to validate a suggestion because of potentially async
  // problems prevent it from finding the suggested text within the autocorrect
  // range in surrounding text.
  kErrorRangeNotValidated = 10,
  // Autocorrect got an error when trying to show undo window.
  kErrorShowUndoWindow = 11,
  // Autocorrect got an error when trying to hide undo window.
  kErrorHideUndoWindow = 12,
  // Autocorrect shows an undo window.
  kShowUndoWindow = 13,
  // Autocorrect hides an undo window.
  kHideUndoWindow = 14,
  // Autocorrect highlights undo button of undo window.
  kHighlightUndoWindow = 15,
  // OnFocus event was called.
  kOnFocusEvent = 16,
  // OnFocus event was called with pending suggestion.
  kOnFocusEventWithPendingSuggestion = 17,
  // OnBlur event was called.
  kOnBlurEvent = 18,
  // OnBlue event was called with pending suggestion.
  kOnBlurEventWithPendingSuggestion = 19,
  // User did some typing and had at least one suggestion.
  kTextFieldEditsWithAtLeastOneSuggestion = 20,
  // Autocorrect could be triggered if the last word typed had an error.
  kCouldTriggerAutocorrect = 21,
  // The focused text field is in a denylisted domain.
  kAppIsInDenylist = 22,
  // The focused text field is in a denylisted domain but autocorrect is still
  // executed.
  kHandleSuggestionInDenylistedApp = 23,
  kMaxValue = kHandleSuggestionInDenylistedApp,
};

// Must match with IMEAutocorrectPreference in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectPreference {
  kDefault = 0,
  kEnabled = 1,
  kDisabled = 2,
  kEnabledByDefault = 3,
  kMaxValue = kEnabledByDefault,
};

// Must match with IMEAutocorrectPrefStateTransition in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectPrefStateTransition {
  // General setting transitions.
  kUnchanged = 0,
  kDefaultToDisabled = 1,
  kDefaultToEnabled = 2,
  kDisabledToEnabled = 3,
  kEnabledToDisabled = 4,
  // Interesting transitions for the enabled by default experiment.
  kDefaultToForceEnabled = 5,
  kForceEnabledToDisabled = 6,
  kForceEnabledToDefault = 7,
  kForceEnabledToEnabled = 8,
  kMaxValue = kForceEnabledToEnabled,
};

// Must match with IMEAutocorrectQualityBreakdown in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectQualityBreakdown {
  // All the suggestions that resolved.
  kSuggestionResolved = 0,
  // Original text included only ascii letters.
  kOriginalTextIsAscii = 1,
  // Suggested text included only ascii letters.
  kSuggestedTextIsAscii = 2,
  // Suggestion splitted a word into more than one.
  kSuggestionSplittedWord = 3,
  // Suggestion capitalized first word.
  kSuggestionCapitalizedWord = 4,
  // Suggestion made word lower case.
  kSuggestionLowerCasedWord = 5,
  // Suggestion is equal to original text when compared case insensitive.
  kSuggestionChangeLetterCases = 6,
  // Suggestion was longer than the original text.
  kSuggestionInsertedLetters = 7,
  // Suggestion was shorter than the original text.
  kSuggestionRemovedLetters = 8,
  // Autocorrect suggestion had the same length as the original text.
  kSuggestionMutatedLetters = 9,
  // Autocorrect suggestion changed accents.
  kSuggestionChangedAccent = 10,
  kMaxValue = kSuggestionChangedAccent,
};

// Must match with IMEAutocorrectRejectionBreakdown in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectRejectionBreakdown {
  kSuggestionRejected = 0,
  kRejectionOther = 1,
  kUndoWithoutKeyboard = 2,
  kUndoWithKeyboard = 3,
  kUndoCtrlZ = 4,
  kRejectedBackspace = 5,
  kRejectedCtrlBackspace = 6,
  kRejectedTypingFull = 7,
  kRejectedTypingPartial = 8,
  kRejectedTypingFullWithExternal = 9,
  kRejectedTypingPartialWithExternal = 10,
  kRemovedLetters = 11,
  kRejectedTypingNoSelection = 12,
  kRejectedSelectedInvalidRange = 13,
  kMaxValue = kRejectedSelectedInvalidRange,
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_ENUMS_H_
