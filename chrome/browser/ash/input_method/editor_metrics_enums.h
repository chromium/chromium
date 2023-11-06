// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_ENUMS_H_

namespace ash::input_method {

// Must match with IMEEditorStates in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EditorStates {
  // When a user right clicks on an eligible text field and the feature itself
  // is enabled.
  kNativeUIShowOpportunity = 0,
  // When the native UI menu is shown.
  kNativeUIShown = 1,
  // For only the first request to the server of the session from the views UI.
  // Refines are not counted here.
  kNativeRequest = 2,
  // When a response page is dismissed by user clicking elsewhere or explicitly
  // clicking the close button.
  kDismiss = 3,
  // For each refine request and by editing the prompt directly at the top of
  // the webUI.
  kRefineRequest = 4,
  // When response(s) are shown to the user.
  kSuccessResponse = 5,
  // When a error is shown to the user.
  kErrorResponse = 6,
  // When a thumbs up button is clicked.
  kThumbsUp = 7,
  // When a thumbs down button is clicked.
  kThumbsDown = 8,
  // When a response is inserted/replaced to the text field.
  kInsert = 9,

  // Migrated to another histogram due to incorrect use
  // kCharsInserted = 10,
  // kCharsSelectedForInsert = 11,
  // kFreeformCharsForInsert = 12,

  // When a user returns to previous in the webUI.
  kReturnToPreviousSuggestions = 13,
  // When a user directly clicks the WebUI close button to close the feature.
  kClickCloseButton = 14,
  // For the first request to the server of the session from the WebUI. This
  // will happen right after a user initially consents, or if they get an error
  // and try again.
  kWebUIRequest = 15,
  // Increase by 1 when a user clicks to approve the consent.
  kApproveConsent = 16,
  // Increase by 1 when a user clicks to decline the consent.
  kDeclineConsent = 17,
  kMaxValue = kDeclineConsent,
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_ENUMS_H_
