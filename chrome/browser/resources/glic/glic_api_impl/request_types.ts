// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebClientInitialState} from '../glic.mojom-webui.js';
import type {AdditionalContext, AdditionalContextPart, AnnotatedPageData, CaptureRegionErrorReason, CaptureRegionParams, CaptureRegionResult, ChromeVersion, ClientCapabilities, ClientErrorDialogType, ConversationInfo, CounterAbuseVerdict, CreateSkillRequest, ErrorReasonTypes, ErrorWithReason, ExperimentalTriggeringUpdate, FocusedTabDataHasFocus, FocusedTabDataHasNoFocus, FormFactor, GeminiEnterpriseSettings, GetPinCandidatesOptions, HostCapability, InvokeOptions, MetricUserInputReactionType, MicrophoneStatus, OnResponseStoppedDetails, OpenPanelInfo, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, PinCandidate, PinTabsOptions, Platform, ResumeActorTaskResult, Screenshot, ScrollToParams, Skill, SkillPreview, SkillsWebClientEvent, TabContextOptions, TabContextResult, TabData, UnpinTabsOptions, UpdateSkillRequest, UserProfileInfo, WebClientMode, ZeroStateSuggestions, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../glic_api/glic_api.js';

import type {ActorClient, ActorHost} from './actor/actor_types.js';
import type {InterfaceDef, InterfaceDefMethods, ReplaceProperties} from './transport/messaging.js';
import {defInterface, defMessage} from './transport/messaging.js';
import type {ErrorCodec, PendingReceiver, PendingRemote, TransferableException} from './transport/post_message_transport.js';

export type {
  ActorClient,
  ActorHost,
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
      }>(),
      backgroundAllowed: true,
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
      backgroundAllowed: true,
      histogram: {id: 2},
    },
    {
      name: 'onExperimentalTriggeringUpdate',
      request: defMessage<{
        observationId: number,
        update?: ExperimentalTriggeringUpdate,
              observation: SubscriberObservationType,
      }>(),
      backgroundAllowed: true,
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
      backgroundAllowed: false,
      histogram: {id: 3},
    },
    {
      name: 'openGlicSettingsPage',
      request: defMessage<{options?: OpenSettingsOptions}>(),
      backgroundAllowed: true,
      histogram: {id: 4},
    },
    {
      name: 'openPasswordManagerSettingsPage',
      backgroundAllowed: true,
      histogram: {id: 78},
    },
    {
      name: 'closePanel',
      backgroundAllowed: true,
      histogram: {id: 5},
    },
    {
      name: 'closePanelAndShutdown',
      backgroundAllowed: true,
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
      backgroundAllowed: true,
      histogram: {id: 8},
    },
    {
      name: 'switchConversation',
      request: defMessage<{
        info?: ConversationInfo,
      }>(),
      response: defMessage<{}>(),
      backgroundAllowed: true,
      histogram: {id: 64},
    },
    {
      name: 'registerConversation',
      request: defMessage<{
        info: ConversationInfo,
      }>(),
      response: defMessage<{}>(),
      backgroundAllowed: true,
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
      backgroundAllowed: false,
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
      backgroundAllowed: false,
      histogram: {id: 10},
    },
    {
      name: 'setMaximumNumberOfPinnedTabs',
      request: defMessage<{
        requestedMax: number,
      }>(),
      response: defMessage<{
        effectiveMax: number,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 12},
    },
    {
      name: 'activateTab',
      request: defMessage<{
        tabId: string,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 76},
    },
    {
      name: 'captureScreenshot',
      response: defMessage<{
        screenshot: Screenshot,
      }>(),
      backgroundAllowed: false,
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
      backgroundAllowed: true,
      histogram: {id: 17},
    },
    {
      name: 'enableDragResize',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
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
      backgroundAllowed: true,
      histogram: {id: 20},
    },
    {
      name: 'setMicrophonePermissionState',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 21},
    },
    {
      name: 'setLocationPermissionState',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 22},
    },
    {
      name: 'setTabContextPermissionState',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 23},
    },
    {
      name: 'setClosedCaptioningSetting',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 56},
    },
    {
      name: 'setContextAccessIndicator',
      request: defMessage<{
        show: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 24},
    },
    {
      name: 'setActuationOnWebSetting',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 69},
    },
    {
      name: 'getUserProfileInfo',
      response: defMessage<{
        profileInfo?: UserProfileInfoPrivate,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 25},
    },
    {
      name: 'refreshSignInCookies',
      response: defMessage<{
        success: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 26},
    },
    {
      name: 'attachPanel',
      backgroundAllowed: true,
      histogram: {id: 27},
    },
    {
      name: 'detachPanel',
      backgroundAllowed: true,
      histogram: {id: 28},
    },
    {
      name: 'setAudioDucking',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 29},
    },
    {
      name: 'onUserInputSubmitted',
      request: defMessage<{
        mode: number,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 38},
    },
    {
      name: 'onReaction',
      request: defMessage<{
        reactionType: MetricUserInputReactionType,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 66},
    },
    {
      name: 'onOptinImpression',
      backgroundAllowed: true,
      histogram: {id: 99},
    },
    {
      name: 'onContextUploadStarted',
      backgroundAllowed: true,
      histogram: {id: 68},
    },
    {
      name: 'onContextUploadCompleted',
      backgroundAllowed: true,
      histogram: {id: 67},
    },
    {
      name: 'onResponseStarted',
      backgroundAllowed: true,
      histogram: {id: 40},
    },
    {
      name: 'onResponseStopped',
      request: defMessage<{details?: OnResponseStoppedDetails}>(),
      backgroundAllowed: true,
      histogram: {id: 41},
    },
    {
      name: 'onSessionTerminated',
      backgroundAllowed: true,
      histogram: {id: 42},
    },
    {
      name: 'onTurnCompleted',
      request: defMessage<{
        model: number,
        duration: number,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 43},
    },
    {
      name: 'onResponseRated',
      request: defMessage<{
        positive: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 39},
    },
    {
      name: 'onClosedCaptionsShown',
      backgroundAllowed: true,
      histogram: {id: 59},
    },
    {
      name: 'onActionSubmitted',
      request: defMessage<{
        isRetry?: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 93},
    },
    {
      name: 'scrollTo',
      request: defMessage<{
        params: ScrollToParams,
      }>(),
      backgroundAllowed: false,
      histogram: {id: 45},
    },
    {
      name: 'dropScrollToHighlight',
      backgroundAllowed: true,
      histogram: {id: 57},
    },
    {
      name: 'setSyntheticExperimentState',
      request: defMessage<{
        trialName: string,
        groupName: string,
      }>(),
      backgroundAllowed: true,
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
      backgroundAllowed: true,
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
      backgroundAllowed: false,
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
      backgroundAllowed: true,
      histogram: {id: 50},
    },
    {
      name: 'unpinAllTabs',
      request: defMessage<{
        options?: UnpinTabsOptions,
      }>(),
      backgroundAllowed: false,
      histogram: {id: 51},
    },
    {
      name: 'createSkill',
      request: defMessage<{
        request: CreateSkillRequest,
      }>(),
      response: defMessage<{
        modalOpened: boolean,
      }>(),
      histogram: {id: 82},
    },
    {
      name: 'updateSkill',
      request: defMessage<{
        request: UpdateSkillRequest,
      }>(),
      response: defMessage<{
        modalOpened: boolean,
      }>(),
      histogram: {id: 83},
    },
    {
      name: 'showManageSkillsUi',
      backgroundAllowed: true,
      histogram: {id: 86},
    },
    {
      name: 'showBrowseSkillsUi',
      backgroundAllowed: true,
      histogram: {id: 95},
    },
    {
      name: 'getSkill',
      request: defMessage<{
        id: string,
      }>(),
      response: defMessage<{
        skill?: Skill,
      }>(),
      histogram: {id: 84},
    },
    {
      name: 'recordSkillsWebClientEvent',
      request: defMessage<{
        event: SkillsWebClientEvent,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 91},
    },
    {
      name: 'subscribeToPinCandidates',
      request: defMessage<{
        options: GetPinCandidatesOptions,
        observationId: number,
      }>(),
      backgroundAllowed: false,
      histogram: {id: 52},
    },
    {
      name: 'unsubscribeFromPinCandidates',
      request: defMessage<{
        observationId: number,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 53},
    },
    {
      name: 'subscribeToCaptureRegion',
      request: defMessage<{
        observationId: number,
        params?: CaptureRegionParams,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 71},
    },
    {
      name: 'unsubscribeFromCaptureRegion',
      request: defMessage<{
        observationId: number,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 72},
    },
    {
      name: 'deleteCapturedRegion',
      request: defMessage<{
        tabId: string,
        regionId: string,
      }>(),
      backgroundAllowed: true,
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
      backgroundAllowed: false,
      histogram: {id: 54},
    },
    {
      name: 'maybeRefreshUserStatus',
      backgroundAllowed: true,
      histogram: {id: 58},
    },
    {
      name: 'getZeroStateSuggestionsAndSubscribe',
      request: defMessage<{
        hasActiveSubscription: boolean,
        options: ZeroStateSuggestionsOptions,
      }>(),
      response: defMessage<{
        suggestions?: ZeroStateSuggestionsV2,
      }>(),
      histogram: {id: 55},
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
      backgroundAllowed: true,
      histogram: {id: 63},
    },
    {
      name: 'onModeChange',
      request: defMessage<{
        newMode: WebClientMode,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 70},
    },
    {
      name: 'setOnboardingCompleted',
      backgroundAllowed: true,
      histogram: {id: 80},
    },
    {
      name: 'subscribeToTabData',
      request: defMessage<{
        tabId: string,
        observationId: number,
        cancel: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 81},
    },
    {
      name: 'subscribeToTabFavicon',
      request: defMessage<{
        tabId: string,
        observationId: number,
        cancel: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 94},
    },
    {
      name: 'onMicrophoneStatusChange',
      request: defMessage<{
        status: MicrophoneStatus,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 90},
    },
    {
      name: 'recordHistogram',
      request: defMessage<{
        name: string,
        sparseValue: number,
        // Add other histogram types as needed.
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'setErrorDialogState',
      request: defMessage<{
        shownDialogType?: ClientErrorDialogType,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'reportClientTransientError',
      request: defMessage<{
        abslStatus: number,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'processCounterAbuseVerdict',
      request: defMessage<{
        tabId: string,
        verdict: CounterAbuseVerdict,
      }>(),
      backgroundAllowed: true,
      histogram: {id: 100},
    },
    {
      name: 'subscribeToZoomLevel',
      backgroundAllowed: true,
      histogram: {id: 96},
    },
    {
      name: 'unsubscribeFromZoomLevel',
      backgroundAllowed: true,
      histogram: {id: 97},
    },
  ],
});


