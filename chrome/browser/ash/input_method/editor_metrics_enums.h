// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_ENUMS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_ENUMS_H_

namespace ash::input_method {

enum class EditorTone {
  kUnset = 0,
  kRephrase = 1,
  kEmojify = 2,
  kShorten = 3,
  kElaborate = 4,
  kFormalize = 5,
  kFreeformRewrite = 6,
  kUnknown = 7,
  kProofread = 8,
  kMaxValue = kProofread,
};

// Must match with IMEEditorCriticalStates in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EditorCriticalStates {
  // Recorded whenever the native card UI is shown to a user, regardless of the
  // contents of the card (ie includes promo and editor card).
  kShowUI = 0,
  // Recorded whenever a request is sent to the server.
  kRequestTriggered = 1,
  // Recorded whenever a text suggestion is inserted to a text field.
  kTextInserted = 2,
};

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
  // Increase by 1 when the feature is blocked.
  kBlocked = 18,
  // Increase by 1 when the feature is blocked because user is in an unsupported
  // country.
  kBlockedByUnsupportedRegion = 19,
  // Increase by 1 when the feature is blocked because user is using a managed
  // device.
  // kBlockedByManagedStatus_DEPRECATED = 20,
  // Increase by 1 when the feature is blocked because the consent status does
  // not satisfy.
  kBlockedByConsent = 21,
  // Increase by 1 when the feature is blocked because the setting toggle is
  // switched off.
  kBlockedBySetting = 22,
  // Increase by 1 when the feature is blocked because the text is too long.
  kBlockedByTextLength = 23,
  // Increase by 1 when the feature is blocked because the focused text input
  // residing in a url found in the url denylist.
  kBlockedByUrl = 24,
  // Increase by 1 when the feature is blocked because the focused text input
  // residing in an app found in the app denylist.
  kBlockedByApp = 25,
  // Increase by 1 when the feature is blocked because the current active input
  // method is not supported.
  kBlockedByInputMethod = 26,
  // Increase by 1 when the feature is blocked because the current active input
  // type is not allowed.
  kBlockedByInputType = 27,
  // Increase by 1 when the feature is blocked because the current app type is
  // not supported.
  kBlockedByAppType = 28,
  // Increase by 1 when the feature is blocked because the current form factor
  // is not supported.
  kBlockedByInvalidFormFactor = 29,
  // Increase by 1 when the feature is blocked because user is not connected to
  // internet.
  kBlockedByNetworkStatus = 30,
  // Increase by 1 when user receives unknown error from the server.
  kErrorUnknown = 31,
  // Increase by 1 when user receives invalid argument error from the server.
  kErrorInvalidArgument = 32,
  // Increase by 1 when user receives resource exhausted error from the server.
  kErrorResourceExhausted = 33,
  // Increase by 1 when user receives backend failure error from the server.
  kErrorBackendFailure = 34,
  // Increase by 1 when user receives internet connection error from the server.
  kErrorNoInternetConnection = 35,
  // Increase by 1 when user receives unsupported language error from the
  // server.
  kErrorUnsupportedLanguage = 36,
  // Increase by 1 when user receives blocked output error from the server.
  kErrorBlockedOutputs = 37,
  // Increase by 1 when user receives restricted region error from the server.
  kErrorRestrictedRegion = 38,
  // Increase by 1 when the native promo card is shown.
  kPromoCardImpression = 39,
  // Increase by 1 when user clicks "Dismiss" on the promo card.
  kPromoCardExplicitDismissal = 40,
  // Increase by 1 when the webui consent screen is shown.
  kConsentScreenImpression = 41,
  // Increase by 1 when a text insertion has been requested.
  kTextInsertionRequested = 42,
  // Increase by 1 when text has been queued for insertion.
  kTextQueuedForInsertion = 43,
  // Increase by 1 when any request is made.
  kRequest = 44,
  // Increase by 1 when the feature is blocked because user does not have the
  // capability (age, account type) to use the feature.
  kBlockedByUnsupportedCapability = 45,
  //  Increase by 1 when the feature is blocked because the capability value has
  //  been been fetched and determined yet.
  kBlockedByUnknownCapability = 46,
  //  Increase by 1 when the feature is blocked because there is an associated
  //  policy that disables the feature.
  kBlockedByPolicy = 47,
  kMaxValue = kBlockedByPolicy,
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_ENUMS_H_
