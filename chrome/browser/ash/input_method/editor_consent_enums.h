// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_

namespace ash::input_method {

enum class PromoCardAction {
  // User explicitly hits 'Learn More' button to proceed to use the feature.
  kAccepted,
  // User explicitly declines the promo card.
  kDeclined,
  // User dismisses the promo card.
  kDismissed,
};

enum class ConsentAction : int {
  // User explicitly hits "Yes/Agree" button.
  kApproved,
  // User explicitly hits "No/Disagree" button.
  kDeclined
};

// Defines the status of the consent which we ask the user to provide before
// we can display the feature to them.
// Only append new entries to the end of the enum value list and do not reorder
// the enum value list to maintain compatibility with the integer values saved
// in the pref storage.
enum class ConsentStatus : int {
  // User has agreed to consent by pressing "Yes/Agree" button to all dialogs
  // from the consent window.
  kApproved = 0,
  // User has disagreed to consent by pressing "No/Disagree" button to any
  // dialog from the consent window.
  kDeclined = 1,
  // Invalid state of the consent result.
  kInvalid = 2,
  // No explicit consent to use the feature has been received yet.
  kPending = 3,
  // No request has been sent to users to collect their consent.
  kUnset = 4,
};

enum class EditorMode { kBlocked, kConsentNeeded, kRewrite, kWrite };

ConsentStatus GetConsentStatusFromInteger(int status_value);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_
