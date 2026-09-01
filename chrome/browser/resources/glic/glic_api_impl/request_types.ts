// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebClientInitialState} from '../glic.mojom-webui.js';
import type {AdditionalContext, AdditionalContextPart, AnnotatedPageData, CaptureRegionErrorReason, CaptureRegionParams, CaptureRegionResult, ChromeVersion, ClientCapabilities, ClientErrorDialogType, ConversationInfo, CounterAbuseVerdict, ErrorReasonTypes, ErrorWithReason, ExperimentalTriggeringUpdate, FileUploadPolicyState, FocusedTabDataHasFocus, FocusedTabDataHasNoFocus, FormFactor, GeminiEnterpriseSettings, GetPinCandidatesOptions, HostCapability, InvokeOptions, MetricUserInputReactionType, MicrophoneStatus, OnResponseStoppedDetails, OpenPanelInfo, OpenPinnedTabPickerOptions, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, PinCandidate, PinTabsOptions, Platform, PromptType, ResumeActorTaskResult, Screenshot, TabContextOptions, TabContextResult, TabData, UnpinTabsOptions, UserProfileInfo, WebClientMode, ZeroStateSuggestions} from '../glic_api/glic_api.js';

import type {ActorClient, ActorHost} from './actor/actor_types.js';
import type {AnnotationClient, AnnotationHost} from './annotation/annotation_types.js';
import type {ExperimentalTriggeringClient} from './experimental_triggering/experimental_triggering_types.js';
import type {InterfaceDef, InterfaceDefMethods, ReplaceProperties} from './transport/messaging.js';
import {defInterface, defMessage} from './transport/messaging.js';
import type {ErrorCodec, PendingReceiver, PendingRemote, TransferableException} from './transport/post_message_transport.js';
import type {ZeroStateSuggestionsClient, ZeroStateSuggestionsHost} from './zero_state_suggestions/zero_state_suggestions_types.js';

export type {
  ActorClient,
  ActorHost,
  AnnotationClient,
  AnnotationHost,
  ExperimentalTriggeringClient,
  ZeroStateSuggestionsClient,
  ZeroStateSuggestionsHost,
};

/*
This file defines messages sent over postMessage in-between the Glic WebUI
and the Glic web client.

Most requests closely match signatures of API methods. Where possible, name
messages by concatenating the interface name with the method name. This helps
readability, and ensures that each name is unique.
*/

