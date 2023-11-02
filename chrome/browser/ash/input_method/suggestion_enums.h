// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_ENUMS_H_

namespace ash {
namespace input_method {

// Must match with IMEAssistiveAction in enums.xml
enum class AssistiveType {
  kGenericAction = 0,
  kPersonalEmail = 1,
  kPersonalAddress = 2,
  kPersonalPhoneNumber = 3,
  kPersonalName = 4,
  kEmoji = 5,
  kAssistiveAutocorrect = 6,
  kPersonalNumber = 7,  // Deprecated, combined with kPersonalPhoneNumber
  kPersonalFirstName = 8,
  kPersonalLastName = 9,
  kAutocorrectWindowShown = 10,
  kAutocorrectUnderlined = 11,
  kAutocorrectReverted = 12,
  kMultiWordPrediction = 13,
  kMultiWordCompletion = 14,
  kLongpressDiacritics = 15,
  kMaxValue = kLongpressDiacritics,
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

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTION_ENUMS_H_
