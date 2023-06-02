// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_ENUMS_H_

namespace ash::input_method {

// Must match with IMEAssistiveAction in enums.xml
enum class AssistiveType {
  kGenericAction = 0,
  // kPersonalEmail = 1,  // Deprecated, feature has been deleted.
  // kPersonalAddress = 2,  // Deprecated, feature has been deleted.
  // kPersonalPhoneNumber = 3,  // Deprecated, feature has been deleted.
  // kPersonalName = 4,  // Deprecated, feature has been deleted.
  kEmoji = 5,
  kAssistiveAutocorrect = 6,
  // kPersonalNumber = 7,  // Deprecated, combined with kPersonalPhoneNumber
  // kPersonalFirstName = 8,  // Deprecated, feature has been deleted.
  // kPersonalLastName = 9,  // Deprecated, feature has been deleted.
  kAutocorrectWindowShown = 10,
  kAutocorrectUnderlined = 11,
  kAutocorrectReverted = 12,
  kMultiWordPrediction = 13,
  kMultiWordCompletion = 14,
  kLongpressDiacritics = 15,
  kLongpressControlV = 16,
  kMaxValue = kLongpressControlV,
};

enum class SuggestionStatus {
  kNotHandled = 0,
  kAccept = 1,
  kDismiss = 2,
  kBrowsing = 3,
  kOpenSettings = 4,
};

// Must match with IMEAssistiveDisabledReason in enums.xml
enum class DisabledReason {
  kNone = 0,
  kFeatureFlagOff = 1,
  kEnterpriseSettingsOff = 2,
  kUserSettingsOff = 3,
  kUrlOrAppNotAllowed = 4,
  kMaxValue = kUrlOrAppNotAllowed,
};

// Must match with IMEAssistiveTextInputState in enums.xml
enum class AssistiveTextInputState {
  kNone = 0,
  kFeatureBlockedByDenylist = 1,
  kFeatureBlockedByPreference = 2,
  kUnsupportedClient = 3,
  kUnsupportedLanguage = 4,
  kFeatureEnabled = 5,
  kMaxValue = kFeatureEnabled,
};

// Must match with IMEAssistiveMultiWordSuggestionType in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MultiWordSuggestionType {
  kUnknown = 0,
  kPrediction = 1,
  kCompletion = 2,
  kMaxValue = kCompletion,
};

// Must match with IMEAssistiveMultiWordSuggestionState in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MultiWordSuggestionState {
  kOther = 0,
  kValid = 1,
  kStaleAndUserEditedText = 2,
  kStaleAndUserDeletedText = 3,
  kStaleAndUserAddedMatchingText = 4,
  kStaleAndUserAddedDifferentText = 5,
  kMaxValue = kStaleAndUserAddedDifferentText,
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_ENUMS_H_