export const WebClientHostDef = defInterface({
  name: 'WebClientHost',
  methods: [
    {
      name: 'webClientCreated',
      request: defMessage<{
        clientCapabilities: ClientCapabilities[],
      }>(),
      response: defMessage<{
        initialState: WebClientInitialStatePrivate,
        actorRemote?: PendingRemote<ActorHost>,
        actorReceiver?: PendingReceiver<ActorClient>,
        experimentalTriggeringReceiver?: PendingReceiver<
                                          ExperimentalTriggeringClient>,
        zeroStateSuggestionsRemote?: PendingRemote<ZeroStateSuggestionsHost>,
      }>(),
      histogram: {name: 'WebClientCreated', id: 1},
    },
    {
      name: 'webClientInitialized',
      request: defMessage<{
        success: boolean,
        // Exception present if initialize() returns a rejected promise
        // (success is false).
        exception?: GlicException,
      }>(),
      histogram: {id: 2},
    },
    {
      name: 'onExperimentalTriggeringUpdate',
      request: defMessage<{
        observationId: number,
        update?: ExperimentalTriggeringUpdate,
              observation: SubscriberObservationType,
      }>(),
      histogram: {id: 98},
    },
    {
      name: 'createTab',
      request: defMessage<{
        url: string,
        options: {openInBackground?: boolean, windowId?: string},
      }>(),
      response: defMessage<{
        // Undefined on failure.
        tabData?: TabDataPrivate,
      }>(),
      histogram: {id: 3},
    },
    {
      name: 'activateTabWithUrl',
      request: defMessage<{
        exactUrl: string,
        options: {
          pattern?: string,
          fallbackWindowId?: string,
        },
      }>(),
      response: defMessage<{
        // Undefined on failure.
        tabData?: TabDataPrivate,
      }>(),
      histogram: {id: 102},
    },
    {
      name: 'openGlicSettingsPage',
      request: defMessage<{options?: OpenSettingsOptions}>(),
      histogram: {id: 4},
    },
    {
      name: 'openPasswordManagerSettingsPage',
      histogram: {id: 78},
    },
    {
      name: 'closePanel',
      histogram: {id: 5},
    },
    {
      name: 'closePanelAndShutdown',
      histogram: {id: 6},
    },
    {
      name: 'showProfilePicker',
      histogram: {id: 7},
    },
    {
      name: 'getModelQualityClientId',
      response: defMessage<{
        modelQualityClientId: string,
      }>(),
      histogram: {id: 8},
    },
    {
      name: 'switchConversation',
      request: defMessage<{
        info?: ConversationInfo,
      }>(),
      response: defMessage<{}>(),
      histogram: {id: 64},
    },
    {
      name: 'registerConversation',
      request: defMessage<{
        info: ConversationInfo,
      }>(),
      response: defMessage<{}>(),
      histogram: {id: 65},
    },
    {
      name: 'getContextFromFocusedTab',
      request: defMessage<{
        options: TabContextOptions,
      }>(),
      response: defMessage<{
        tabContextResult: TabContextResultPrivate,
      }>(),
      histogram: {id: 9},
    },
    {
      name: 'getContextFromTab',
      request: defMessage<{
        tabId: string,
        options: TabContextOptions,
      }>(),
      response: defMessage<{
        tabContextResult: TabContextResultPrivate,
      }>(),
      histogram: {id: 10},
    },
    {
      name: 'getImageBytesFromTab',
      request: defMessage<{
        tabId: string,
        documentId: string,
        domNodeId: number,
      }>(),
      response: defMessage<{
        result: ImageBytesResultPrivate | null,
      }>(),
      histogram: {id: 101},
    },
    {
      name: 'setMaximumNumberOfPinnedTabs',
      request: defMessage<{
        requestedMax: number,
      }>(),
      response: defMessage<{
        effectiveMax: number,
      }>(),
      histogram: {id: 12},
    },
    {
      name: 'activateTab',
      request: defMessage<{
        tabId: string,
      }>(),
      histogram: {id: 76},
    },
    {
      name: 'captureScreenshot',
      response: defMessage<{
        screenshot: Screenshot,
      }>(),
      histogram: {id: 16},
    },
    {
      name: 'resizeWindow',
      request: defMessage<{
        size: {
          width: number,
          height: number,
        },
        options?: {
          durationMs?: number,
        },
      }>(),
      histogram: {id: 17},
    },
    {
      name: 'enableDragResize',
      request: defMessage<{
        enabled: boolean,
      }>(),
      histogram: {id: 18},
    },
    {
      name: 'setMinimumWidgetSize',
      request: defMessage<{
        size: {
          width: number,
          height: number,
        },
      }>(),
      histogram: {id: 20},
    },
    {
      name: 'setMicrophonePermissionState',
      request: defMessage<{
        enabled: boolean,
      }>(),
      histogram: {id: 21},
    },
    {
      name: 'setLocationPermissionState',
      request: defMessage<{
        enabled: boolean,
      }>(),
      histogram: {id: 22},
    },
    {
      name: 'setTabContextPermissionState',
      request: defMessage<{
        enabled: boolean,
      }>(),
      histogram: {id: 23},
    },
    {
      name: 'setClosedCaptioningSetting',
      request: defMessage<{
        enabled: boolean,
      }>(),
      histogram: {id: 56},
    },
    {
      name: 'setContextAccessIndicator',
      request: defMessage<{
        show: boolean,
      }>(),
      histogram: {id: 24},
    },
    {
      name: 'setActuationOnWebSetting',
      request: defMessage<{
        enabled: boolean,
      }>(),
      histogram: {id: 69},
    },
    {
      name: 'getUserProfileInfo',
      response: defMessage<{
        profileInfo?: UserProfileInfoPrivate,
      }>(),
      histogram: {id: 25},
    },
    {
      name: 'refreshSignInCookies',
      response: defMessage<{
        success: boolean,
      }>(),
      histogram: {id: 26},
    },
    {
      name: 'attachPanel',
      histogram: {id: 27},
    },
    {
      name: 'detachPanel',
      histogram: {id: 28},
    },
    {
      name: 'setAudioDucking',
      request: defMessage<{
        enabled: boolean,
      }>(),
      histogram: {id: 29},
    },
    {
      name: 'onUserInputSubmitted',
      request: defMessage<{
        mode: number,
        promptType?: PromptType,
      }>(),
      histogram: {id: 38},
    },
    {
      name: 'onReaction',
      request: defMessage<{
        reactionType: MetricUserInputReactionType,
      }>(),
      histogram: {id: 66},
    },
    {
      name: 'onOptinImpression',
      histogram: {id: 99},
    },
    {
      name: 'onContextUploadStarted',
      histogram: {id: 68},
    },
    {
      name: 'onContextUploadCompleted',
      histogram: {id: 67},
    },
    {
      name: 'onResponseStarted',
      histogram: {id: 40},
    },
    {
      name: 'onResponseStopped',
      request: defMessage<{details?: OnResponseStoppedDetails}>(),
      histogram: {id: 41},
    },
    {
      name: 'onSessionTerminated',
      histogram: {id: 42},
    },
    {
      name: 'onTurnCompleted',
      request: defMessage<{
        model: number,
        duration: number,
      }>(),
      histogram: {id: 43},
    },
    {
      name: 'onResponseRated',
      request: defMessage<{
        positive: boolean,
      }>(),
      histogram: {id: 39},
    },
    {
      name: 'onClosedCaptionsShown',
      histogram: {id: 59},
    },
    {
      name: 'onActionSubmitted',
      request: defMessage<{
        isRetry?: boolean,
      }>(),
      histogram: {id: 93},
    },
    {
      name: 'setSyntheticExperimentState',
      request: defMessage<{
        trialName: string,
        groupName: string,
      }>(),
      histogram: {id: 46},
    },
    {
      name: 'openOsPermissionSettingsMenu',
      request: defMessage<{permission: string}>(),
      histogram: {id: 47},
    },
    {
      name: 'getOsMicrophonePermissionStatus',
      response: defMessage<{
        enabled: boolean,
      }>(),
      histogram: {id: 48},
    },
    {
      name: 'pinTabs',
      request: defMessage<{
        tabIds: string[],
        options?: PinTabsOptions,
      }>(),
      response: defMessage<{
        pinnedAll: boolean,
      }>(),
      histogram: {id: 49},
    },
    {
      name: 'unpinTabs',
      request: defMessage<{
        tabIds: string[],
        options?: UnpinTabsOptions,
      }>(),
      response: defMessage<{
        unpinnedAll: boolean,
      }>(),
      histogram: {id: 50},
    },
    {
      name: 'unpinAllTabs',
      request: defMessage<{
        options?: UnpinTabsOptions,
      }>(),
      histogram: {id: 51},
    },
    {
      name: 'subscribeToPinCandidates',
      request: defMessage<{
        options: GetPinCandidatesOptions,
        pinCandidatesPipe: PendingRemote<WebClientPinCandidatesObserver>,
      }>(),
      histogram: {id: 52},
    },
    {
      name: 'openPinnedTabPicker',
      request: defMessage<{options?: OpenPinnedTabPickerOptions}>(),
      histogram: {id: 104},
    },
    {
      name: 'subscribeToCaptureRegion',
      request: defMessage<{
        remote: PendingRemote<WebClientRegionCapture>,
        params?: CaptureRegionParams,
      }>(),
      histogram: {id: 71},
    },
    {
      name: 'deleteCapturedRegion',
      request: defMessage<{
        tabId: string,
        regionId: string,
      }>(),
      histogram: {id: 92},
    },
    {
      name: 'getZeroStateSuggestionsForFocusedTab',
      request: defMessage<{
        isFirstRun?: boolean,
      }>(),
      response: defMessage<{
        suggestions?: ZeroStateSuggestions,
      }>(),
      histogram: {id: 54},
    },
    {
      name: 'maybeRefreshUserStatus',
      histogram: {id: 58},
    },

    {
      name: 'subscribeToPageMetadata',
      request: defMessage<{
        tabId: string,
        names: string[],
      }>(),
      response: defMessage<{
        success: boolean,
      }>(),
      histogram: {id: 63},
    },
    {
      name: 'onModeChange',
      request: defMessage<{
        newMode: WebClientMode,
      }>(),
      histogram: {id: 70},
    },
    {
      name: 'setOnboardingCompleted',
      histogram: {id: 80},
    },
    {
      name: 'subscribeToTabData',
      request: defMessage<{
        tabId: string,
        remote: PendingRemote<WebClientTabDataObserver>,
      }>(),
      histogram: {id: 81},
    },
    {
      name: 'subscribeToTabFavicon',
      request: defMessage<{
        tabId: string,
        remote: PendingRemote<WebClientTabFaviconObserver>,
      }>(),
      histogram: {id: 94},
    },
    {
      name: 'onMicrophoneStatusChange',
      request: defMessage<{
        status: MicrophoneStatus,
      }>(),
      histogram: {id: 90},
    },
    {
      name: 'recordHistogram',
      request: defMessage<{
        name: string,
        sparseValue: number,
        // Add other histogram types as needed.
      }>(),
    },
    {
      name: 'setErrorDialogState',
      request: defMessage<{
        shownDialogType?: ClientErrorDialogType,
      }>(),
    },
    {
      name: 'reportClientTransientError',
      request: defMessage<{
        abslStatus: number,
      }>(),
    },
    {
      name: 'processCounterAbuseVerdict',
      request: defMessage<{
        tabId: string,
        verdict: CounterAbuseVerdict,
      }>(),
      histogram: {id: 100},
    },
    {
      name: 'subscribeToZoomLevel',
      histogram: {id: 96},
    },
    {
      name: 'unsubscribeFromZoomLevel',
      histogram: {id: 97},
    },
    {
      name: 'createAnnotationHandler',
      request: defMessage<{
        annotationReceiver: PendingReceiver<AnnotationHost>,
      }>(),
    },
  ],
});


