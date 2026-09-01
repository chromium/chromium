// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_API_METRICS_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_API_METRICS_H_

#include <stdint.h>

#include <string_view>

#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

// LINT.IfChange(GlicHostApiRequestId)
enum class GlicHostApiRequestId {
  kWebClientCreated = 1,
  kWebClientInitialized = 2,
  kCreateTab = 3,
  kOpenGlicSettingsPage = 4,
  kClosePanel = 5,
  kClosePanelAndShutdown = 6,
  kShowProfilePicker = 7,
  kGetModelQualityClientId = 8,
  kGetContextFromFocusedTab = 9,
  kGetContextFromTab = 10,
  kGetContextForActorFromTab = 11,
  kSetMaximumNumberOfPinnedTabs = 12,
  kStopActorTask = 13,
  kPauseActorTask = 14,
  kResumeActorTask = 15,
  kCaptureScreenshot = 16,
  kResizeWindow = 17,
  kEnableDragResize = 18,
  kSetMinimumWidgetSize = 20,
  kSetMicrophonePermissionState = 21,
  kSetLocationPermissionState = 22,
  kSetTabContextPermissionState = 23,
  kSetContextAccessIndicator = 24,
  kGetUserProfileInfo = 25,
  kRefreshSignInCookies = 26,
  kAttachPanel = 27,
  kDetachPanel = 28,
  kSetAudioDucking = 29,
  kLogBeginAsyncEvent = 30,
  kLogEndAsyncEvent = 31,
  kLogInstantEvent = 32,
  kJournalClear = 33,
  kJournalSnapshot = 34,
  kJournalStart = 35,
  kJournalStop = 36,
  kJournalRecordFeedback = 37,
  kOnUserInputSubmitted = 38,
  kOnResponseRated = 39,
  kOnResponseStarted = 40,
  kOnResponseStopped = 41,
  kOnSessionTerminated = 42,
  kOnTurnCompleted = 43,
  kScrollTo = 45,
  kSetSyntheticExperimentState = 46,
  kOpenOsPermissionSettingsMenu = 47,
  kGetOsMicrophonePermissionStatus = 48,
  kPinTabs = 49,
  kUnpinTabs = 50,
  kUnpinAllTabs = 51,
  kSubscribeToPinCandidates = 52,
  kGetZeroStateSuggestionsForFocusedTab = 54,
  kSetClosedCaptioningSetting = 56,
  kDropScrollToHighlight = 57,
  kMaybeRefreshUserStatus = 58,
  kOnClosedCaptionsShown = 59,
  kCreateTask = 60,
  kPerformActions = 61,
  kSubscribeToPageMetadata = 63,
  kSwitchConversation = 64,
  kRegisterConversation = 65,
  kOnReaction = 66,
  kOnContextUploadCompleted = 67,
  kOnContextUploadStarted = 68,
  kSetActuationOnWebSetting = 69,
  kOnModeChange = 70,
  kSubscribeToCaptureRegion = 71,
  kInterruptActorTask = 74,
  kUninterruptActorTask = 75,
  kActivateTab = 76,
  kCreateActorTab = 77,
  kOpenPasswordManagerSettingsPage = 78,
  kSetOnboardingCompleted = 80,
  kSubscribeToTabData = 81,
  kCreateSkill = 82,
  kUpdateSkill = 83,
  kGetSkill = 84,
  kCancelActions = 85,
  kShowManageSkillsUi = 86,
  kAutofillSuggestionDialogOnFormPresented = 87,
  kAutofillSuggestionDialogOnFormPreviewChanged = 88,
  kAutofillSuggestionDialogOnFormConfirmed = 89,
  kOnMicrophoneStatusChange = 90,
  kRecordSkillsWebClientEvent = 91,
  kDeleteCapturedRegion = 92,
  kOnActionSubmitted = 93,
  kSubscribeToTabFavicon = 94,
  kShowBrowseSkillsUi = 95,
  kSubscribeToZoomLevel = 96,
  kUnsubscribeFromZoomLevel = 97,
  kOnExperimentalTriggeringUpdate = 98,
  kOnOptinImpression = 99,
  kProcessCounterAbuseVerdict = 100,
  kGetImageBytesFromTab = 101,
  kActivateTabWithUrl = 102,
  kUpdateActorTaskStepProgress = 103,
  kOpenPinnedTabPicker = 104,
};
// LINT.ThenChange(
// //tools/metrics/histograms/metadata/glic/histograms.xml:ApiRequestType,
// //tools/metrics/histograms/metadata/glic/enums.xml:GlicHostApiRequestType)

// Returns the string name for a GlicHostApiRequestId (e.g. "CreateSkill").
std::string_view ToString(GlicHostApiRequestId request_id);

// Records the Glic.Api.RequestCounts.{request_type} and Glic.Api.StatusCounts.*
// UMA histograms for an API request event from the web client.
void LogApiRequestCount(
    GlicHostApiRequestId request_type_id,
    mojom::GlicRequestEvent event = mojom::GlicRequestEvent::kRequestReceived);

inline void LogApiRequestCount(
    int32_t request_type_id,
    mojom::GlicRequestEvent event = mojom::GlicRequestEvent::kRequestReceived) {
  LogApiRequestCount(static_cast<GlicHostApiRequestId>(request_type_id), event);
}

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_API_METRICS_H_
