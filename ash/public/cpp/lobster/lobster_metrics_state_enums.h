// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_METRICS_STATE_ENUMS_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_METRICS_STATE_ENUMS_H_

namespace ash {

enum class LobsterMetricState {
  // recorded when the feature can be shown, or is blocked but has the potential
  // to be shown.
  kShownOpportunity = 0,
  // recorded when the feature is blocked.
  kBlocked = 1,
  // recorded when the right click menu trigger is shown to the user.
  kRightClickTriggerImpression = 2,
  // recorded when the right click menu trigger is fired by the user.
  kRightClickTriggerFired = 3,
  // recorded when the right click menu trigger is fired but the user hasn’t
  // given their consent to use the feature.
  kRightClickTriggerNeedsConsent = 4,
  // recorded when the Picker trigger is shown to the user.
  kPickerTriggerImpression = 5,
  // recorded when the Picker trigger is fired by the user.
  kPickerTriggerFired = 6,
  // recorded when the Picker trigger is fired but the user hasn’t given their
  // consent to use the feature.
  kPickerTriggerNeedsConsent = 7,
  // recorded when the consent screen is shown after the user has fired one of
  // the triggers.
  kConsentScreenImpression = 8,
  // recorded when the user approves their consent.
  kConsentAccepted = 9,
  // recorded when the user rejects their consent.
  kConsentRejected = 10,
  // recorded when the lobster ui is shown with the freeform input ready to
  // collect the user’s freeform query.
  kQueryPageImpression = 11,
  // recorded when a request is made to generate the initial candidates to be
  // shown in the results page (not when the “generate more” button is fired).
  kRequestInitialCandidates = 12,
  // recorded when the initial candidates request returned successfully.
  kRequestInitialCandidatesSuccess = 13,
  // recorded when the initial candidates request returned with an error
  // response.
  kRequestInitialCandidatesError = 14,
  // recorded when the lobster ui is shown with results for the first time (not
  // fired when new results are generated with the “generate more” button).
  kInitialCandidatesImpression = 15,
  // recorded when the user requests more candidates to be generated (i.e. when
  // the “generate more” button is clicked).
  kRequestMoreCandidates = 16,
  // recorded when request more candidates returns successfully.
  kRequestMoreCandidatesSuccess = 17,
  // recorded when request more candidates returns an error.
  kRequestMoreCandidatesError = 18,
  // recorded when more candidates are generated and shown to the user.
  kMoreCandidatesAppended = 19,
  // recorded when a user triggers a download of a candidate image.
  kCandidateDownload = 20,
  // recorded when a candidate image has successfully downloaded after the user
  // requested to download it.
  kCandidateDownloadSuccess = 21,
  // recorded when a candidate image failed to download after a user requested
  // to download it.
  kCandidateDownloadError = 22,
  // recorded when a user requests to end their session by downloading an image
  // candidate (ie. the final CTA text is “download” instead of “insert”).
  kCommitAsDownload = 23,
  // recorded when the user requests to end their session by downloading an
  // image candidate and the download was successful.
  kCommitAsDownloadSuccess = 24,
  // recorded when the user requests to end their session by downloading an
  // image candidate and the download fails.
  kCommitAsDownloadError = 25,
  // recorded when a user requests to end their session by inserting an image
  // candidate (ie. the final CTA text is “insert”).
  kCommitAsInsert = 26,
  // recorded when a user requests to end their session by inserting an image
  // candidate and the insertion was successful.
  kCommitAsInsertSuccess = 27,
  //  recorded when a user requests to end their session by inserting an image
  //  candidate and the insertion failed.
  kCommitAsInsertError = 28,
  //  recorded when a user presses thumbs up button for any image candidate.
  kFeedbackThumbsUp = 29,
  //  recorded when a user presses thumbs down button for any image candidate.
  kFeedbackThumbsDown = 30,
  kMaxValue = kFeedbackThumbsDown,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_METRICS_STATE_ENUMS_H_
