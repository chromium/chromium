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
  // Increments by 1 when a user right clicks on an eligible text field and the
  // feature itself is enabled.
  kNativeUIShowOpportunity = 0,
  // Increase by 1 when the native UI menu is shown.
  kNativeUIShown = 1,
  // Increase by 1 for only the first request to the server of the session.
  // Refines are not counted here.
  kRequest = 2,
  // Increase by 1 when a response page is dismissed by user clicking elsewhere
  // or explicitly clicking the close button.
  kDismiss = 3,
  // Increase by 1 for each refine request.
  kRefineRequest = 4,
  // Increase by 1 when a response is shown to the user.
  kSuccessResponse = 5,
  // Increase by 1 when a error is shown to the user.
  kErrorResponse = 6,
  // Increase by 1 when a thumbs up button is clicked.
  kThumbsUp = 7,
  // Increase by 1 when a thumbs down button is clicked.
  kThumbsDown = 8,
  // Increase by 1 when a response is inserted/replaced to the text field.
  kInsert = 9,
  // Increase by #characters inserted.
  kCharsInserted = 10,
  // Increase by #characters selected (user writing) for an insert. This is
  // recorded at the time of an insert.
  kCharsSelectedForInsert = 11,
  // Increase by #characters in a freeform request (if any) for an insert. This
  // is recorded at the time of an insert.
  kFreeformCharsForInsert = 12,
  // Increase by 1 when a user returns to previous in the webUI.
  kReturnToPreviousSuggestions = 13,
  kMaxValue = kReturnToPreviousSuggestions,
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_ENUMS_H_