export type WebClientHost = typeof WebClientHostDef;

// Types of requests to the GlicWebClient.
export const WebClientDef = defInterface({
  name: 'WebClient',
  methods: [
    {
      name: 'glicWebClientNotifyPanelWillOpen',
      request: defMessage<{
        panelOpeningData: PanelOpeningData,
      }>(),
      response: defMessage<{
        openPanelInfo?: OpenPanelInfo,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyPanelWasClosed',
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientStopMicrophone',
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientPanelStateChanged',
      request: defMessage<{
        panelState: PanelState,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientCanAttachStateChanged',
      request: defMessage<{
        canAttach: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyGeminiEnterpriseSettingsChanged',
      request: defMessage<{
        settings: GeminiEnterpriseSettings | undefined,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyMicrophonePermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyLocationPermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyTabContextPermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyDefaultTabContextPermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyOsLocationPermissionStateChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyClosedCaptioningSettingChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyActuationOnWebSettingChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyFocusedTabChanged',
      request: defMessage<{
        focusedTabDataPrivate: FocusedTabDataPrivate,
      }>(),
    },
    {
      name: 'glicWebClientNotifyPanelActiveChanged',
      request: defMessage<{
        panelActive: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientCheckResponsive',
      response: defMessage<{
        clientSendMessageQueueLength: number,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyManualResizeChanged',
      request: defMessage<{
        resizing: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientBrowserIsOpenChanged',
      request: defMessage<{
        browserIsOpen: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyOsHotkeyStateChanged',
      request: defMessage<{
        hotkey: string,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyPinnedTabsChanged',
      request: defMessage<{
        tabData: TabDataPrivate[],
      }>(),
    },
    {
      name: 'glicWebClientNotifyPinnedTabDataChanged',
      request: defMessage<{
        tabData: TabDataPrivate,
      }>(),
    },
    {
      name: 'glicWebClientNotifySkillPreviewsChanged',
      request: defMessage<{
        skillPreviews: SkillPreview[],
      }>(),
    },
    {
      name: 'glicWebClientNotifySkillPreviewChanged',
      request: defMessage<{
        skillPreview: SkillPreview,
      }>(),
    },
    {
      name: 'glicWebClientNotifyContextualSkillPreviewsChanged',
      request: defMessage<{
        contextualSkillPreviews: SkillPreview[],
      }>(),
    },
    {
      name: 'glicWebClientNotifySkillDeleted',
      request: defMessage<{
        skillId: string,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientPinCandidatesChanged',
      request: defMessage<{
        candidates: PinCandidatePrivate[],
        observationId: number,
      }>(),
    },
    {
      name: 'glicWebClientZeroStateSuggestionsChanged',
      request: defMessage<{
        suggestions: ZeroStateSuggestionsV2,
        options: ZeroStateSuggestionsOptions,
      }>(),
    },
    {
      name: 'glicWebClientPageMetadataChanged',
      request: defMessage<{
        tabId: string,
        pageMetadata: PageMetadata | null,
      }>(),
    },
    {
      name: 'glicWebClientNotifyAdditionalContext',
      request: defMessage<{
        context: AdditionalContextPrivate,
      }>(),
    },
    {
      name: 'glicWebClientCaptureRegionUpdate',
      request: defMessage<{
        result?: CaptureRegionResult,
        reason?: CaptureRegionErrorReason, observationId: number,
      }>(),
    },
    {
      name: 'glicWebClientNotifyActOnWebCapabilityChanged',
      request: defMessage<{
        canActOnWeb: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientOnboardingCompletedChanged',
      request: defMessage<{
        completed: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyActorTaskListRowClicked',
      request: defMessage<{
        taskId: number,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientTabDataChanged',
      request: defMessage<{
        tabData?: TabDataPrivate, observationId: number,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientTabFaviconChanged',
      request: defMessage<{
        observationId: number,
        tabRemoved?: boolean,
        favicon?: RgbaImage,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientInvoke',
      request: defMessage<{
        options: InvokeOptionsPrivate,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientGetExperimentalTriggeringUpdates',
      request: defMessage<{
        observationId: number,
      }>(),
      response: defMessage<{
        success: boolean,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientNotifyZoomLevelChanged',
      request: defMessage<{
        zoomFactor: number,
      }>(),
      backgroundAllowed: true,
    },
  ],
});

export type WebClient = typeof WebClientDef;

export type WebClientRequestTypes =
    InterfaceDefMethods<WebClient>&InterfaceDefMethods<ActorClient>;

export type HostRequestTypes =
    InterfaceDefMethods<WebClientHost>&InterfaceDefMethods<ActorHost>;

type InterfaceHistogramIds<I extends InterfaceDef> = {
  [M in I['methods'][number] as M['histogram'] extends {id: number} ?
       (M['histogram'] extends {name: infer Name extends string} ?
            Name :
            Capitalize<M['name']&string>) :
       never]: M['histogram'] extends {id: infer Id} ? Id : never;
};

// LINT.IfChange(ApiRequestType)
// New values here must be added to histograms.xml and to enums.xml.
// Note: Not for accessing in code, so it can be stripped from compiled js.
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
} as const satisfies InterfaceHistogramIds<WebClientHost>&
    InterfaceHistogramIds<ActorHost>;
// LINT.ThenChange(
// //tools/metrics/histograms/metadata/glic/histograms.xml:ApiRequestType,
// //tools/metrics/histograms/metadata/glic/enums.xml:GlicHostApiRequestType)
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
  // interfaceDef() ensures histogram satisfies HostRequestHistogramInfo, or is
  // unset.
  return method?.histogram as HostRequestHistogramInfo | undefined;
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
