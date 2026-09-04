// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_api_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

std::string_view ToString(GlicHostApiRequestId request_id) {
  switch (request_id) {
    case GlicHostApiRequestId::kWebClientCreated:
      return "WebClientCreated";
    case GlicHostApiRequestId::kWebClientInitialized:
      return "WebClientInitialized";
    case GlicHostApiRequestId::kCreateTab:
      return "CreateTab";
    case GlicHostApiRequestId::kOpenGlicSettingsPage:
      return "OpenGlicSettingsPage";
    case GlicHostApiRequestId::kClosePanel:
      return "ClosePanel";
    case GlicHostApiRequestId::kClosePanelAndShutdown:
      return "ClosePanelAndShutdown";
    case GlicHostApiRequestId::kShowProfilePicker:
      return "ShowProfilePicker";
    case GlicHostApiRequestId::kGetModelQualityClientId:
      return "GetModelQualityClientId";
    case GlicHostApiRequestId::kGetContextFromFocusedTab:
      return "GetContextFromFocusedTab";
    case GlicHostApiRequestId::kGetContextFromTab:
      return "GetContextFromTab";
    case GlicHostApiRequestId::kGetContextForActorFromTab:
      return "GetContextForActorFromTab";
    case GlicHostApiRequestId::kSetMaximumNumberOfPinnedTabs:
      return "SetMaximumNumberOfPinnedTabs";
    case GlicHostApiRequestId::kStopActorTask:
      return "StopActorTask";
    case GlicHostApiRequestId::kPauseActorTask:
      return "PauseActorTask";
    case GlicHostApiRequestId::kResumeActorTask:
      return "ResumeActorTask";
    case GlicHostApiRequestId::kCaptureScreenshot:
      return "CaptureScreenshot";
    case GlicHostApiRequestId::kResizeWindow:
      return "ResizeWindow";
    case GlicHostApiRequestId::kEnableDragResize:
      return "EnableDragResize";
    case GlicHostApiRequestId::kSetMinimumWidgetSize:
      return "SetMinimumWidgetSize";
    case GlicHostApiRequestId::kSetMicrophonePermissionState:
      return "SetMicrophonePermissionState";
    case GlicHostApiRequestId::kSetLocationPermissionState:
      return "SetLocationPermissionState";
    case GlicHostApiRequestId::kSetTabContextPermissionState:
      return "SetTabContextPermissionState";
    case GlicHostApiRequestId::kSetContextAccessIndicator:
      return "SetContextAccessIndicator";
    case GlicHostApiRequestId::kGetUserProfileInfo:
      return "GetUserProfileInfo";
    case GlicHostApiRequestId::kRefreshSignInCookies:
      return "RefreshSignInCookies";
    case GlicHostApiRequestId::kAttachPanel:
      return "AttachPanel";
    case GlicHostApiRequestId::kDetachPanel:
      return "DetachPanel";
    case GlicHostApiRequestId::kSetAudioDucking:
      return "SetAudioDucking";
    case GlicHostApiRequestId::kLogBeginAsyncEvent:
      return "LogBeginAsyncEvent";
    case GlicHostApiRequestId::kLogEndAsyncEvent:
      return "LogEndAsyncEvent";
    case GlicHostApiRequestId::kLogInstantEvent:
      return "LogInstantEvent";
    case GlicHostApiRequestId::kJournalClear:
      return "JournalClear";
    case GlicHostApiRequestId::kJournalSnapshot:
      return "JournalSnapshot";
    case GlicHostApiRequestId::kJournalStart:
      return "JournalStart";
    case GlicHostApiRequestId::kJournalStop:
      return "JournalStop";
    case GlicHostApiRequestId::kJournalRecordFeedback:
      return "JournalRecordFeedback";
    case GlicHostApiRequestId::kOnUserInputSubmitted:
      return "OnUserInputSubmitted";
    case GlicHostApiRequestId::kOnResponseRated:
      return "OnResponseRated";
    case GlicHostApiRequestId::kOnResponseStarted:
      return "OnResponseStarted";
    case GlicHostApiRequestId::kOnResponseStopped:
      return "OnResponseStopped";
    case GlicHostApiRequestId::kOnSessionTerminated:
      return "OnSessionTerminated";
    case GlicHostApiRequestId::kOnTurnCompleted:
      return "OnTurnCompleted";
    case GlicHostApiRequestId::kScrollTo:
      return "ScrollTo";
    case GlicHostApiRequestId::kSetSyntheticExperimentState:
      return "SetSyntheticExperimentState";
    case GlicHostApiRequestId::kOpenOsPermissionSettingsMenu:
      return "OpenOsPermissionSettingsMenu";
    case GlicHostApiRequestId::kGetOsMicrophonePermissionStatus:
      return "GetOsMicrophonePermissionStatus";
    case GlicHostApiRequestId::kPinTabs:
      return "PinTabs";
    case GlicHostApiRequestId::kUnpinTabs:
      return "UnpinTabs";
    case GlicHostApiRequestId::kUnpinAllTabs:
      return "UnpinAllTabs";
    case GlicHostApiRequestId::kSubscribeToPinCandidates:
      return "SubscribeToPinCandidates";
    case GlicHostApiRequestId::kGetZeroStateSuggestionsForFocusedTab:
      return "GetZeroStateSuggestionsForFocusedTab";
    case GlicHostApiRequestId::kSetClosedCaptioningSetting:
      return "SetClosedCaptioningSetting";
    case GlicHostApiRequestId::kDropScrollToHighlight:
      return "DropScrollToHighlight";
    case GlicHostApiRequestId::kMaybeRefreshUserStatus:
      return "MaybeRefreshUserStatus";
    case GlicHostApiRequestId::kOnClosedCaptionsShown:
      return "OnClosedCaptionsShown";
    case GlicHostApiRequestId::kCreateTask:
      return "CreateTask";
    case GlicHostApiRequestId::kPerformActions:
      return "PerformActions";
    case GlicHostApiRequestId::kSubscribeToPageMetadata:
      return "SubscribeToPageMetadata";
    case GlicHostApiRequestId::kSwitchConversation:
      return "SwitchConversation";
    case GlicHostApiRequestId::kRegisterConversation:
      return "RegisterConversation";
    case GlicHostApiRequestId::kOnReaction:
      return "OnReaction";
    case GlicHostApiRequestId::kOnContextUploadCompleted:
      return "OnContextUploadCompleted";
    case GlicHostApiRequestId::kOnContextUploadStarted:
      return "OnContextUploadStarted";
    case GlicHostApiRequestId::kSetActuationOnWebSetting:
      return "SetActuationOnWebSetting";
    case GlicHostApiRequestId::kOnModeChange:
      return "OnModeChange";
    case GlicHostApiRequestId::kSubscribeToCaptureRegion:
      return "SubscribeToCaptureRegion";
    case GlicHostApiRequestId::kInterruptActorTask:
      return "InterruptActorTask";
    case GlicHostApiRequestId::kUninterruptActorTask:
      return "UninterruptActorTask";
    case GlicHostApiRequestId::kActivateTab:
      return "ActivateTab";
    case GlicHostApiRequestId::kCreateActorTab:
      return "CreateActorTab";
    case GlicHostApiRequestId::kOpenPasswordManagerSettingsPage:
      return "OpenPasswordManagerSettingsPage";
    case GlicHostApiRequestId::kSetOnboardingCompleted:
      return "SetOnboardingCompleted";
    case GlicHostApiRequestId::kSubscribeToTabData:
      return "SubscribeToTabData";
    case GlicHostApiRequestId::kCreateSkill:
      return "CreateSkill";
    case GlicHostApiRequestId::kUpdateSkill:
      return "UpdateSkill";
    case GlicHostApiRequestId::kGetSkill:
      return "GetSkill";
    case GlicHostApiRequestId::kCancelActions:
      return "CancelActions";
    case GlicHostApiRequestId::kShowManageSkillsUi:
      return "ShowManageSkillsUi";
    case GlicHostApiRequestId::kAutofillSuggestionDialogOnFormPresented:
      return "AutofillSuggestionDialogOnFormPresented";
    case GlicHostApiRequestId::kAutofillSuggestionDialogOnFormPreviewChanged:
      return "AutofillSuggestionDialogOnFormPreviewChanged";
    case GlicHostApiRequestId::kAutofillSuggestionDialogOnFormConfirmed:
      return "AutofillSuggestionDialogOnFormConfirmed";
    case GlicHostApiRequestId::kOnMicrophoneStatusChange:
      return "OnMicrophoneStatusChange";
    case GlicHostApiRequestId::kRecordSkillsWebClientEvent:
      return "RecordSkillsWebClientEvent";
    case GlicHostApiRequestId::kDeleteCapturedRegion:
      return "DeleteCapturedRegion";
    case GlicHostApiRequestId::kOnActionSubmitted:
      return "OnActionSubmitted";
    case GlicHostApiRequestId::kSubscribeToTabFavicon:
      return "SubscribeToTabFavicon";
    case GlicHostApiRequestId::kShowBrowseSkillsUi:
      return "ShowBrowseSkillsUi";
    case GlicHostApiRequestId::kSubscribeToZoomLevel:
      return "SubscribeToZoomLevel";
    case GlicHostApiRequestId::kUnsubscribeFromZoomLevel:
      return "UnsubscribeFromZoomLevel";
    case GlicHostApiRequestId::kOnExperimentalTriggeringUpdate:
      return "OnExperimentalTriggeringUpdate";
    case GlicHostApiRequestId::kOnOptinImpression:
      return "OnOptinImpression";
    case GlicHostApiRequestId::kProcessCounterAbuseVerdict:
      return "ProcessCounterAbuseVerdict";
    case GlicHostApiRequestId::kGetImageBytesFromTab:
      return "GetImageBytesFromTab";
    case GlicHostApiRequestId::kActivateTabWithUrl:
      return "ActivateTabWithUrl";
    case GlicHostApiRequestId::kUpdateActorTaskStepProgress:
      return "UpdateActorTaskStepProgress";
    case GlicHostApiRequestId::kOpenPinnedTabPicker:
      return "OpenPinnedTabPicker";
    case GlicHostApiRequestId::kOpenContactInfoSettingsPage:
      return "OpenContactInfoSettingsPage";
  }
  return "";
}

void LogApiRequestCount(GlicHostApiRequestId request_type_id,
                        mojom::GlicRequestEvent event) {
  std::string_view request_type = ToString(request_type_id);
  if (!request_type.empty()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Glic.Api.RequestCounts.", request_type}), event);
  }
  auto raw_id = static_cast<int32_t>(request_type_id);
  if (raw_id > 0) {
    switch (event) {
      case mojom::GlicRequestEvent::kRequestReceived:
        base::UmaHistogramSparse("Glic.Api.StatusCounts.Received", raw_id);
        break;
      case mojom::GlicRequestEvent::kRequestReceivedWhileInactive:
        base::UmaHistogramSparse("Glic.Api.StatusCounts.Inactive", raw_id);
        break;
      case mojom::GlicRequestEvent::kRequestHandlerException:
        base::UmaHistogramSparse("Glic.Api.StatusCounts.Error", raw_id);
        break;
      case mojom::GlicRequestEvent::kResponseSent:
        break;
    }
  }
}

}  // namespace glic