export type WebClientHost = typeof WebClientHostDef;

// Types of requests to the GlicWebClient.
export const WebClientDef = defInterface({
  name: 'WebClient',
  methods: [
    {
      name: 'notifyPanelWillOpen',
      request: defMessage<{
        panelOpeningData: PanelOpeningData,
      }>(),
      response: defMessage<{
        openPanelInfo?: OpenPanelInfo,
      }>(),
    },
    {
      name: 'notifyPanelWasClosed',
    },
    {
      name: 'stopMicrophone',
    },
    {
      name: 'panelStateChanged',
      request: defMessage<{
        panelState: PanelState,
      }>(),
    },
    {
      name: 'canAttachStateChanged',
      request: defMessage<{
        canAttach: boolean,
      }>(),
    },
    {
      name: 'notifyGeminiEnterpriseSettingsChanged',
      request: defMessage<{
        settings: GeminiEnterpriseSettings | undefined,
      }>(),
    },
    {
      name: 'notifyMicrophonePermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
    },
    {
      name: 'notifyLocationPermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
    },
    {
      name: 'notifyTabContextPermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
    },
    {
      name: 'notifyDefaultTabContextPermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
    },
    {
      name: 'notifyOsLocationPermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
    },
    {
      name: 'notifyClosedCaptioningSettingChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
    },
    {
      name: 'notifyActuationOnWebSettingChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
    },
    {
      name: 'notifyFileUploadStateChanged',
      request: defMessage<{
        state: FileUploadPolicyState,
      }>(),
    },
    {
      name: 'notifyFocusedTabChanged',
      request: defMessage<{
        focusedTabDataPrivate: FocusedTabDataPrivate,
      }>(),
    },
    {
      name: 'notifyPanelActiveChanged',
      request: defMessage<{
        panelActive: boolean,
      }>(),
    },
    {
      name: 'checkResponsive',
    },
    {
      name: 'notifyManualResizeChanged',
      request: defMessage<{
        resizing: boolean,
      }>(),
    },
    {
      name: 'browserIsOpenChanged',
      request: defMessage<{
        browserIsOpen: boolean,
      }>(),
    },
    {
      name: 'notifyOsHotkeyStateChanged',
      request: defMessage<{
        hotkey: string,
      }>(),
    },
    {
      name: 'notifyPinnedTabsChanged',
      request: defMessage<{
        tabData: TabDataPrivate[],
      }>(),
    },
    {
      name: 'notifyPinnedTabDataChanged',
      request: defMessage<{
        tabData: TabDataPrivate,
      }>(),
    },

    {
      name: 'pageMetadataChanged',
      request: defMessage<{
        tabId: string,
        pageMetadata: PageMetadata | null,
      }>(),
    },
    {
      name: 'notifyAdditionalContext',
      request: defMessage<{
        context: AdditionalContextPrivate,
      }>(),
    },

    {
      name: 'notifyActOnWebCapabilityChanged',
      request: defMessage<{
        canActOnWeb: boolean,
      }>(),
    },
    {
      name: 'onboardingCompletedChanged',
      request: defMessage<{
        completed: boolean,
      }>(),
    },
    {
      name: 'notifyActorTaskListRowClicked',
      request: defMessage<{
        taskId: number,
      }>(),
    },
    {
      name: 'invoke',
      request: defMessage<{
        options: InvokeOptionsPrivate,
      }>(),
    },
    {
      name: 'notifyZoomLevelChanged',
      request: defMessage<{
        zoomFactor: number,
      }>(),
    },
  ],
});

