// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebClientInitialState} from '../glic.mojom-webui.js';
import type {AdditionalContext, AdditionalContextPart, AnnotatedPageData, CaptureRegionErrorReason, CaptureRegionParams, CaptureRegionResult, ChromeVersion, ClientCapabilities, ClientErrorDialogType, ConversationInfo, CounterAbuseVerdict, CreateSkillRequest, ErrorReasonTypes, ErrorWithReason, ExperimentalTriggeringUpdate, FocusedTabDataHasFocus, FocusedTabDataHasNoFocus, FormFactor, GetPinCandidatesOptions, HostCapability, InvokeOptions, MetricUserInputReactionType, MicrophoneStatus, OnResponseStoppedDetails, OpenPanelInfo, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, PinCandidate, PinTabsOptions, Platform, ResumeActorTaskResult, Screenshot, ScrollToParams, Skill, SkillPreview, SkillsWebClientEvent, TabContextOptions, TabContextResult, TabData, UnpinTabsOptions, UpdateSkillRequest, UserProfileInfo, WebClientMode, ZeroStateSuggestions, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../glic_api/glic_api.js';

import type {ActorClient, ActorHost} from './actor/actor_types.js';
import type {CheckStructuredClonable, ReplaceProperties, ValidateRequestMap} from './transport/messaging.js';
import {assertNever} from './transport/messaging.js';

/*
This file defines messages sent over postMessage in-between the Glic WebUI
and the Glic web client.

Most requests closely match signatures of API methods. Where possible, name
messages by concatenating the interface name with the method name. This helps
readability, and ensures that each name is unique.
*/

export declare interface WebClientHost {
  // This message is sent just before calling initialize() on the web client.
  // It is not part of the GlicBrowserHost public API.
  glicBrowserWebClientCreated: {
    request: {
      clientCapabilities: ClientCapabilities[],
    },
    response: {
      initialState: WebClientInitialStatePrivate,
    },
    backgroundAllowed: true,
  };
  // This message is sent after the client returns from initialize(). It is not
  // part of the GlicBrowserHost public API.
  glicBrowserWebClientInitialized: {
    request: {
      success: boolean,
      // Exception present if initialize() returns a rejected promise (success
      // is false).
      exception?: TransferableException,
    },
    backgroundAllowed: true,
  };

  glicBrowserOnExperimentalTriggeringUpdate: {
    request: {
      observationId: number,
      update?: ExperimentalTriggeringUpdate,
            observation: SubscriberObservationType,
    },
    backgroundAllowed: true,
  };

  // The messages that fulfil the GlicBrowserHost public API follow below.

  glicBrowserCreateTab: {
    request: {
      url: string,
      options: {openInBackground?: boolean, windowId?: string},
    },
    response: {
      // Undefined on failure.
      tabData?: TabDataPrivate,
    },
    backgroundAllowed: false,
  };
  glicBrowserOpenGlicSettingsPage: {
    request: {options?: OpenSettingsOptions},
    backgroundAllowed: true,
  };
  glicBrowserOpenPasswordManagerSettingsPage: {
    backgroundAllowed: true,
  };
  glicBrowserClosePanel: {
    backgroundAllowed: true,
  };
  glicBrowserClosePanelAndShutdown: {
    backgroundAllowed: true,
  };
  glicBrowserShowProfilePicker: {};
  glicBrowserGetModelQualityClientId: {
    response: {
      modelQualityClientId: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserSwitchConversation: {
    request: {
      info?: ConversationInfo,
    },
    response: {},
    backgroundAllowed: true,
  };
  glicBrowserRegisterConversation: {
    request: {
      info: ConversationInfo,
    },
    response: {},
    backgroundAllowed: true,
  };
  glicBrowserGetContextFromFocusedTab: {
    request: {
      options: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
    backgroundAllowed: false,
  };
  glicBrowserGetContextFromTab: {
    backgroundAllowed: false,
    request: {
      tabId: string,
      options: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
  };
  glicBrowserSetMaximumNumberOfPinnedTabs: {
    request: {
      requestedMax: number,
    },
    response: {
      effectiveMax: number,
    },
    backgroundAllowed: true,
  };
  glicBrowserActivateTab: {
    request: {
      tabId: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserCaptureScreenshot: {
    response: {
      screenshot: Screenshot,
    },
    backgroundAllowed: false,
  };
  glicBrowserResizeWindow: {
    request: {
      size: {
        width: number,
        height: number,
      },
      options?: {
        durationMs?: number,
      },
    },
    backgroundAllowed: true,
  };
  glicBrowserEnableDragResize: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserSetMinimumWidgetSize: {
    request: {
      size: {
        width: number,
        height: number,
      },
    },
    backgroundAllowed: true,
  };
  glicBrowserSetMicrophonePermissionState: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserSetLocationPermissionState: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserSetTabContextPermissionState: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserSetClosedCaptioningSetting: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserSetContextAccessIndicator: {
    request: {
      show: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserSetActuationOnWebSetting: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserGetUserProfileInfo: {
    response: {
      profileInfo?: UserProfileInfoPrivate,
    },
    backgroundAllowed: true,
  };
  glicBrowserRefreshSignInCookies: {
    response: {
      success: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserAttachPanel: {
    backgroundAllowed: true,
  };
  glicBrowserDetachPanel: {
    backgroundAllowed: true,
  };
  glicBrowserSetAudioDucking: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserOnUserInputSubmitted: {
    request: {
      mode: number,
    },
    backgroundAllowed: true,
  };
  glicBrowserOnReaction: {
    backgroundAllowed: true,
    request: {
      reactionType: MetricUserInputReactionType,
    },
  };
  glicBrowserOnOptinImpression: {
    backgroundAllowed: true,
  };
  glicBrowserOnContextUploadStarted: {
    backgroundAllowed: true,
  };
  glicBrowserOnContextUploadCompleted: {
    backgroundAllowed: true,
  };
  glicBrowserOnResponseStarted: {
    backgroundAllowed: true,
  };
  glicBrowserOnResponseStopped: {
    request: {details?: OnResponseStoppedDetails},
    backgroundAllowed: true,
  };
  glicBrowserOnSessionTerminated: {
    backgroundAllowed: true,
  };
  glicBrowserOnTurnCompleted: {
    request: {
      model: number,
      duration: number,
    },
    backgroundAllowed: true,
  };
  glicBrowserOnResponseRated: {
    request: {
      positive: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserOnClosedCaptionsShown: {
    backgroundAllowed: true,
  };
  glicBrowserOnActionSubmitted: {
    request: {
      isRetry?: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserScrollTo: {
    request: {
      params: ScrollToParams,
    },
    backgroundAllowed: false,
  };
  glicBrowserDropScrollToHighlight: {
    backgroundAllowed: true,
  };
  glicBrowserSetSyntheticExperimentState: {
    request: {
      trialName: string,
      groupName: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserOpenOsPermissionSettingsMenu: {request: {permission: string}};
  glicBrowserGetOsMicrophonePermissionStatus: {
    response: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserPinTabs: {
    backgroundAllowed: false,
    request: {
      tabIds: string[],
      options?: PinTabsOptions,
    },
    response: {
      pinnedAll: boolean,
    },
  };
  glicBrowserUnpinTabs: {
    backgroundAllowed: true,
    request: {
      tabIds: string[],
      options?: UnpinTabsOptions,
    },
    response: {
      unpinnedAll: boolean,
    },
  };
  glicBrowserUnpinAllTabs: {
    backgroundAllowed: false,
    request: {
      options?: UnpinTabsOptions,
    },
  };
  glicBrowserCreateSkill: {
    request: {
      request: CreateSkillRequest,
    },
    response: {
      modalOpened: boolean,
    },
  };
  glicBrowserUpdateSkill: {
    request: {
      request: UpdateSkillRequest,
    },
    response: {
      modalOpened: boolean,
    },
  };
  glicBrowserShowManageSkillsUi: {
    backgroundAllowed: true,
  };
  glicBrowserShowBrowseSkillsUi: {
    backgroundAllowed: true,
  };
  glicBrowserGetSkill: {
    request: {
      id: string,
    },
    response: {
      skill?: Skill,
    },
  };
  glicBrowserRecordSkillsWebClientEvent: {
    request: {
      event: SkillsWebClientEvent,
    },
    backgroundAllowed: true,
  };
  glicBrowserSubscribeToPinCandidates: {
    backgroundAllowed: false,
    request: {
      options: GetPinCandidatesOptions,
      observationId: number,
    },
  };
  glicBrowserUnsubscribeFromPinCandidates: {
    request: {
      observationId: number,
    },
    backgroundAllowed: true,
  };
  glicBrowserSubscribeToCaptureRegion: {
    request: {
      observationId: number,
      params?: CaptureRegionParams,
    },
    backgroundAllowed: true,
  };
  glicBrowserUnsubscribeFromCaptureRegion: {
    request: {
      observationId: number,
    },
    backgroundAllowed: true,
  };
  glicBrowserDeleteCapturedRegion: {
    request: {
      tabId: string,
      regionId: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserGetZeroStateSuggestionsForFocusedTab: {
    request: {
      isFirstRun?: boolean,
    },
    response: {
      suggestions?: ZeroStateSuggestions,
    },
    backgroundAllowed: false,
  };
  glicBrowserMaybeRefreshUserStatus: {
    backgroundAllowed: true,
  };
  glicBrowserGetZeroStateSuggestionsAndSubscribe: {
    request: {
      hasActiveSubscription: boolean,
      options: ZeroStateSuggestionsOptions,
    },
    response: {
      suggestions?: ZeroStateSuggestionsV2,
    },
  };
  glicBrowserSubscribeToPageMetadata: {
    request: {
      tabId: string,
      names: string[],
    },
    response: {
      success: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserOnModeChange: {
    request: {
      newMode: WebClientMode,
    },
    backgroundAllowed: true,
  };
  glicBrowserSetOnboardingCompleted: {
    backgroundAllowed: true,
  };
  glicBrowserSubscribeToTabData: {
    request: {
      tabId: string,
      observationId: number,
      cancel: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserSubscribeToTabFavicon: {
    request: {
      tabId: string,
      observationId: number,
      cancel: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserOnMicrophoneStatusChange: {
    request: {
      status: MicrophoneStatus,
    },
    backgroundAllowed: true,
  };
  glicBrowserRecordHistogram: {
    request: {
      name: string,
      sparseValue: number,
      // Add other histogram types as needed.
    },
    backgroundAllowed: true,
  };
  glicBrowserSetErrorDialogState: {
    request: {
      shownDialogType?: ClientErrorDialogType,
    },
    backgroundAllowed: true,
  };
  glicBrowserReportClientTransientError: {
    request: {
      abslStatus: number,
    },
    backgroundAllowed: true,
  };
  glicBrowserProcessCounterAbuseVerdict: {
    request: {
      tabId: string,
      verdict: CounterAbuseVerdict,
    },
    backgroundAllowed: true,
  };
  glicBrowserSubscribeToZoomLevel: {
    backgroundAllowed: true,
  };
  glicBrowserUnsubscribeFromZoomLevel: {
    backgroundAllowed: true,
  };
}
export type CheckWebClientHost = ValidateRequestMap<WebClientHost>;

// Types of requests to the GlicWebClient.
export declare interface WebClient {
  glicWebClientNotifyPanelWillOpen: {
    request: {
      panelOpeningData: PanelOpeningData,
    },
    response: {
      openPanelInfo?: OpenPanelInfo,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyPanelWasClosed: {
    backgroundAllowed: true,
  };
  glicWebClientStopMicrophone: {
    backgroundAllowed: true,
  };
  glicWebClientPanelStateChanged: {
    request: {
      panelState: PanelState,
    },
    backgroundAllowed: true,
  };
  glicWebClientCanAttachStateChanged: {
    request: {
      canAttach: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyMicrophonePermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyLocationPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyTabContextPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyDefaultTabContextPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyOsLocationPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyClosedCaptioningSettingChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyActuationOnWebSettingChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyFocusedTabChanged: {
    request: {
      focusedTabDataPrivate: FocusedTabDataPrivate,
    },
  };
  glicWebClientNotifyPanelActiveChanged: {
    request: {
      panelActive: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientCheckResponsive: {
    response: {
      clientSendMessageQueueLength: number,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyManualResizeChanged: {
    request: {
      resizing: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientBrowserIsOpenChanged: {
    request: {
      browserIsOpen: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyOsHotkeyStateChanged: {
    request: {
      hotkey: string,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyPinnedTabsChanged: {
    request: {
      tabData: TabDataPrivate[],
    },
  };
  glicWebClientNotifyPinnedTabDataChanged: {
    request: {
      tabData: TabDataPrivate,
    },
  };
  glicWebClientNotifySkillPreviewsChanged: {
    request: {
      skillPreviews: SkillPreview[],
    },
  };
  glicWebClientNotifySkillPreviewChanged: {
    request: {
      skillPreview: SkillPreview,
    },
  };
  glicWebClientNotifyContextualSkillPreviewsChanged: {
    request: {
      contextualSkillPreviews: SkillPreview[],
    },
  };
  glicWebClientNotifySkillDeleted: {
    request: {
      skillId: string,
    },
    backgroundAllowed: true,
  };
  glicWebClientPinCandidatesChanged: {
    request: {
      candidates: PinCandidatePrivate[],
      observationId: number,
    },
  };
  glicWebClientZeroStateSuggestionsChanged: {
    request: {
      suggestions: ZeroStateSuggestionsV2,
      options: ZeroStateSuggestionsOptions,
    },
  };
  glicWebClientPageMetadataChanged: {
    request: {
      tabId: string,
      pageMetadata: PageMetadata|null,
    },
  };
  glicWebClientNotifyAdditionalContext: {
    request: {
      context: AdditionalContextPrivate,
    },
  };
  glicWebClientCaptureRegionUpdate: {
    request: {
      result?: CaptureRegionResult,
      reason?: CaptureRegionErrorReason, observationId: number,
    },
  };
  glicWebClientNotifyActOnWebCapabilityChanged: {
    request: {
      canActOnWeb: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientOnboardingCompletedChanged: {
    request: {
      completed: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyActorTaskListRowClicked: {
    request: {
      taskId: number,
    },
    backgroundAllowed: true,
  };
  glicWebClientTabDataChanged: {
    request: {
      // If not present, the tab no longer exists and no more updates will be
      // received.
      tabData?: TabDataPrivate, observationId: number,
    },
    backgroundAllowed: true,
  };
  glicWebClientTabFaviconChanged: {
    request: {
      observationId: number,
      // If true, the tab was removed and no more updates will be received.
      tabRemoved?: boolean,
      favicon?: RgbaImage,
    },
    backgroundAllowed: true,
  };
  glicWebClientInvoke: {
    request: {
      options: InvokeOptionsPrivate,
    },
    backgroundAllowed: true,
  };
  glicWebClientGetExperimentalTriggeringUpdates: {
    request: {
      observationId: number,
    },
    response: {
      success: boolean,
    },
    backgroundAllowed: true,
  };
  glicWebClientNotifyZoomLevelChanged: {
    request: {
      zoomFactor: number,
    },
    backgroundAllowed: true,
  };
}
export type CheckWebClient = ValidateRequestMap<WebClient>;

export type WebClientRequestTypes = WebClient&ActorClient;

export type HostRequestTypes = WebClientHost&ActorHost;

export type ValidateAllMessages =
    ValidateRequestMap<HostRequestTypes&WebClientRequestTypes>;

// Each host request needs to be added to either UnreportedRequests or
// RECORDED_REQUEST_IDS. Requests in UnreportedRequests will not record
// histograms.
interface UnreportedRequests {
  RecordHistogram: null;
  SetErrorDialogState: null;
  ReportClientTransientError: null;
}

type RemoveStringPrefix<S extends string, Prefix extends string> =
    S extends `${Prefix}${infer Rest}` ? Rest : 'prefixNotFound!';

type HostRequestEnumNamesType = Omit<
    {
      [K in keyof HostRequestTypes as RemoveStringPrefix<K, 'glicBrowser'>]:
          number;
    },
    keyof UnreportedRequests>;

// LINT.IfChange(ApiRequestType)
// New values here must be added to histograms.xml and to enums.xml.
const RECORDED_REQUEST_IDS = {
  WebClientCreated: 1,
  WebClientInitialized: 2,
  CreateTab: 3,
  OpenGlicSettingsPage: 4,
  ClosePanel: 5,
  ClosePanelAndShutdown: 6,
  ShowProfilePicker: 7,
  GetModelQualityClientId: 8,
  GetContextFromFocusedTab: 9,
  GetContextFromTab: 10,
  GetContextForActorFromTab: 11,
  SetMaximumNumberOfPinnedTabs: 12,
  StopActorTask: 13,
  PauseActorTask: 14,
  ResumeActorTask: 15,
  CaptureScreenshot: 16,
  ResizeWindow: 17,
  EnableDragResize: 18,
  // Do not reuse deleted request ID: 19,
  SetMinimumWidgetSize: 20,
  SetMicrophonePermissionState: 21,
  SetLocationPermissionState: 22,
  SetTabContextPermissionState: 23,
  SetContextAccessIndicator: 24,
  GetUserProfileInfo: 25,
  RefreshSignInCookies: 26,
  AttachPanel: 27,
  DetachPanel: 28,
  SetAudioDucking: 29,
  LogBeginAsyncEvent: 30,
  LogEndAsyncEvent: 31,
  LogInstantEvent: 32,
  JournalClear: 33,
  JournalSnapshot: 34,
  JournalStart: 35,
  JournalStop: 36,
  JournalRecordFeedback: 37,
  OnUserInputSubmitted: 38,
  OnResponseRated: 39,
  OnResponseStarted: 40,
  OnResponseStopped: 41,
  OnSessionTerminated: 42,
  OnTurnCompleted: 43,
  // Do not reuse deleted request ID: 44,
  ScrollTo: 45,
  SetSyntheticExperimentState: 46,
  OpenOsPermissionSettingsMenu: 47,
  GetOsMicrophonePermissionStatus: 48,
  PinTabs: 49,
  UnpinTabs: 50,
  UnpinAllTabs: 51,
  SubscribeToPinCandidates: 52,
  UnsubscribeFromPinCandidates: 53,
  GetZeroStateSuggestionsForFocusedTab: 54,
  GetZeroStateSuggestionsAndSubscribe: 55,
  SetClosedCaptioningSetting: 56,
  DropScrollToHighlight: 57,
  MaybeRefreshUserStatus: 58,
  OnClosedCaptionsShown: 59,
  CreateTask: 60,
  PerformActions: 61,
  // Do not reuse deleted request ID: 62,
  SubscribeToPageMetadata: 63,
  SwitchConversation: 64,
  RegisterConversation: 65,
  OnReaction: 66,
  OnContextUploadCompleted: 67,
  OnContextUploadStarted: 68,
  SetActuationOnWebSetting: 69,
  OnModeChange: 70,
  SubscribeToCaptureRegion: 71,
  UnsubscribeFromCaptureRegion: 72,
  // Do not reuse deleted request ID: 73,
  InterruptActorTask: 74,
  UninterruptActorTask: 75,
  ActivateTab: 76,
  CreateActorTab: 77,
  OpenPasswordManagerSettingsPage: 78,
  SetOnboardingCompleted: 80,
  SubscribeToTabData: 81,
  CreateSkill: 82,
  UpdateSkill: 83,
  GetSkill: 84,
  CancelActions: 85,
  ShowManageSkillsUi: 86,
  AutofillSuggestionDialogOnFormPresented: 87,
  AutofillSuggestionDialogOnFormPreviewChanged: 88,
  AutofillSuggestionDialogOnFormConfirmed: 89,
  OnMicrophoneStatusChange: 90,
  RecordSkillsWebClientEvent: 91,
  DeleteCapturedRegion: 92,
  OnActionSubmitted: 93,
  SubscribeToTabFavicon: 94,
  ShowBrowseSkillsUi: 95,
  SubscribeToZoomLevel: 96,
  UnsubscribeFromZoomLevel: 97,
  OnExperimentalTriggeringUpdate: 98,
  OnOptinImpression: 99,
  ProcessCounterAbuseVerdict: 100,
} as const satisfies HostRequestEnumNamesType;
// LINT.ThenChange(
//  //tools/metrics/histograms/metadata/glic/histograms.xml:ApiRequestType,
//  //tools/metrics/histograms/metadata/glic/enums.xml:GlicHostApiRequestType)

export const HOST_REQUEST_TYPES: HostRequestEnumNamesType&
    {MAX_VALUE: number} = {
      ...RECORDED_REQUEST_IDS,
      MAX_VALUE: Math.max(...Object.values(RECORDED_REQUEST_IDS)),
    };

// Provides metrics histogram information for a host request type.
export interface HostRequestHistogramInfo {
  // The name of the host request type, used as histogram suffix.
  name: string;
  // The histogram enum value for this host request type.
  id: number;
}

export function getHostRequestHistogramInfo(requestType: string):
    HostRequestHistogramInfo|undefined {
  if (!requestType.startsWith('glicBrowser')) {
    return undefined;
  }
  const requestName = requestType.substring(11);
  const id: number|undefined =
      (HOST_REQUEST_TYPES as unknown as Record<string, number>)[requestName];
  if (id === undefined) {
    return undefined;
  }
  return {name: requestName, id: id};
}

export type AllRequestTypes = {
  [K in keyof(HostRequestTypes&WebClientRequestTypes)]:
      (HostRequestTypes&WebClientRequestTypes)[K]
};
// All request types which do not provide a return.
export type AllRequestTypesWithoutReturn = {
  [K in keyof AllRequestTypes as
       RequestResponseType<K> extends void ? K : never]: AllRequestTypes[K]
};
export type AllRequestTypesWithReturn = {
  [K in keyof AllRequestTypes as
       RequestResponseType<K> extends void ? never : K]: AllRequestTypes[K]
};

export type RequestRequestType<T extends keyof AllRequestTypes> =
    'request' extends keyof AllRequestTypes[T] ? AllRequestTypes[T]['request'] :
                                                 undefined;
export type RequestResponseType<T extends keyof AllRequestTypes> =
    'response' extends keyof AllRequestTypes[T] ?
    AllRequestTypes[T]['response'] :
    void;

assertNever<CheckStructuredClonable<HostRequestTypes>>();
assertNever<CheckStructuredClonable<WebClientRequestTypes>>();
// Message names should be unique.
assertNever<keyof WebClient&keyof ActorClient>();
assertNever<keyof WebClientHost&keyof ActorHost>();

//
// Types used in messages that are not exposed directly to the API.
//
// Some types cannot be directly transported over postMessage. The pattern we
// use here is to define a new type with a 'Private' suffix, which replaces the
// property types that cannot be structured cloned, with types that can.
//
// Note that it's a good idea to replace properties with new properties that
// have the same name, but different type. This ensures that we don't
// accidentally leave the private data on the returned object.
//


export type WebClientInitialStatePrivate =
    ReplaceProperties<WebClientInitialState, {
      panelState: PanelState,
      chromeVersion: ChromeVersion,
      platform: Platform,
      formFactor: FormFactor,
      focusedTabData: FocusedTabDataPrivate,
      loggingEnabled: boolean,
      maxInFlightRequests: number,
      sendResponsesForAllRequests: boolean,
      enableZeroStateSuggestions: boolean,
      enableCachedGetUserProfileInfo: boolean,
      hostCapabilities: HostCapability[],
    }>;

// TabData format for postMessage transport.
export declare interface TabDataPrivate extends Omit<TabData, 'favicon'> {
  favicon?: RgbaImage;
}

export declare interface PinCandidatePrivate extends
    Omit<PinCandidate, 'tabData'> {
  tabData: TabDataPrivate;
}

// A bitmap, used to store data from a BitmapN32 without conversion.
export declare interface RgbaImage {
  dataRGBA: ArrayBuffer;
  width: number;
  height: number;
  alphaType: ImageAlphaType;
  colorType: ImageColorType;
}

export enum ImageAlphaType {
  // RGB values are unmodified.
  UNPREMUL = 0,
  // RGB values have been premultiplied by alpha.
  PREMUL = 1,
}

// Chromium currently only uses a single color type for BitmapN32.
export enum ImageColorType {
  BGRA = 0,
  RGBA = 1,
}

// Types of subscriber observations that may be observed.
export enum SubscriberObservationType {
  // An update was observed.
  UPDATE = 0,
  // Completed all observations.
  COMPLETE = 1,
  // An unexpected error was observed.
  ERROR = 2,
}

// FocusedTabData data for postMessage transport.
export declare interface FocusedTabDataPrivate {
  hasFocus?: Omit<FocusedTabDataHasFocus, 'tabData'>&{tabData: TabDataPrivate};
  hasNoFocus?: Omit<FocusedTabDataHasNoFocus, 'tabFocusCandidateData'>&
      {tabFocusCandidateData?: TabDataPrivate};
}

// TabContextResult data for postMessage transport.
export declare interface TabContextResultPrivate extends
    Omit<TabContextResult, 'tabData'|'pdfDocumentData'|'annotatedPageData'> {
  tabData: TabDataPrivate;
  pdfDocumentData?: PdfDocumentDataPrivate;
  annotatedPageData?: AnnotatedPageDataPrivate;
}

// ResumeActorTaskResult data for postMessage transport.
export declare interface ResumeActorTaskResultPrivate extends Omit<
    ResumeActorTaskResult, 'tabData'|'pdfDocumentData'|'annotatedPageData'> {
  tabData: TabDataPrivate;
  pdfDocumentData?: PdfDocumentDataPrivate;
  annotatedPageData?: AnnotatedPageDataPrivate;
}

export declare interface UserProfileInfoPrivate extends
    Omit<UserProfileInfo, 'avatarIcon'> {
  avatarIcon?: RgbaImage;
}

export declare interface PdfDocumentDataPrivate extends
    Omit<PdfDocumentData, 'pdfData'> {
  pdfData?: ArrayBuffer;
}

export declare interface AnnotatedPageDataPrivate extends
    Omit<AnnotatedPageData, 'annotatedPageContent'> {
  annotatedPageContent?: ArrayBuffer;
  metadata?: PageMetadata;
}

export declare interface AdditionalContextPartPrivate extends
    Omit<AdditionalContextPart, 'annotatedPageData'|'pdf'|'data'|'tabContext'> {
  annotatedPageData?: AnnotatedPageDataPrivate;
  pdf?: PdfDocumentDataPrivate;
  data?: {mimeType: string, data: ArrayBuffer};
  tabContext?: TabContextResultPrivate;
}

export declare interface AdditionalContextPrivate extends
    Omit<AdditionalContext, 'parts'> {
  parts: AdditionalContextPartPrivate[];
}

export declare interface InvokeOptionsPrivate extends
    Omit<InvokeOptions, 'context'> {
  context?: AdditionalContextPrivate;
}

export class ErrorWithReasonImpl<T extends keyof ErrorReasonTypes> extends Error
    implements ErrorWithReason<T> {
  constructor(
      public reasonType: T,
      public reason: ErrorReasonTypes[T],
      message?: string,
  ) {
    super(message ?? `${reasonType} Error: ${reason}`);
  }
}

/** Any ErrorWithReason<T>.reason type. */
export type AnyErrorReasonType = ErrorReasonTypes[keyof ErrorReasonTypes];
/** Any ErrorWithReason type. */
export type AnyErrorWithReasonType = ErrorWithReason<keyof ErrorReasonTypes>;
/** Sent in ResponseMessage to reconstruct the ErrorWithReason. */
export interface ErrorWithReasonDetails {
  reason: AnyErrorReasonType;
  reasonType: keyof ErrorReasonTypes;
}

// Exception information that can be passed across postMessage.
export interface TransferableException {
  // An error that occurred during processing the request.
  exception: Error;
  // This may be set to indicate that the exception is a ErrorWithReason
  // exception.
  exceptionReason?: ErrorWithReasonDetails;
}

// Constructs an exception from a TransferableException.
export function exceptionFromTransferable(e: TransferableException): Error|
    AnyErrorWithReasonType {
  // Error types are serializable, but they do not serialize all members.
  // If exceptionReason is provided, we use it to reconstruct a
  // ErrorWithReason by just setting additional fields after
  // serialization.
  if (e.exceptionReason !== undefined) {
    const withReason = e.exception as AnyErrorWithReasonType;
    withReason.reason = e.exceptionReason.reason;
    withReason.reasonType = e.exceptionReason.reasonType;
  }

  return e.exception;
}

// Transform an Error into a TransferableException.
export function newTransferableException(e: Error): TransferableException {
  let exceptionReason = undefined;
  const maybeWithReason = e as Partial<AnyErrorWithReasonType>;
  if (maybeWithReason.reasonType !== undefined &&
      maybeWithReason.reason !== undefined) {
    exceptionReason = {
      reason: maybeWithReason.reason,
      reasonType: maybeWithReason.reasonType,
    };
  }
  return {exception: e, exceptionReason};
}
