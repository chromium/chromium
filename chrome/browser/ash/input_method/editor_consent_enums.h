// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_

namespace ash::input_method {

enum class ConsentAction : int {
  // User explicitly hits "Yes/Agree" button.
  kApproved,
  // User dismisses the consent window.
  kDismissed,
  // User explicitly hits "No/Disagree" button.
  kDeclined
};

// Defines the status of the consent which we ask the user to provide before
// we can display the feature to them.
enum class ConsentStatus : int {
  // User has agreed to consent by pressing "Yes/Agree" button to all dialogs
  // from the consent window.
  kApproved,
  // User has disagreed to consent by pressing "No/Disagree" button to any
  // dialog from the consent window.
  kDeclined,
  // User has dismissed the consent page too many times and is deemed to
  // implicitly decline the consent.
  kImplicitlyDeclined,
  // Invalid state of the consent result.
  kInvalid,
  // No explicit consent to use the feature has been received yet.
  kPending,
  // No request has been sent to users to collect their consent.
  kUnset,
};

ConsentStatus GetConsentStatusFromInteger(int status_value);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_