export type WebClient = typeof WebClientDef;

export const WebClientRegionCaptureDef = defInterface({
  name: 'WebClientRegionCapture',
  methods: [
    {
      name: 'captureRegionUpdate',
      request: defMessage<{
        result?: CaptureRegionResult,
        reason?: CaptureRegionErrorReason,
      }>(),
    },
  ],
});
export type WebClientRegionCapture = typeof WebClientRegionCaptureDef;

export const WebClientPinCandidatesObserverDef = defInterface({
  name: 'WebClientPinCandidatesObserver',
  methods: [
    {
      name: 'pinCandidatesChanged',
      request: defMessage<{
        candidates: PinCandidatePrivate[],
      }>(),
    },
  ],
});
export type WebClientPinCandidatesObserver =
    typeof WebClientPinCandidatesObserverDef;

export const WebClientTabDataObserverDef = defInterface({
  name: 'WebClientTabDataObserver',
  methods: [
    {
      name: 'tabDataChanged',
      request: defMessage<{
        tabData: TabDataPrivate,
      }>(),
    },
  ],
});
export type WebClientTabDataObserver = typeof WebClientTabDataObserverDef;

export const WebClientTabFaviconObserverDef = defInterface({
  name: 'WebClientTabFaviconObserver',
  methods: [
    {
      name: 'tabFaviconChanged',
      request: defMessage<{
        favicon?: RgbaImage,
      }>(),
    },
  ],
});
export type WebClientTabFaviconObserver = typeof WebClientTabFaviconObserverDef;

