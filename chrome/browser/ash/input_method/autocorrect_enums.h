// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_ENUMS_H_

namespace ash {
namespace input_method {

// Must match with IMEAutocorrectPreference in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectPreference {
  kDefault = 0,
  kEnabled = 1,
  kDisabled = 2,
  kMaxValue = kDisabled,
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

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_ENUMS_H_
