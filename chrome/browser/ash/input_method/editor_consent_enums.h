// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_

namespace ash::input_method {

enum class PromoCardAction {
  // User explicitly hits 'Learn More' button to proceed to use the feature.
  kAccept,
  // User explicitly declines the promo card.
  kDecline,
  // User dismisses the promo card.
  kDismiss,
};

enum class ConsentAction : int {
  // User explicitly hits "Yes/Agree" button.
  kApprove,
  // User explicitly hits "No/Disagree" button.
  kDecline
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

// TODO: b: - Migrate EditorMode and EditorOpportunityMode out of this file.
enum class EditorMode {
  // Blocked because it does not meet hard requirements such as user age,
  // country and policy.
  kHardBlocked,
  // Temporarily blocked because it does not meet transient requirements such as
  // internet connection, device mode.
  kSoftBlocked,
  // Mode that requires users to provide consent before using the feature.
  kConsentNeeded,
  // Feature in rewrite mode.
  kRewrite,
  // Feature in write mode.
  kWrite
};

enum class EditorOpportunityMode {
  kInvalidInput,
  kRewrite,
  kWrite,
  kNotAllowedForUse,
};

// Defines the reason why the editor is blocked.
enum class EditorBlockedReason {
  // Blocked because the consent status does not satisfy.
  kBlockedByConsent,
  // Blocked because the setting toggle is switched off.
  kBlockedBySetting,
  // Blocked because the text is too long.
  kBlockedByTextLength,
  // Blocked because the focused text input residing in a url found in the
  // url denylist.
  kBlockedByUrl,
  // Blocked because the focused text input residing in an app found in the
  // app denylist.
  kBlockedByApp,
  // Blocked because the current active input method is not supported.
  kBlockedByInputMethod,
  // Blocked because the current active input type is not allowed.
  kBlockedByInputType,
  // Blocked because the current app type is not supported.
  kBlockedByAppType,
  // Blocked because current form factor is not supported.
  kBlockedByInvalidFormFactor,
  // Blocked because user is not connected to internet.
  kBlockedByNetworkStatus,
  // Blocked because user is not in a supported country.
  kBlockedByUnsupportedRegion,
  // Blocked because user is using a managed device.
  // kBlockedByManagedStatus_DEPRECATRD,
  // Blocked because user does not have the capability (age, account type) to
  // use the feature.
  kBlockedByUnsupportedCapability,
  // Blocked because the capability value has been been fetched and determined
  // yet.
  kBlockedByUnknownCapability,
  // Blocked because there is a policy that disables the feature.
  kBlockedByPolicy,
};

ConsentStatus GetConsentStatusFromInteger(int status_value);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_