export type WebClientRequestTypes =
    InterfaceDefMethods<WebClient>&InterfaceDefMethods<ActorClient>&
    InterfaceDefMethods<WebClientRegionCapture>&
    InterfaceDefMethods<WebClientPinCandidatesObserver>&
    InterfaceDefMethods<WebClientTabDataObserver>&
    InterfaceDefMethods<WebClientTabFaviconObserver>;

export type HostRequestTypes = InterfaceDefMethods<WebClientHost>&
    InterfaceDefMethods<ActorHost>&InterfaceDefMethods<AnnotationHost>;

type InterfaceHistogramIds<I extends InterfaceDef> = {
  [M in I['methods'][number] as M['histogram'] extends {id: number} ?
       (M['histogram'] extends {name: infer Name extends string} ?
            Name :
            Capitalize<M['name']&string>) :
       never]: M['histogram'] extends {id: infer Id} ? Id : never;
};

// Note: We are migrating API request reporting to C++. This list should be
// trimmed down and fully deleted eventually.
// See chrome/browser/glic/public/glic_api_metrics.h.
export const RECORDED_REQUEST_IDS = {
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
  // Do not reuse deleted request ID: 53,
  GetZeroStateSuggestionsForFocusedTab: 54,
  // Do not reuse deleted request ID: 55,
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
  // Do not reuse deleted request ID: 72,
  // Do not reuse deleted request ID: 73,
  InterruptActorTask: 74,
  UninterruptActorTask: 75,
  ActivateTab: 76,
  CreateActorTab: 77,
  OpenPasswordManagerSettingsPage: 78,
  SetOnboardingCompleted: 80,
  SubscribeToTabData: 81,
  // Do not reuse deleted request ID: 82,
  // Do not reuse deleted request ID: 83,
  // Do not reuse deleted request ID: 84,
  CancelActions: 85,
  // Do not reuse deleted request ID: 86,
  AutofillSuggestionDialogOnFormPresented: 87,
  AutofillSuggestionDialogOnFormPreviewChanged: 88,
  AutofillSuggestionDialogOnFormConfirmed: 89,
  OnMicrophoneStatusChange: 90,
  // Do not reuse deleted request ID: 91,
  DeleteCapturedRegion: 92,
  OnActionSubmitted: 93,
  SubscribeToTabFavicon: 94,
  // Do not reuse deleted request ID: 95,
  SubscribeToZoomLevel: 96,
  UnsubscribeFromZoomLevel: 97,
  OnExperimentalTriggeringUpdate: 98,
  OnOptinImpression: 99,
  ProcessCounterAbuseVerdict: 100,
  GetImageBytesFromTab: 101,
  ActivateTabWithUrl: 102,
  UpdateActorTaskStepProgress: 103,
  OpenPinnedTabPicker: 104,
} as const satisfies
InterfaceHistogramIds<WebClientHost>&InterfaceHistogramIds<ActorHost>&
    InterfaceHistogramIds<AnnotationHost>;
