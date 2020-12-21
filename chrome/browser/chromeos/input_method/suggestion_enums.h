// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_ENUMS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_ENUMS_H_

namespace chromeos {

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
  kMaxValue = kAutocorrectReverted,
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

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_ENUMS_H_
