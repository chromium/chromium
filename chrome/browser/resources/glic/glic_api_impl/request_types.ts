// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebClientInitialState} from '../glic.mojom-webui.js';
import type {ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason, AdditionalContext, AdditionalContextPart, AnnotatedPageData, AutofillSuggestion, CaptureRegionErrorReason, CaptureRegionResult, ChromeVersion, ConversationInfo, Credential, DraggableArea, ErrorReasonTypes, ErrorWithReason, FocusedTabDataHasFocus, FocusedTabDataHasNoFocus, FormFillingRequest, GetPinCandidatesOptions, HostCapability, Journal, MetricUserInputReactionType, NavigationConfirmationRequest, NavigationConfirmationResponse, OnResponseStoppedDetails, OpenPanelInfo, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, PinCandidate, Platform, ResumeActorTaskResult, Screenshot, ScrollToParams, SelectAutofillSuggestionsDialogRequest, SelectAutofillSuggestionsDialogResponse, SelectCredentialDialogRequest, SelectCredentialDialogResponse, TabContextOptions, TabContextResult, TabData, TaskOptions, UserConfirmationDialogRequest, UserConfirmationDialogResponse, UserProfileInfo, ViewChangedNotification, ViewChangeRequest, WebClientMode, ZeroStateSuggestions, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../glic_api/glic_api.js';

/*
This file defines messages sent over postMessage in-between the Glic WebUI
and the Glic web client.

Most requests closely match signatures of API methods. Where possible, name
messages by concatenating the interface name with the method name. This helps
readability, and ensures that each name is unique.
*/

/**
 * Defines a request and optionally a corresponding response messages.
 */
export interface RequestDef {
  // The type of payload sent. Defaults to 'undefined', which means the request
  // has no request payload.
  request?: any;
  // The type of response payload. Defaults to 'void', which means the request
  // sends no response payload.
  response?: any;
  /**
   * Whether the request can be processed in the background.
   *
   * If true, the request is allowed to be sent and serviced in the
   * background.
   * If false (the default if omitted):
   * For Host requests, `BACKGROUND_RESPONSES` defines how these are handled.
   * For Client requests, it affects usage of `GatedSender`.
   */
  backgroundAllowed?: boolean;
}

// Validates each key is a RequestDef.
type ValidateRequestMap<T extends Record<string, RequestDef>> = T;

// Types of requests to the host (Chrome).
export declare type HostRequestTypes = ValidateRequestMap<{
  // This message is sent just before calling initialize() on the web client.
  // It is not part of the GlicBrowserHost public API.
  glicBrowserWebClientCreated: {
    response: {
      initialState: WebClientInitialStatePrivate,
    },
    backgroundAllowed: true,
  },
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
  },

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
  },
  glicBrowserOpenGlicSettingsPage: {
    request: {options?: OpenSettingsOptions},
    backgroundAllowed: true,
  },
  glicBrowserOpenPasswordManagerSettingsPage: {
    backgroundAllowed: true,
  },
  glicBrowserClosePanel: {
    backgroundAllowed: true,
  },
  glicBrowserClosePanelAndShutdown: {
    backgroundAllowed: true,
  },
  glicBrowserShowProfilePicker: {},
  glicBrowserGetModelQualityClientId: {
    response: {
      modelQualityClientId: string,
    },
    backgroundAllowed: true,
  },
  glicBrowserSwitchConversation: {
    request: {
      info?: ConversationInfo,
    },
    response: {},
    backgroundAllowed: true,
  },
  glicBrowserRegisterConversation: {
    request: {
      info: ConversationInfo,
    },
    response: {},
    backgroundAllowed: true,
  },
  glicBrowserGetContextFromFocusedTab: {
    request: {
      options: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
    backgroundAllowed: false,
  },
  glicBrowserGetContextFromTab: {
    backgroundAllowed: false,
    request: {
      tabId: string,
      options: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
  },
  glicBrowserGetContextForActorFromTab: {
    request: {
      tabId: string,
      options: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
    backgroundAllowed: true,
  },
  glicBrowserSetMaximumNumberOfPinnedTabs: {
    request: {
      requestedMax: number,
    },
    response: {
      effectiveMax: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserCreateTask: {
    request: {
      taskOptions?: TaskOptions,
    },
    response: {
      taskId: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserPerformActions: {
    request: {
      actions: ArrayBuffer,
    },
    response: {
      actionsResult: ArrayBuffer,
    },
    backgroundAllowed: true,
  },
  glicBrowserStopActorTask: {
    request: {
      taskId: number,
      stopReason: ActorTaskStopReason,
    },
    backgroundAllowed: true,
  },
  glicBrowserPauseActorTask: {
    request: {
      taskId: number,
      pauseReason: ActorTaskPauseReason,
      tabId: string,
    },
    backgroundAllowed: true,
  },
  glicBrowserResumeActorTask: {
    request: {
      taskId: number,
      tabContextOptions: TabContextOptions,
    },
    response: {
      resumeActorTaskResult: ResumeActorTaskResultPrivate,
    },
    backgroundAllowed: true,
  },
  glicBrowserInterruptActorTask: {
    request: {
      taskId: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserUninterruptActorTask: {
    request: {
      taskId: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserCreateActorTab: {
    request: {
      taskId: number,
      options: {
        initiatorTabId?: string,
        initiatorWindowId?: string,
        openInBackground?: boolean,
      },
    },
    response: {
      // Undefined on failure.
      tabData?: TabDataPrivate,
    },
    backgroundAllowed: true,
  },
  glicBrowserActivateTab: {
    request: {
      tabId: string,
    },
    backgroundAllowed: true,
  },
  glicBrowserCaptureScreenshot: {
    response: {
      screenshot: Screenshot,
    },
    backgroundAllowed: false,
  },
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
  },
  glicBrowserEnableDragResize: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserSetWindowDraggableAreas: {
    request: {
      areas: DraggableArea[],
    },
    backgroundAllowed: true,
  },
  glicBrowserSetMinimumWidgetSize: {
    request: {
      size: {
        width: number,
        height: number,
      },
    },
    backgroundAllowed: true,
  },
  glicBrowserSetMicrophonePermissionState: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserSetLocationPermissionState: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserSetTabContextPermissionState: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserSetClosedCaptioningSetting: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserSetContextAccessIndicator: {
    request: {
      show: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserSetActuationOnWebSetting: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserGetUserProfileInfo: {
    response: {
      profileInfo?: UserProfileInfoPrivate,
    },
    backgroundAllowed: true,
  },
  glicBrowserRefreshSignInCookies: {
    response: {
      success: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserAttachPanel: {
    backgroundAllowed: true,
  },
  glicBrowserDetachPanel: {
    backgroundAllowed: true,
  },
  glicBrowserSetAudioDucking: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserLogBeginAsyncEvent: {
    request: {
      asyncEventId: number,
      taskId: number,
      event: string,
      details: string,
    },
    backgroundAllowed: true,
  },
  glicBrowserLogEndAsyncEvent: {
    request: {
      asyncEventId: number,
      details: string,
    },
    backgroundAllowed: true,
  },
  glicBrowserLogInstantEvent: {
    request: {
      taskId: number,
      event: string,
      details: string,
    },
    backgroundAllowed: true,
  },
  glicBrowserJournalClear: {
    backgroundAllowed: true,
  },
  glicBrowserJournalSnapshot: {
    request: {
      clear: boolean,
    },
    response: {
      journal: Journal,
    },
    backgroundAllowed: true,
  },
  glicBrowserJournalStart: {
    request: {
      maxBytes: number,
      captureScreenshots: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserJournalStop: {
    backgroundAllowed: true,
  },
  glicBrowserJournalRecordFeedback: {
    request: {
      positive: boolean,
      reason: string,
    },
    backgroundAllowed: true,
  },
  glicBrowserOnUserInputSubmitted: {
    request: {
      mode: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserOnReaction: {
    backgroundAllowed: true,
    request: {
      reactionType: MetricUserInputReactionType,
    },
  },
  glicBrowserOnContextUploadStarted: {
    backgroundAllowed: true,
  },
  glicBrowserOnContextUploadCompleted: {
    backgroundAllowed: true,
  },
  glicBrowserOnResponseStarted: {
    backgroundAllowed: true,
  },
  glicBrowserOnResponseStopped: {
    request: {details?: OnResponseStoppedDetails},
    backgroundAllowed: true,
  },
  glicBrowserOnSessionTerminated: {
    backgroundAllowed: true,
  },
  glicBrowserOnTurnCompleted: {
    request: {
      model: number,
      duration: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserOnModelChanged: {
    request: {
      model: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserOnRecordUseCounter: {
    request: {
      counter: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserOnResponseRated: {
    request: {
      positive: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserOnClosedCaptionsShown: {
    backgroundAllowed: true,
  },
  glicBrowserScrollTo: {
    request: {
      params: ScrollToParams,
    },
    backgroundAllowed: false,
  },
  glicBrowserDropScrollToHighlight: {
    backgroundAllowed: true,
  },
  glicBrowserSetSyntheticExperimentState: {
    request: {
      trialName: string,
      groupName: string,
    },
    backgroundAllowed: true,
  },
  glicBrowserOpenOsPermissionSettingsMenu: {request: {permission: string}},
  glicBrowserGetOsMicrophonePermissionStatus: {
    response: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserPinTabs: {
    backgroundAllowed: false,
    request: {
      tabIds: string[],
    },
    response: {
      pinnedAll: boolean,
    },
  },
  glicBrowserUnpinTabs: {
    backgroundAllowed: true,
    request: {
      tabIds: string[],
    },
    response: {
      unpinnedAll: boolean,
    },
  },
  glicBrowserUnpinAllTabs: {
    backgroundAllowed: false,
  },
  glicBrowserSubscribeToPinCandidates: {
    backgroundAllowed: false,
    request: {
      options: GetPinCandidatesOptions,
      observationId: number,
    },
  },
  glicBrowserUnsubscribeFromPinCandidates: {
    request: {
      observationId: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserSubscribeToCaptureRegion: {
    request: {
      observationId: number,
    },
    backgroundAllowed: true,
  },
  glicBrowserUnsubscribeFromCaptureRegion: {
    request: {
      observationId: number,
    },
    backgroundAllowed: true,
  },

  glicBrowserGetZeroStateSuggestionsForFocusedTab: {
    request: {
      isFirstRun?: boolean,
    },
    response: {
      suggestions?: ZeroStateSuggestions,
    },
    backgroundAllowed: false,
  },
  glicBrowserMaybeRefreshUserStatus: {
    backgroundAllowed: true,
  },

  glicBrowserGetZeroStateSuggestionsAndSubscribe: {
    request: {
      hasActiveSubscription: boolean,
      options: ZeroStateSuggestionsOptions,
    },
    response: {
      suggestions?: ZeroStateSuggestionsV2,
    },
  },
  glicBrowserOnViewChanged: {
    request: {
      notification: ViewChangedNotification,
    },
    backgroundAllowed: true,
  },
  glicBrowserSubscribeToPageMetadata: {
    request: {
      tabId: string,
      names: string[],
    },
    response: {
      success: boolean,
    },
    backgroundAllowed: true,
  },
  glicBrowserOnModeChange: {
    request: {
      newMode: WebClientMode,
    },
    backgroundAllowed: true,
  },
  glicBrowserLoadAndExtractContent: {
    request: {
      urls: string[],
      options: TabContextOptions[],
    },
    response: {
      results: TabContextResultPrivate[],
    },
    backgroundAllowed: true,
  },
}>;

// Types of requests to the GlicWebClient.
export declare type WebClientRequestTypes = ValidateRequestMap<{
  glicWebClientNotifyPanelWillOpen: {
    request: {
      panelOpeningData: PanelOpeningData,
    },
    response: {
      openPanelInfo?: OpenPanelInfo,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyPanelWasClosed: {
    backgroundAllowed: true,
  },
  glicWebClientPanelStateChanged: {
    request: {
      panelState: PanelState,
    },
    backgroundAllowed: true,
  },
  glicWebClientRequestViewChange: {
    request: {
      request: ViewChangeRequest,
    },
    backgroundAllowed: true,
  },
  glicWebClientCanAttachStateChanged: {
    request: {
      canAttach: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyMicrophonePermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyLocationPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyTabContextPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyDefaultTabContextPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyOsLocationPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyClosedCaptioningSettingChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyActuationOnWebSettingChanged: {
    request: {
      enabled: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyFocusedTabChanged: {
    request: {
      focusedTabDataPrivate: FocusedTabDataPrivate,
    },
  },
  glicWebClientNotifyPanelActiveChanged: {
    request: {
      panelActive: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientCheckResponsive: {
    backgroundAllowed: true,
  },
  glicWebClientNotifyManualResizeChanged: {
    request: {
      resizing: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientBrowserIsOpenChanged: {
    request: {
      browserIsOpen: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyOsHotkeyStateChanged: {
    request: {
      hotkey: string,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyPinnedTabsChanged: {
    request: {
      tabData: TabDataPrivate[],
    },
  },
  glicWebClientNotifyPinnedTabDataChanged: {
    request: {
      tabData: TabDataPrivate,
    },
  },
  glicWebClientPinCandidatesChanged: {
    request: {
      candidates: PinCandidatePrivate[],
      observationId: number,
    },
  },
  glicWebClientZeroStateSuggestionsChanged: {
    request: {
      suggestions: ZeroStateSuggestionsV2,
      options: ZeroStateSuggestionsOptions,
    },
  },
  glicWebClientNotifyActorTaskStateChanged: {
    request: {
      taskId: number,
      state: ActorTaskState,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyTabDataChanged: {
    request: {
      tabData: TabDataPrivate,
    },
    backgroundAllowed: true,
  },
  glicWebClientPageMetadataChanged: {
    request: {
      tabId: string,
      pageMetadata: PageMetadata | null,
    },
  },
  glicWebClientRequestToShowDialog: {
    request: {
      request: SelectCredentialDialogRequestPrivate,
    },
    response: {
      response: SelectCredentialDialogResponsePrivate,
    },
    backgroundAllowed: true,
  },
  glicWebClientRequestToShowConfirmationDialog: {
    request: {
      request: UserConfirmationDialogRequestPrivate,
    },
    response: {
      response: UserConfirmationDialogResponsePrivate,
    },
    backgroundAllowed: true,
  },
  glicWebClientRequestToConfirmNavigation: {
    request: {
      request: NavigationConfirmationRequestPrivate,
    },
    response: {
      response: NavigationConfirmationResponsePrivate,
    },
    backgroundAllowed: true,
  },
  glicWebClientNotifyAdditionalContext: {
    request: {
      context: AdditionalContextPrivate,
    },
  },
  glicWebClientCaptureRegionUpdate: {
    request: {
      result?: CaptureRegionResult,
      reason?: CaptureRegionErrorReason, observationId: number,
    },
  },
  glicWebClientNotifyActOnWebCapabilityChanged: {
    request: {
      canActOnWeb: boolean,
    },
    backgroundAllowed: true,
  },
  glicWebClientRequestToShowAutofillSuggestionsDialog: {
    request: {
      request: SelectAutofillSuggestionsDialogRequestPrivate,
    },
    response: {
      response: SelectAutofillSuggestionsDialogResponsePrivate,
    },
    backgroundAllowed: true,
  },
}>;


type RemoveStringPrefix<S extends string, Prefix extends string> =
    S extends `${Prefix}${infer Rest}` ? Rest : 'prefixNotFound!';

export type HostRequestEnumNamesType = {
  [K in keyof HostRequestTypes as RemoveStringPrefix<K, 'glicBrowser'>]: number;
};

// LINT.IfChange(ApiRequestType)
// New values here must be added to histograms.xml and to enums.xml.
export const HOST_REQUEST_TYPES: HostRequestEnumNamesType&{MAX_VALUE: number} =
    (() => {
      const result = {
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
        SetWindowDraggableAreas: 19,
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
        OnModelChanged: 44,
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
        OnViewChanged: 62,
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
        OnRecordUseCounter: 73,
        InterruptActorTask: 74,
        UninterruptActorTask: 75,
        ActivateTab: 76,
        CreateActorTab: 77,
        OpenPasswordManagerSettingsPage: 78,
        LoadAndExtractContent: 79,
      };
      return {...result, MAX_VALUE: Math.max(...Object.values(result))};
    })();
// clang-format off
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/histograms.xml:ApiRequestType, //tools/metrics/histograms/metadata/glic/enums.xml:GlicHostApiRequestType)
// clang-format on

export function requestTypeToHistogramSuffix(type: string): string|undefined {
  if (!type.startsWith('glicBrowser')) {
    return undefined;
  }
  return type.substring(11);
}

export type AllRequestTypes = HostRequestTypes&WebClientRequestTypes;
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

type AllValues<T> = T[keyof T];
type ArrayElement<ArrayType extends unknown[]> =
    ArrayType extends Array<infer ElementType>? ElementType : never;

// Do some high level checks that we don't accidentally add a non-cloneable or
// transferable type to our messages. These are not perfect.

// This can be extended for other transferable types when we need them. Using
// 'extends ...' for all possible Transferable types is too permissive.
type TransferableTypes = ArrayBuffer|Blob;
type StructuredClonableBasicType = string|boolean|number|void|undefined|null;
type CheckStructuredClonable<T> =
    T extends StructuredClonableBasicType ? never : T extends any[] ?
    CheckStructuredClonable<ArrayElement<T>>:
    T extends Map<infer K, infer V>?
    (CheckStructuredClonable<K>&CheckStructuredClonable<V>) :
    T extends Function ?
    ['Function not structured cloneable', T] :
    T extends Promise<any>? ['Promise not structured cloneable', T] :
                            CheckStructuredClonableObject<T>;
type CheckStructuredClonableObject<T> = T extends TransferableTypes ?
    never :
    AllValues<{[K in keyof T] -?: CheckStructuredClonable<T[K]>;}>;

/* eslint-disable-next-line @typescript-eslint/naming-convention */
function assertNever<_T extends never>() {}

assertNever<CheckStructuredClonable<HostRequestTypes>>();
assertNever<CheckStructuredClonable<WebClientRequestTypes>>();

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

// Same as A&B, but replaces properties that are in both with those in B.
type ReplaceProperties<A, B> = {
  [K in keyof A |
   keyof B]: K extends keyof B ? B[K] : K extends keyof A ? A[K] : never;
};

export type WebClientInitialStatePrivate =
    ReplaceProperties<WebClientInitialState, {
      panelState: PanelState,
      chromeVersion: ChromeVersion,
      platform: Platform,
      focusedTabData: FocusedTabDataPrivate,
      loggingEnabled: boolean,
      enableZeroStateSuggestions: boolean,
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

export declare interface CredentialPrivate extends Omit<Credential, 'getIcon'> {
}

export declare interface SelectCredentialDialogRequestPrivate extends Omit<
    SelectCredentialDialogRequest, 'onDialogClosed'|'icons'|'credentials'> {
  icons: Map<string, RgbaImage>;
  credentials: CredentialPrivate[];
}

/** Reasons why the credential selection dialog request failed. */
export enum SelectCredentialDialogErrorReason {
  // The hosting WebUI received the request, but the web client has not
  // subscribed to the request yet. We couldn't show the dialog in this case.
  DIALOG_PROMISE_NO_SUBSCRIBER = 0,
}

export declare interface SelectCredentialDialogResponsePrivate extends
    SelectCredentialDialogResponse {
  errorReason?: SelectCredentialDialogErrorReason;
}

export declare interface AutofillSuggestionPrivate extends
    Omit<AutofillSuggestion, 'getIcon'> {
  icon?: RgbaImage;
}

export declare interface FormFillingRequestPrivate extends
    Omit<FormFillingRequest, 'suggestions'> {
  suggestions: AutofillSuggestionPrivate[];
}

export declare interface SelectAutofillSuggestionsDialogRequestPrivate extends
    Omit<
        SelectAutofillSuggestionsDialogRequest,
        'onDialogClosed'|'formFillingRequests'> {
  taskId: number;
  formFillingRequests: FormFillingRequestPrivate[];
}

// LINT.IfChange(SelectAutofillSuggestionsDialogErrorReason)
/** Reasons why the autofill suggestion selection dialog request failed. */
export enum SelectAutofillSuggestionsDialogErrorReason {
  // The hosting WebUI received the request, but the web client has not
  // subscribed to the request yet. We couldn't show the dialog in this case.
  DIALOG_PROMISE_NO_SUBSCRIBER = 0,
  // The requested task id did not match the response task id. This error is
  // internal to the browser and not sent by the client over mojo.
  MISMATCHED_TASK_ID = 1,
  // The task is not connected to a delegate. I.e. attempting to run the task
  // from the experimental actor API. This error is internal to the browser and
  // not sent by the client over mojo.
  NO_ACTOR_TASK_DELEGATE = 2,
}
// LINT.ThenChange(//chrome/common/actor_webui.mojom:SelectAutofillSuggestionsDialogErrorReason)

export declare interface SelectAutofillSuggestionsDialogResponsePrivate extends
    SelectAutofillSuggestionsDialogResponse {
  taskId: number;
  errorReason?: SelectAutofillSuggestionsDialogErrorReason;
}

export declare interface UserConfirmationDialogRequestPrivate extends
    Omit<UserConfirmationDialogRequest, 'onDialogClosed'> {}

export enum ConfirmationRequestErrorReason {
  // The hosting WebUI received the request, but the web client has not
  // subscribed to the request yet. We couldn't show the dialog in this case.
  REQUEST_PROMISE_NO_SUBSCRIBER = 0,
  // The task requested a new user confirmation dialog before the current
  // one completed.
  PREEMPTED_BY_NEW_REQUEST = 1,
}

export declare interface UserConfirmationDialogResponsePrivate extends
    UserConfirmationDialogResponse {
  errorReason?: ConfirmationRequestErrorReason;
}

export declare interface NavigationConfirmationRequestPrivate extends
    Omit<NavigationConfirmationRequest, 'onConfirmationDecision'> {}

export declare interface NavigationConfirmationResponsePrivate extends
    NavigationConfirmationResponse {
  errorReason?: ConfirmationRequestErrorReason;
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