export const MAX_REQUEST_ID = Math.max(...Object.values(RECORDED_REQUEST_IDS));

// Provides metrics histogram information for a host request type.
export interface HostRequestHistogramInfo {
  // The name of the host request type, used as histogram suffix.
  name: string;
  // The histogram enum value for this host request type.
  id: number;
}

export function getHostRequestHistogramInfo(
    requestType: string,
    interfaceDef: InterfaceDef|undefined): HostRequestHistogramInfo|undefined {
  if (!interfaceDef) {
    return undefined;
  }
  const method = interfaceDef.methodMap?.get(requestType);
  if (!method || !method.histogram) {
    return undefined;
  }
  const name = (method.histogram as {name?: string}).name ??
      (requestType.charAt(0).toUpperCase() + requestType.slice(1));
  return {
    name,
    id: method.histogram.id,
  };
}

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
export declare interface TabContextResultPrivate extends Omit<
    TabContextResult,
    'tabData'|'screenshotInfo'|'pdfDocumentData'|'annotatedPageData'> {
  tabData: TabDataPrivate;
  screenshotInfo?: ArrayBuffer;
  pdfDocumentData?: PdfDocumentDataPrivate;
  annotatedPageData?: AnnotatedPageDataPrivate;
}

// ResumeActorTaskResult data for postMessage transport.
export declare interface ResumeActorTaskResultPrivate extends Omit<
    ResumeActorTaskResult,
    'tabData'|'screenshotInfo'|'pdfDocumentData'|'annotatedPageData'> {
  tabData: TabDataPrivate;
  screenshotInfo?: ArrayBuffer;
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
  filename?: string;
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

export declare interface ImageInfoPrivate {
  caption?: string;
  sourceOrigin?: string;
  url: string;
  mimeType?: string;
}

export declare interface ImageBytesResultPrivate {
  bytes: ArrayBuffer;
  imageInfo: ImageInfoPrivate;
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
export interface GlicException extends TransferableException {
  // This may be set to indicate that the exception is a ErrorWithReason
  // exception.
  exceptionReason?: ErrorWithReasonDetails;
}

// Constructs an exception from a TransferableException.
export function exceptionFromTransferable(e: GlicException): Error|
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
export function newTransferableException(e: Error): GlicException {
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

export const ERROR_CODEC: ErrorCodec = {
  serialize: newTransferableException,
  deserialize: exceptionFromTransferable,
};
