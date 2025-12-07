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
  // Blocked because the selection is invalid. E.g., selection causes the utf
  // conversion to fail.
  kBlockedByInvalidSelection,
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

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ENUMS_H_
