// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {ActorTaskInterruptReason, AdditionalContext, AnnotatedPageData, CancelActionsResult, CaptureRegionErrorReason, CaptureRegionParams, CaptureRegionResult, ChromeVersion, ClientCapabilities, ClientErrorDialogType, ConversationInfo, CounterAbuseVerdict, CreateActorTabOptions, CreateSkillRequest, CreateTabOptions, ExperimentalTriggeringUpdate, FocusedTabData, FormFactor, FormFillingResponse, GeminiEnterpriseSettings, GetPinCandidatesOptions, GlicBrowserHost, GlicBrowserHostJournal, GlicBrowserHostMetrics, GlicHostRegistry, GlicWebClient, InvokeOptions, MicrophoneStatus, NavigationConfirmationRequest, Observable, ObservableValue, OnResponseStoppedDetails, OpenPanelInfo, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, PinCandidate, PinTabsOptions, Platform, ResizeWindowOptions, ResumeActorTaskResult, Screenshot, ScrollToParams, SelectAutofillSuggestionsDialogRequest, SelectCredentialDialogRequest, Skill, SkillPreview, SkillsWebClientEvent, TabContextOptions, TabContextResult, TabData, TaskOptions, UnpinTabsOptions, UpdateSkillRequest, UserConfirmationDialogRequest, UserProfileInfo, WebClientMode, ZeroStateSuggestions, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../../glic_api/glic_api.js';
import {ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason, HostCapability} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl, Subject} from '../../observable.js';
import {OneShotTimer} from '../../timer.js';
import {ActorWebClientMessageHandler, GlicBrowserHostJournalImpl} from '../actor/actor_client.js';
import type {MessageHandlerInterface, ResponseExtras} from '../transport/messaging.js';
import {createBidirectionalPostMessageTransport} from '../transport/post_message_transport.js';
import type {InterfaceDef, PostMessageLifecycleObserver, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

import {ActorClientDef} from './../actor/actor_types.js';
import {replaceProperties} from './../conversions.js';
import type {ActorHost, AdditionalContextPrivate, AnnotatedPageDataPrivate, FocusedTabDataPrivate, GlicException, InvokeOptionsPrivate, PdfDocumentDataPrivate, PinCandidatePrivate, ResumeActorTaskResultPrivate, RgbaImage, TabContextResultPrivate, TabDataPrivate, WebClient, WebClientHost} from './../request_types.js';
import {ERROR_CODEC, ErrorWithReasonImpl, newTransferableException, SubscriberObservationType, WebClientDef, WebClientHostDef} from './../request_types.js';
import {rgbaImageToBlob} from './image_utils.js';

// Web client side of the Glic API.
// Communicates with the Chrome-WebUI-side in glic_api_host.ts

export class GlicHostRegistryImpl implements GlicHostRegistry {
  private host: GlicBrowserHostImpl|undefined;
  constructor(private windowProxy: WindowProxy) {}

  async registerWebClient(webClient: GlicWebClient): Promise<void> {
    this.host = new GlicBrowserHostImpl(webClient, this.windowProxy);
    const clientCapabilities = webClient.getClientCapabilities?.() ?? new Set();
    await this.host.webClientCreated(clientCapabilities);
    let success = false;
    let exception: GlicException|undefined;
    try {
      await webClient.initialize(this.host);
      success = true;
    } catch (e) {
      console.warn(e);
      if (e instanceof Error) {
        exception = newTransferableException(e);
      }
    }
    if (this.host) {
      this.host.webClientInitialized(success, exception);
    }
  }
}

// A type which the guest should implement.
// This helps verify that WebClientMessageHandler is implemented with the
// correct parameter and return types.
class WebClientMessageHandler implements MessageHandlerInterface<WebClient> {
  private cachedPinnedTabs: TabData[]|undefined = undefined;
  private cachedSkillPreviews: SkillPreview[] = [];
  private cachedContextualSkillPreviews: SkillPreview[] = [];
  private cachedSkillPrompts = new Map<string, string>();

  constructor(
      private webClient: GlicWebClient, private host: GlicBrowserHostImpl) {}

  async glicWebClientNotifyPanelWillOpen(payload: {
    panelOpeningData: PanelOpeningData,
  }): Promise<{openPanelInfo?: OpenPanelInfo}> {
    let openPanelInfo: OpenPanelInfo|undefined;
    try {
      const mergedArgument: PanelOpeningData&PanelState = Object.assign(
          {}, payload.panelOpeningData, payload.panelOpeningData.panelState);
      const completedPromise = this.host.notifyPanelWillOpenCompleted;
      const result = await this.webClient.notifyPanelWillOpen?.(mergedArgument);
      completedPromise.resolve();

      if (result) {
        openPanelInfo = result;
      }
    } catch (e) {
      console.warn(e);
    }
    return {openPanelInfo};
  }

  async glicWebClientNotifyPanelWasClosed(): Promise<void> {
    try {
      this.host.notifyPanelWillOpenCompleted = Promise.withResolvers<void>();
      await this.webClient.notifyPanelWasClosed?.();
    } catch (e) {
      console.warn(e);
    }
  }

  glicWebClientPanelStateChanged(payload: {panelState: PanelState}): void {
    this.host.getPanelState?.().assignAndSignal(payload.panelState);
  }

  glicWebClientZeroStateSuggestionsChanged(payload: {
    suggestions: ZeroStateSuggestionsV2,
    options: ZeroStateSuggestionsOptions,
  }): void {
    this.host.currentZeroStateObserver?.assignAndSignal(payload.suggestions);
  }

  glicWebClientCanAttachStateChanged(payload: {canAttach: boolean}): void {
    this.host.canAttachPanelValue.assignAndSignal(payload.canAttach);
  }

  glicWebClientNotifyGeminiEnterpriseSettingsChanged(payload: {
    settings: GeminiEnterpriseSettings|undefined,
  }) {
    this.host.getGeminiEnterpriseSettings?.().assignAndSignal(payload.settings);
  }

  glicWebClientNotifyMicrophonePermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getMicrophonePermissionState().assignAndSignal(payload.enabled);
  }

  async glicWebClientStopMicrophone(): Promise<void> {
    await this.webClient.stopMicrophone?.();
  }

  glicWebClientNotifyLocationPermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getLocationPermissionState().assignAndSignal(payload.enabled);
  }

  glicWebClientNotifyTabContextPermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getTabContextPermissionState().assignAndSignal(payload.enabled);
  }

  glicWebClientNotifyDefaultTabContextPermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.defaultTabContextPermission.assignAndSignal(payload.enabled);
  }

  glicWebClientNotifyOsLocationPermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getOsLocationPermissionState().assignAndSignal(payload.enabled);
  }

  glicWebClientNotifyClosedCaptioningSettingChanged(payload: {
    enabled: boolean,
  }) {
    this.host.closedCaptioningState.assignAndSignal(payload.enabled);
  }

  async glicWebClientInvoke(payload: {options: InvokeOptionsPrivate}):
      Promise<void> {
    try {
      const options = convertInvokeOptionsFromPrivate(payload.options);
      // Wait until notifyPanelWillOpen has resolved before invoking.
      await this.host.notifyPanelWillOpenCompleted.promise;
      await this.webClient.invoke?.(options);
    } catch (e) {
      console.warn(e);
    }
  }

  async glicWebClientGetExperimentalTriggeringUpdates(
      payload: {observationId: number},
      _extras: ResponseExtras): Promise<{success: boolean}> {
    const getUpdates = this.webClient.getExperimentalTriggeringUpdates;
    if (!getUpdates) {
      return {success: false};
    }
    const observable = await getUpdates.call(this.webClient);
    if (!observable) {
      return {success: false};
    }
    const subscriber = observable.subscribeObserver({
      next: (update: ExperimentalTriggeringUpdate) => {
        this.host.clientRemote.requestNoResponse(
            'onExperimentalTriggeringUpdate', {
              observationId: payload.observationId,
              update,
              observation: SubscriberObservationType.UPDATE,
            });
      },
      complete: () => {
        this.host.clientRemote.requestNoResponse(
            'onExperimentalTriggeringUpdate', {
              observationId: payload.observationId,
              observation: SubscriberObservationType.COMPLETE,
            });
        if (subscriber) {
          subscriber.unsubscribe();
        }
      },
      error: (_err: unknown) => {
        this.host.clientRemote.requestNoResponse(
            'onExperimentalTriggeringUpdate', {
              observationId: payload.observationId,
              observation: SubscriberObservationType.ERROR,
            });
        if (subscriber) {
          subscriber.unsubscribe();
        }
      },
    });
    return {success: true};
  }

  glicWebClientNotifyActuationOnWebSettingChanged(payload: {
    enabled: boolean,
  }) {
    this.host.actuationOnWebState.assignAndSignal(payload.enabled);
  }

  glicWebClientNotifyFocusedTabChanged(payload: {
    focusedTabDataPrivate: FocusedTabDataPrivate,
  }) {
    const focusedTabData =
        convertFocusedTabDataFromPrivate(payload.focusedTabDataPrivate);
    this.host.getFocusedTabStateV2().assignAndSignal(focusedTabData);
  }

  glicWebClientNotifyZoomLevelChanged(payload: {zoomFactor: number}) {
    this.host.getZoomLevel().assignAndSignal(payload.zoomFactor);
  }

  glicWebClientNotifyPanelActiveChanged(payload: {panelActive: boolean}): void {
    this.host.panelActiveValue.assignAndSignal(payload.panelActive);
  }

  async glicWebClientCheckResponsive():
      Promise<{clientSendMessageQueueLength: number}> {
    await this.webClient.checkResponsive?.();
    return {
      clientSendMessageQueueLength:
          this.host.clientRemote.rawSender().messageQueueLength() +
          this.host.clientRemote.rawSender().inFlightRequestCount(),
    };
  }

  glicWebClientNotifyManualResizeChanged(payload: {resizing: boolean}) {
    this.host.isManuallyResizing().assignAndSignal(payload.resizing);
  }

  glicWebClientBrowserIsOpenChanged(payload: {browserIsOpen: boolean}) {
    this.host.isBrowserOpenValue.assignAndSignal(payload.browserIsOpen);
  }

  glicWebClientNotifyOsHotkeyStateChanged(payload: {hotkey: string}) {
    this.host.getOsHotkeyState().assignAndSignal(payload);
  }

  glicWebClientPinCandidatesChanged(payload: {
    candidates: PinCandidatePrivate[],
    observationId: number,
  }): void {
    this.host.pinCandidates?.processUpdate(
        payload.candidates, payload.observationId);
  }

  glicWebClientNotifyPinnedTabsChanged(payload: {tabData: TabDataPrivate[]}):
      void {
    this.cachedPinnedTabs =
        payload.tabData.map((x) => convertTabDataFromPrivate(x));
    this.host.pinnedTabs?.assignAndSignal(this.cachedPinnedTabs);
  }

  glicWebClientNotifyPinnedTabDataChanged(payload: {tabData: TabDataPrivate}):
      void {
    if (!this.cachedPinnedTabs) {
      return;
    }
    const tabData = convertTabDataFromPrivate(payload.tabData);
    this.cachedPinnedTabs = this.cachedPinnedTabs.map((cachedTab) => {
      if (cachedTab.tabId === tabData.tabId) {
        return tabData;
      }
      return cachedTab;
    });
    this.host.pinnedTabs.assignAndSignal(this.cachedPinnedTabs);
  }

  glicWebClientNotifySkillPreviewsChanged(payload: {
    skillPreviews: SkillPreview[],
  }): void {
    this.cachedSkillPrompts.clear();
    this.cachedSkillPreviews = payload.skillPreviews;
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  glicWebClientNotifyContextualSkillPreviewsChanged(payload: {
    contextualSkillPreviews: SkillPreview[],
  }): void {
    this.cachedContextualSkillPreviews = payload.contextualSkillPreviews;
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  glicWebClientNotifySkillPreviewChanged(payload: {skillPreview: SkillPreview}):
      void {
    const skillPreview = payload.skillPreview;
    this.cachedSkillPrompts.delete(skillPreview.id);

    const index = this.cachedSkillPreviews.findIndex(
        (cachedSkillPreview) => cachedSkillPreview.id === skillPreview.id);

    if (index !== -1) {
      // SkillPreview with the same ID exists, replace it.
      this.cachedSkillPreviews = [
        ...this.cachedSkillPreviews.slice(0, index),
        skillPreview,
        ...this.cachedSkillPreviews.slice(index + 1),
      ];
    } else {
      // SkillPreview with this ID not found, add it to the cache.
      this.cachedSkillPreviews = [...this.cachedSkillPreviews, skillPreview];
    }

    // Signal the change to the host.
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  glicWebClientNotifySkillDeleted(payload: {
    skillId: string,
  }): void {
    const skillId = payload.skillId;
    this.cachedSkillPrompts.delete(skillId);
    const index = this.cachedSkillPreviews.findIndex(
        (cachedSkillPreview) => cachedSkillPreview.id === skillId);
    if (index !== -1) {
      // SkillPreview with the same ID exists, remove it.
      this.cachedSkillPreviews = [
        ...this.cachedSkillPreviews.slice(0, index),
        ...this.cachedSkillPreviews.slice(index + 1),
      ];
    }
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  glicWebClientPageMetadataChanged(
      payload: {tabId: string, pageMetadata: PageMetadata|null}): void {
    const observable = this.host.pageMetadataObservers.get(payload.tabId);
    if (!observable) {
      return;
    }

    if (payload.pageMetadata) {
      observable.assignAndSignal(payload.pageMetadata);
    } else {
      if (!observable.isStopped()) {
        observable.complete();
      }
      this.host.pageMetadataObservers.delete(payload.tabId);
    }
  }

  glicWebClientNotifyAdditionalContext(payload: {
    context: AdditionalContextPrivate,
  }): void {
    const context = convertAdditionalContextFromPrivate(payload.context);
    this.host.additionalContextSubject.next(context);
  }

  glicWebClientCaptureRegionUpdate(payload: {
    result?: CaptureRegionResult,
    reason?: CaptureRegionErrorReason, observationId: number,
  }): void {
    const observable = this.host.captureRegionObservable;
    if (observable?.observationId !== payload.observationId) {
      return;
    }

    if (payload.result) {
      observable.processUpdate(payload.result);
    } else if (payload.reason !== undefined) {
      observable.processError(payload.reason);
    }
  }

  glicWebClientNotifyActOnWebCapabilityChanged(payload: {
    canActOnWeb: boolean,
  }): void {
    this.host.actOnWebCapabilityValue.assignAndSignal(payload.canActOnWeb);
  }

  glicWebClientOnboardingCompletedChanged(payload: {completed: boolean}): void {
    this.host.onboardingCompleted.assignAndSignal(payload.completed);
  }

  glicWebClientNotifyActorTaskListRowClicked(payload: {taskId: number}): void {
    this.host.actorTaskListRowClickedSubject.next(payload.taskId);
  }

  glicWebClientTabDataChanged(payload: {
    tabData?: TabDataPrivate, observationId: number,
  }): void {
    if (payload.tabData === undefined) {
      this.host.getTabByIdObservableSet.completeObservable(
          payload.observationId);
    } else {
      this.host.getTabByIdObservableSet.assignAndSignal(
          payload.observationId, convertTabDataFromPrivate(payload.tabData));
    }
  }

  glicWebClientTabFaviconChanged(payload: {
    favicon?: RgbaImage, observationId: number,
    tabRemoved?: boolean,
  }): void {
    if (payload.tabRemoved) {
      this.host.getTabFaviconByIdObservableSet.completeObservable(
          payload.observationId);
      return;
    }
    if (payload.favicon === undefined) {
      this.host.getTabFaviconByIdObservableSet.assignAndSignal(
          payload.observationId, undefined);
    } else {
      this.host.getTabFaviconByIdObservableSet.assignAndSignal(
          payload.observationId, rgbaImageToBlob(payload.favicon));
    }
  }

  cacheSkillPrompt(skill: Skill) {
    const preview = skill.preview;
    if (preview.id && skill.prompt) {
      this.cachedSkillPrompts.set(preview.id, skill.prompt);
    }
  }

  combineSkillPreviews() {
    return [...this.cachedContextualSkillPreviews, ...this.cachedSkillPreviews];
  }
}

export class GlicBrowserHostImpl implements GlicBrowserHost,
                                            PostMessageLifecycleObserver {
  readonly router: PostMessageRouter;
  readonly clientRemote: PostMessageRemote<WebClientHost>;
  private actorSender?: PostMessageRemote<ActorHost>;
  private webClientMessageHandler: WebClientMessageHandler;
  private actorWebClientMessageHandler: ActorWebClientMessageHandler;
  private chromeVersion?: ChromeVersion;
  private platform?: Platform;
  private formFactor?: FormFactor;
  private panelState = ObservableValueImpl.withNoValue<PanelState>();
  canAttachPanelValue = ObservableValueImpl.withNoValue<boolean>();
  private focusedTabStateV2 = ObservableValueImpl.withNoValue<FocusedTabData>();
  private geminiEnterpriseSettings =
      ObservableValueImpl.withNoValue<GeminiEnterpriseSettings|undefined>();
  private zoomLevel =
      ObservableValueImpl.withNoValue<number>(async (isActive: boolean) => {
        if (isActive) {
          await this.clientRemote.requestWithResponse(
              'subscribeToZoomLevel', undefined);
        } else {
          this.clientRemote.requestNoResponse(
              'unsubscribeFromZoomLevel', undefined);
        }
      });
  private permissionStateMicrophone =
      ObservableValueImpl.withNoValue<boolean>();
  private permissionStateLocation = ObservableValueImpl.withNoValue<boolean>();
  private permissionStateTabContext =
      ObservableValueImpl.withNoValue<boolean>();
  defaultTabContextPermission = ObservableValueImpl.withNoValue<boolean>();
  private enableDefaultTabContextSettingFeature = false;
  private permissionStateOsLocation =
      ObservableValueImpl.withNoValue<boolean>();
  closedCaptioningState = ObservableValueImpl.withNoValue<boolean>();
  actuationOnWebState = ObservableValueImpl.withNoValue<boolean>();
  private osHotkeyState = ObservableValueImpl.withNoValue<{hotkey: string}>();
  onboardingCompleted = ObservableValueImpl.withNoValue<boolean>();
  panelActiveValue = ObservableValueImpl.withNoValue<boolean>();
  isBrowserOpenValue = ObservableValueImpl.withNoValue<boolean>();
  private journalHost?: GlicBrowserHostJournalImpl;
  private metrics: GlicBrowserHostMetricsImpl;
  private manuallyResizing = ObservableValueImpl.withValue<boolean>(false);
  private cachedUserProfile?: Promise<UserProfileInfo>;
  private enableCachedGetUserProfileInfo?: boolean;
  pinnedTabs = ObservableValueImpl.withNoValue<TabData[]>();
  skillPreviews = ObservableValueImpl.withNoValue<SkillPreview[]>();
  skillToInvoke = ObservableValueImpl.withNoValue<Skill>();
  pinCandidates: PinCandidatesObservable|undefined;
  captureRegionObservable?: CaptureRegionObservable;
  // Makes IDs that are unique within the scope of this class.
  idGenerator = new IdGenerator();
  private currentZeroStateSuggestionOptions: ZeroStateSuggestionsOptions = {
    isFirstRun: false,
    supportedTools: [],
  };
  currentZeroStateObserver =
      ObservableValueImpl.withNoValue<ZeroStateSuggestionsV2>();
  private hostCapabilities: Set<HostCapability> = new Set();
  private actorTaskState =
      new Map<number, ObservableValueImpl<ActorTaskState>>();
  readonly additionalContextSubject = new Subject<AdditionalContext>();
  pageMetadataObservers: Map<string, ObservableValueImpl<PageMetadata>> =
      new Map();
  readonly selectCredentialDialogRequestSubject =
      new Subject<SelectCredentialDialogRequest>();
  readonly userConfirmationDialogRequestSubject =
      new Subject<UserConfirmationDialogRequest>();

  readonly navigationConfirmationRequestSubject =
      new Subject<NavigationConfirmationRequest>();
  readonly actorTaskListRowClickedSubject = new Subject<number>();
  actOnWebCapabilityValue = ObservableValueImpl.withNoValue<boolean>();

  readonly selectAutofillSuggestionsDialogRequestSubject =
      new Subject<SelectAutofillSuggestionsDialogRequest>();
  getTabByIdObservableSet: ObservableSetByTabId<TabData>;
  getTabFaviconByIdObservableSet: ObservableSetByTabId<Blob|undefined>;
  notifyPanelWillOpenCompleted = Promise.withResolvers<void>();

  constructor(public webClient: GlicWebClient, windowProxy: WindowProxy) {
    // TODO(harringtond): Ideally, we could ensure we only process requests from
    // the single senderId used by the web client. This would avoid accidental
    // processing of requests from a previous client. This risk is very minimal,
    // as it would require reloading the webview page and initializing a new
    // web client very quickly, and in normal operation, the webview does not
    // reload after successful load.
    this.webClientMessageHandler =
        new WebClientMessageHandler(this.webClient, this);
    this.actorWebClientMessageHandler = new ActorWebClientMessageHandler(this);
    const {router, rootRemote} = createBidirectionalPostMessageTransport(
        'chrome://glic',
        windowProxy,
        this,
        this.webClientMessageHandler,
        'glic_api_client',
        /*isHost=*/ false,
        ERROR_CODEC,
        WebClientDef,
        WebClientHostDef,
    );
    this.router = router;
    this.clientRemote = rootRemote;
    this.getTabByIdObservableSet = new ObservableSetByTabId<TabData>(
        new GetTabByIdObservableSetImpl(), this.clientRemote, this.idGenerator);
    this.getTabFaviconByIdObservableSet =
        new ObservableSetByTabId<Blob|undefined>(
            new GetTabFaviconByIdObservableSetImpl(), this.clientRemote,
            this.idGenerator);
    this.metrics = new GlicBrowserHostMetricsImpl(this.clientRemote);
  }

  destroy() {
    this.router.destroy();
  }

  async webClientCreated(clientCapabilities: Set<ClientCapabilities>) {
    const response = await this.clientRemote.requestWithResponse(
        'webClientCreated',
        {clientCapabilities: Array.from(clientCapabilities)});
    if (response.actorRemote !== undefined &&
        response.actorReceiver !== undefined) {
      this.actorSender = this.router.newRemote(response.actorRemote);
      this.router.newReceiver(
          response.actorReceiver, this.actorWebClientMessageHandler,
          ActorClientDef);
      this.journalHost = new GlicBrowserHostJournalImpl(this.actorSender);
    }
    const state = response.initialState;
    this.geminiEnterpriseSettings.assignAndSignal(
        state.geminiEnterpriseSettings ?? undefined);
    this.router.setLoggingEnabled(state.loggingEnabled);
    this.clientRemote.rawSender().setMaxInFlightRequests(
        state.maxInFlightRequests);
    this.clientRemote.rawSender().sendResponsesForAllRequests =
        state.sendResponsesForAllRequests;
    this.panelState.assignAndSignal(state.panelState);
    const focusedTabData =
        convertFocusedTabDataFromPrivate(state.focusedTabData);
    this.focusedTabStateV2.assignAndSignal(focusedTabData);
    this.permissionStateMicrophone.assignAndSignal(
        state.microphonePermissionEnabled);
    this.permissionStateLocation.assignAndSignal(
        state.locationPermissionEnabled);
    if (state.enableDefaultTabContextSettingFeature) {
      this.permissionStateTabContext.assignAndSignal(
          state.defaultTabContextSettingEnabled);
    } else {
      this.permissionStateTabContext.assignAndSignal(
          state.tabContextPermissionEnabled);
    }
    this.defaultTabContextPermission.assignAndSignal(
        state.defaultTabContextSettingEnabled);
    this.enableDefaultTabContextSettingFeature =
        state.enableDefaultTabContextSettingFeature;
    this.permissionStateOsLocation.assignAndSignal(
        state.osLocationPermissionEnabled);
    this.canAttachPanelValue.assignAndSignal(state.canAttach);
    this.chromeVersion = state.chromeVersion;
    this.platform = state.platform;
    this.formFactor = state.formFactor;
    this.enableCachedGetUserProfileInfo = state.enableCachedGetUserProfileInfo;
    this.panelActiveValue.assignAndSignal(state.panelIsActive);
    this.isBrowserOpenValue.assignAndSignal(state.browserIsOpen);
    this.osHotkeyState.assignAndSignal({hotkey: state.hotkey});
    this.closedCaptioningState.assignAndSignal(
        state.closedCaptioningSettingEnabled);
    this.actuationOnWebState.assignAndSignal(
        state.actuationOnWebSettingEnabled);
    for (const capability of state.hostCapabilities) {
      this.hostCapabilities.add(capability);
    }
    this.actOnWebCapabilityValue.assignAndSignal(state.canActOnWeb);
    this.onboardingCompleted.assignAndSignal(state.onboardingCompleted);

    // Set the method to undefined since it's gated behind a mojo
    // RuntimeFeature. Calling a such a method when the feature is disabled
    // results in a mojo pipe closure.
    if (!this.hostCapabilities.has(
            HostCapability.GET_MODEL_QUALITY_CLIENT_ID)) {
      // MOJO_RUNTIME_FEATURE_GATED GetModelQualityClientId
      this.getModelQualityClientId = undefined;
    }

    if (!state.enableSkills) {
      this.createSkill = undefined;
      this.updateSkill = undefined;
      this.showManageSkillsUi = undefined;
      this.showBrowseSkillsUi = undefined;
      this.getSkill = undefined;
    }

    if (!state.enableScrollTo) {
      this.scrollTo = undefined;
      this.dropScrollToHighlight = undefined;
    }

    if (!state.enableActInFocusedTab) {
      this.createTask = undefined;
      this.performActions = undefined;
      this.cancelActions = undefined;
      this.stopActorTask = undefined;
      this.pauseActorTask = undefined;
      this.resumeActorTask = undefined;
      this.interruptActorTask = undefined;
      this.uninterruptActorTask = undefined;
      this.getActOnWebCapability = undefined;
      this.createActorTab = undefined;
      this.actorTaskListRowClicked = undefined;
      this.getJournalHost = undefined;
    }

    if (!state.enableZeroStateSuggestions) {
      this.getZeroStateSuggestionsForFocusedTab = undefined;
      // MOJO_RUNTIME_FEATURE_GATED GetZeroStateSuggestionsAndSubscribe
      this.getZeroStateSuggestions = undefined;
    }

    if (!state.enableDefaultTabContextSettingFeature) {
      this.getDefaultTabContextPermissionState = undefined;
    }

    if (!state.enableMaybeRefreshUserStatus) {
      this.maybeRefreshUserStatus = undefined;
    }

    if (!state.enableGetContextActor) {
      // MOJO_RUNTIME_FEATURE_GATED GetContextForActorFromTab
      this.getContextForActorFromTab = undefined;
    }

    if (!state.enableGetPageMetadata) {
      this.getPageMetadata = undefined;
    }

    if (!state.enableWebActuationSettingFeature) {
      this.getActuationOnWebSetting = undefined;
      this.setActuationOnWebSetting = undefined;
    }

    if (!state.enableCaptureRegion) {
      this.captureRegion = undefined;
      this.deleteCapturedRegion = undefined;
    }

    if (!state.enableActivateTab) {
      // MOJO_RUNTIME_FEATURE_GATED ActivateTab
      this.activateTab = undefined;
    }

    if (!state.enableGetTabById) {
      this.getTabById = undefined;
    }

    if (!state.enableOpenPasswordManagerSettingsPage) {
      this.openPasswordManagerSettingsPage = undefined;
    }

    if (!state.enableGetTabFaviconById) {
      this.getTabFaviconById = undefined;
    }

    if (!state.enableProcessCounterAbuseVerdict) {
      this.processCounterAbuseVerdict = undefined;
    }
  }

  webClientInitialized(success: boolean, exception: GlicException|undefined) {
    this.clientRemote.requestNoResponse(
        'webClientInitialized', {success, exception});
  }

  onRequestReceived(_type: string, _interfaceDef: InterfaceDef|undefined):
      void {}
  onRequestHandlerException(
      _type: string, _interfaceDef: InterfaceDef|undefined): void {}
  onRequestCompleted(_type: string, _interfaceDef: InterfaceDef|undefined):
      void {}

  setActorTaskState(taskId: number, state: ActorTaskState): void {
    this.getActorTaskState(taskId).assignAndSignal(state);

    if (state === ActorTaskState.STOPPED) {
      this.actorTaskState.delete(taskId);
    }
  }

  // GlicBrowserHost implementation.

  getChromeVersion() {
    return Promise.resolve(this.chromeVersion!);
  }

  getPlatform(): Platform {
    return this.platform!;
  }

  getFormFactor(): FormFactor {
    return this.formFactor!;
  }

  async createTab(url: string, options: CreateTabOptions): Promise<TabData> {
    const result = await this.clientRemote.requestWithResponse('createTab', {
      url,
      options,
    });
    if (!result.tabData) {
      throw new Error('createTab: failed');
    }
    return convertTabDataFromPrivate(result.tabData);
  }

  openGlicSettingsPage(options?: OpenSettingsOptions): void {
    this.clientRemote.requestNoResponse('openGlicSettingsPage', {options});
  }

  autofillSuggestionDialogOnFormPresented(taskId: number, params: {
    formFillingRequestIndex: number,
  }): void {
    this.actorSender?.requestNoResponse(
        'autofillSuggestionDialogOnFormPresented', {taskId, params});
  }

  autofillSuggestionDialogOnFormPreviewChanged(taskId: number, params: {
    formFillingRequestIndex: number,
    response?: FormFillingResponse,
  }): void {
    this.actorSender?.requestNoResponse(
        'autofillSuggestionDialogOnFormPreviewChanged', {taskId, params});
  }

  autofillSuggestionDialogOnFormConfirmed(taskId: number, params: {
    formFillingRequestIndex: number,
    response: FormFillingResponse,
  }): void {
    this.actorSender?.requestNoResponse(
        'autofillSuggestionDialogOnFormConfirmed', {taskId, params});
  }

  openPasswordManagerSettingsPage?(): void {
    this.clientRemote.requestNoResponse(
        'openPasswordManagerSettingsPage', undefined);
  }

  reportClientTransientError(abslStatus: number): void {
    this.clientRemote.requestNoResponse(
        'reportClientTransientError', {abslStatus});
  }

  processCounterAbuseVerdict?(tabId: string, verdict: CounterAbuseVerdict): void {
    this.clientRemote.requestNoResponse(
        'processCounterAbuseVerdict', {tabId, verdict});
  }

  closePanel(): Promise<void> {
    return this.clientRemote.requestWithResponse('closePanel', undefined);
  }

  closePanelAndShutdown(): void {
    this.clientRemote.requestNoResponse('closePanelAndShutdown', undefined);
  }

  attachPanel?(): void {
    this.clientRemote.requestNoResponse('attachPanel', undefined);
  }

  detachPanel?(): void {
    if (this.hostCapabilities.has(HostCapability.NO_LIVE_MODE)) {
      throw new Error('NO_LIVE_MODE: detachPanel not supported');
    }
    this.clientRemote.requestNoResponse('detachPanel', undefined);
  }

  showProfilePicker(): void {
    this.clientRemote.requestNoResponse('showProfilePicker', undefined);
  }

  async getModelQualityClientId?(): Promise<string> {
    const result = await this.clientRemote.requestWithResponse(
        'getModelQualityClientId', undefined);
    return result.modelQualityClientId;
  }

  getGeminiEnterpriseSettings?
      (): ObservableValueImpl<GeminiEnterpriseSettings|undefined> {
    return this.geminiEnterpriseSettings;
  }

  async switchConversation(info?: ConversationInfo): Promise<void> {
    await this.clientRemote.requestWithResponse('switchConversation', {info});
  }

  async registerConversation(info: ConversationInfo): Promise<void> {
    await this.clientRemote.requestWithResponse('registerConversation', {info});
  }

  async getContextFromFocusedTab(options: TabContextOptions):
      Promise<TabContextResult> {
    const context = await this.clientRemote.requestWithResponse(
        'getContextFromFocusedTab', {options});
    return convertTabContextResultFromPrivate(context.tabContextResult);
  }

  async setMaximumNumberOfPinnedTabs?(requestedMax: number): Promise<number> {
    const result = await this.clientRemote.requestWithResponse(
        'setMaximumNumberOfPinnedTabs', {requestedMax});
    return result.effectiveMax;
  }

  async getContextFromTab?
      (tabId: string, options: TabContextOptions): Promise<TabContextResult> {
    const result = await this.clientRemote.requestWithResponse(
        'getContextFromTab', {tabId, options});
    return convertTabContextResultFromPrivate(result.tabContextResult);
  }

  async getContextForActorFromTab?
      (tabId: string, options: TabContextOptions): Promise<TabContextResult> {
    assert(this.actorSender);
    const result = await this.actorSender.requestWithResponse(
        'getContextForActorFromTab', {tabId, options});
    return convertTabContextResultFromPrivate(result.tabContextResult);
  }

  async createTask?(taskOptions?: TaskOptions): Promise<number> {
    assert(this.actorSender);
    const result =
        await this.actorSender.requestWithResponse('createTask', {taskOptions});
    return result.taskId;
  }

  async performActions?(actions: ArrayBuffer): Promise<ArrayBuffer> {
    assert(this.actorSender);
    const result =
        await this.actorSender.requestWithResponse('performActions', {actions});
    return result.actionsResult;
  }

  async cancelActions?(taskId: number): Promise<CancelActionsResult> {
    assert(this.actorSender);
    const response =
        await this.actorSender.requestWithResponse('cancelActions', {taskId});
    return response.result;
  }

  stopActorTask?(taskId?: number, stopReason?: ActorTaskStopReason): void {
    this.actorSender?.requestNoResponse('stopActorTask', {
      taskId: taskId ?? 0,
      stopReason: stopReason ?? ActorTaskStopReason.TASK_COMPLETE,
    });
  }

  pauseActorTask?
      (taskId: number, pauseReason?: ActorTaskPauseReason, tabId?: string):
          void {
    this.actorSender?.requestNoResponse('pauseActorTask', {
      taskId,
      pauseReason: pauseReason ?? ActorTaskPauseReason.PAUSED_BY_MODEL,
      tabId: tabId ?? '',
    });
  }

  async resumeActorTask?(taskId: number, tabContextOptions: TabContextOptions):
      Promise<ResumeActorTaskResult> {
    assert(this.actorSender);
    const response = await this.actorSender.requestWithResponse(
        'resumeActorTask', {taskId, tabContextOptions});
    return convertTabContextResultFromPrivate(response.resumeActorTaskResult);
  }

  interruptActorTask?
      (taskId: number, interruptReason?: ActorTaskInterruptReason): void {
    this.actorSender?.requestNoResponse('interruptActorTask', {
      taskId,
      interruptReason,
    });
  }

  uninterruptActorTask?(taskId: number): void {
    this.actorSender?.requestNoResponse('uninterruptActorTask', {
      taskId,
    });
  }

  getActorTaskState(taskId: number): ObservableValueImpl<ActorTaskState> {
    const stateObs = this.actorTaskState.get(taskId);
    if (stateObs) {
      return stateObs;
    }
    // TODO(mcnee): The client could pass an id that will never have
    // state updates (e.g. the task already finished and we cleared the old
    // observable in setActorTaskState). Consider removing these cases from the
    // map when all subscribers are removed.
    const newObs = ObservableValueImpl.withNoValue<ActorTaskState>();
    this.actorTaskState.set(taskId, newObs);
    return newObs;
  }

  async createActorTab?
      (taskId: number, options: CreateActorTabOptions): Promise<TabData> {
    assert(this.actorSender);
    const result = await this.actorSender.requestWithResponse(
        'createActorTab', {taskId, options});
    if (!result.tabData) {
      throw new Error('createActorTab: failed');
    }
    return convertTabDataFromPrivate(result.tabData);
  }

  getTabById?(tabId: string): ObservableValueImpl<TabData> {
    return this.getTabByIdObservableSet.getObservableByTabId(tabId);
  }

  getTabFaviconById?(tabId: string): ObservableValueImpl<Blob|undefined> {
    return this.getTabFaviconByIdObservableSet.getObservableByTabId(tabId);
  }

  activateTab?(tabId: string): void {
    this.clientRemote.requestNoResponse('activateTab', {tabId});
  }

  onModeChange?(newMode: WebClientMode): void {
    this.clientRemote.requestNoResponse('onModeChange', {newMode});
  }

  onMicrophoneStatusChange?(status: MicrophoneStatus): void {
    this.clientRemote.requestNoResponse('onMicrophoneStatusChange', {status});
  }

  setErrorDialogState?(shownDialogType?: ClientErrorDialogType): void {
    this.clientRemote.requestNoResponse(
        'setErrorDialogState', {shownDialogType});
  }

  async resizeWindow(
      width: number, height: number,
      options?: ResizeWindowOptions): Promise<void> {
    return this.clientRemote.requestWithResponse(
        'resizeWindow', {size: {width, height}, options});
  }

  enableDragResize?(enabled: boolean): Promise<void> {
    return this.clientRemote.requestWithResponse('enableDragResize', {enabled});
  }

  async captureScreenshot(): Promise<Screenshot> {
    const screenshotResult = await this.clientRemote.requestWithResponse(
        'captureScreenshot', undefined);
    return screenshotResult.screenshot;
  }

  captureRegion?
      (params?: CaptureRegionParams): ObservableValue<CaptureRegionResult> {
    if (this.captureRegionObservable) {
      this.captureRegionObservable.complete();
    }
    this.captureRegionObservable = new CaptureRegionObservable(
        this.idGenerator.next(), this.clientRemote, params);
    return this.captureRegionObservable;
  }

  deleteCapturedRegion?(tabId: string, regionId: string): void {
    this.clientRemote.requestNoResponse(
        'deleteCapturedRegion', {tabId, regionId});
  }

  setMinimumWidgetSize(width: number, height: number): Promise<void> {
    return this.clientRemote.requestWithResponse(
        'setMinimumWidgetSize', {size: {width, height}});
  }

  getPanelState?(): ObservableValueImpl<PanelState> {
    return this.panelState;
  }

  panelActive(): ObservableValueImpl<boolean> {
    return this.panelActiveValue;
  }

  canAttachPanel?(): ObservableValue<boolean> {
    return this.canAttachPanelValue;
  }

  isBrowserOpen(): ObservableValue<boolean> {
    return this.isBrowserOpenValue;
  }

  getFocusedTabStateV2(): ObservableValueImpl<FocusedTabData> {
    return this.focusedTabStateV2;
  }

  getZoomLevel(): ObservableValueImpl<number> {
    return this.zoomLevel;
  }

  getMicrophonePermissionState(): ObservableValueImpl<boolean> {
    return this.permissionStateMicrophone;
  }

  getLocationPermissionState(): ObservableValueImpl<boolean> {
    return this.permissionStateLocation;
  }

  getTabContextPermissionState(): ObservableValueImpl<boolean> {
    return this.permissionStateTabContext;
  }

  getDefaultTabContextPermissionState?(): ObservableValueImpl<boolean> {
    return this.defaultTabContextPermission;
  }

  getOsLocationPermissionState(): ObservableValueImpl<boolean> {
    return this.permissionStateOsLocation;
  }

  getClosedCaptioningSetting?(): ObservableValueImpl<boolean> {
    return this.closedCaptioningState;
  }

  getActuationOnWebSetting?(): ObservableValueImpl<boolean> {
    return this.actuationOnWebState;
  }

  setMicrophonePermissionState(enabled: boolean): Promise<void> {
    return this.clientRemote.requestWithResponse(
        'setMicrophonePermissionState', {enabled});
  }

  setLocationPermissionState(enabled: boolean): Promise<void> {
    return this.clientRemote.requestWithResponse(
        'setLocationPermissionState', {enabled});
  }

  setTabContextPermissionState(enabled: boolean): Promise<void> {
    if (this.enableDefaultTabContextSettingFeature) {
      this.permissionStateTabContext.assignAndSignal(enabled);
      return Promise.resolve();
    }
    return this.clientRemote.requestWithResponse(
        'setTabContextPermissionState', {enabled});
  }

  setClosedCaptioningSetting?(enabled: boolean): Promise<void> {
    return this.clientRemote.requestWithResponse(
        'setClosedCaptioningSetting', {enabled});
  }

  setContextAccessIndicator(show: boolean): void {
    this.clientRemote.requestWithResponse('setContextAccessIndicator', {show});
  }

  setActuationOnWebSetting?(enabled: boolean): Promise<void> {
    return this.clientRemote.requestWithResponse(
        'setActuationOnWebSetting', {enabled});
  }

  async getUserProfileInfo?(): Promise<UserProfileInfo> {
    return this.enableCachedGetUserProfileInfo ? this.fetchUserProfileCached() :
                                                 this.fetchUserProfileDirect();
  }

  private async fetchUserProfileDirect(): Promise<UserProfileInfo> {
    const {profileInfo} = await this.clientRemote.requestWithResponse(
        'getUserProfileInfo', undefined);
    if (!profileInfo) {
      throw new Error('getUserProfileInfo failed');
    }
    const {avatarIcon} = profileInfo;
    return replaceProperties(profileInfo, {
      avatarIcon: async () =>
          avatarIcon && Promise.resolve(rgbaImageToBlob(avatarIcon)),
    });
  }

  private async fetchUserProfileCached(): Promise<UserProfileInfo> {
    if (this.cachedUserProfile) {
      return this.cachedUserProfile;
    }

    this.cachedUserProfile = (async () => {
      try {
        const {profileInfo} = await this.clientRemote.requestWithResponse(
            'getUserProfileInfo', undefined);

        if (!profileInfo) {
          throw new Error('getUserProfileInfo failed');
        }

        let blobPromise: Promise<Blob|undefined>|undefined;
        return replaceProperties(profileInfo, {
          avatarIcon: () => {
            if (blobPromise) {
              return blobPromise;
            }
            if (!profileInfo.avatarIcon) {
              blobPromise = Promise.resolve(undefined);
              return blobPromise;
            }
            const newBlob = rgbaImageToBlob(profileInfo.avatarIcon);
            // Clear memory after conversion
            profileInfo.avatarIcon = undefined;
            blobPromise = Promise.resolve(newBlob);
            return blobPromise;
          },
        });
      } catch (e) {
        this.cachedUserProfile = undefined;
        throw e;
      }
    })();

    return this.cachedUserProfile;
  }

  async refreshSignInCookies(): Promise<void> {
    const result = await this.clientRemote.requestWithResponse(
        'refreshSignInCookies', undefined);
    if (!result.success) {
      throw Error('refreshSignInCookies failed');
    }
  }

  setAudioDucking?(enabled: boolean): void {
    this.clientRemote.requestNoResponse('setAudioDucking', {enabled});
  }

  getJournalHost?(): GlicBrowserHostJournal {
    assert(this.journalHost);
    return this.journalHost;
  }

  getMetrics(): GlicBrowserHostMetrics {
    return this.metrics;
  }

  scrollTo?(params: ScrollToParams): Promise<void> {
    return this.clientRemote.requestWithResponse('scrollTo', {params});
  }

  setSyntheticExperimentState(trialName: string, groupName: string): void {
    this.clientRemote.requestNoResponse(
        'setSyntheticExperimentState', {trialName, groupName});
  }

  openOsPermissionSettingsMenu?(permission: string): void {
    this.clientRemote.requestNoResponse(
        'openOsPermissionSettingsMenu', {permission});
  }

  async getOsMicrophonePermissionStatus(): Promise<boolean> {
    return (await this.clientRemote.requestWithResponse(
                'getOsMicrophonePermissionStatus', undefined))
        .enabled;
  }

  isManuallyResizing(): ObservableValueImpl<boolean> {
    return this.manuallyResizing;
  }

  getOsHotkeyState(): ObservableValueImpl<{hotkey: string}> {
    return this.osHotkeyState;
  }

  getPinnedTabs?(): ObservableValueImpl<TabData[]> {
    return this.pinnedTabs;
  }

  async pinTabs?(tabIds: string[], options?: PinTabsOptions): Promise<boolean> {
    return (await this.clientRemote.requestWithResponse(
                'pinTabs', {tabIds, options}))
        .pinnedAll;
  }

  async unpinTabs?
      (tabIds: string[], options?: UnpinTabsOptions): Promise<boolean> {
    return (await this.clientRemote.requestWithResponse(
                'unpinTabs', {tabIds, options}))
        .unpinnedAll;
  }

  async createSkill?(request: CreateSkillRequest): Promise<void> {
    const result =
        await this.clientRemote.requestWithResponse('createSkill', {request});
    if (!result.modalOpened) {
      throw new Error('createSkill: failed to open dialog');
    }
  }

  async updateSkill?(request: UpdateSkillRequest): Promise<void> {
    const result =
        await this.clientRemote.requestWithResponse('updateSkill', {request});
    if (!result.modalOpened) {
      throw new Error('updateSkill: failed to open dialog');
    }
  }

  showManageSkillsUi?(): void {
    this.clientRemote.requestNoResponse('showManageSkillsUi', undefined);
  }

  showBrowseSkillsUi?(): void {
    this.clientRemote.requestNoResponse('showBrowseSkillsUi', undefined);
  }

  async getSkill?(id: string): Promise<Skill> {
    const result =
        await this.clientRemote.requestWithResponse('getSkill', {id});
    if (!result.skill) {
      throw new Error('getSkill: failed');
    }
    this.webClientMessageHandler.cacheSkillPrompt(result.skill);
    return result.skill;
  }

  recordSkillsWebClientEvent?(event: SkillsWebClientEvent): void {
    this.clientRemote.requestNoResponse('recordSkillsWebClientEvent', {event});
  }

  getSkillPreviews?(): ObservableValue<SkillPreview[]> {
    return this.skillPreviews;
  }

  getSkillToInvoke?(): ObservableValue<Skill> {
    return this.skillToInvoke;
  }

  unpinAllTabs?(options?: UnpinTabsOptions): void {
    this.clientRemote.requestNoResponse('unpinAllTabs', {options});
  }

  getPinCandidates?
      (options: GetPinCandidatesOptions): ObservableValue<PinCandidate[]> {
    this.pinCandidates?.setObsolete();
    return this.pinCandidates = new PinCandidatesObservable(
               this.idGenerator.next(), this.clientRemote, options);
  }

  async getZeroStateSuggestionsForFocusedTab?
      (isFirstRun?: boolean): Promise<ZeroStateSuggestions> {
    const zeroStateResult = await this.clientRemote.requestWithResponse(
        'getZeroStateSuggestionsForFocusedTab', {isFirstRun});
    if (!zeroStateResult.suggestions) {
      return {
        suggestions: [],
        tabId: '',
        url: '',
      };
    }
    return zeroStateResult.suggestions;
  }

  private async zeroStateActiveSubscriptionStateChanged(
      options: ZeroStateSuggestionsOptions, hasActiveSubscription: boolean) {
    if (options !== this.currentZeroStateSuggestionOptions) {
      // Dont send out of date updates.
      return;
    }
    const zeroStateResult = await this.clientRemote.requestWithResponse(
        'getZeroStateSuggestionsAndSubscribe', {
          hasActiveSubscription: hasActiveSubscription,
          options: options,
        });
    if (zeroStateResult.suggestions) {
      this.currentZeroStateObserver?.assignAndSignal(
          zeroStateResult.suggestions);
    }
  }

  getZeroStateSuggestions?(options?: ZeroStateSuggestionsOptions):
      ObservableValueImpl<ZeroStateSuggestionsV2> {
    options = options ?? {
      isFirstRun: false,
      supportedTools: [],
    };
    this.currentZeroStateSuggestionOptions = options;
    this.currentZeroStateObserver =
        ObservableValueImpl.withNoValue<ZeroStateSuggestionsV2>(
            this.zeroStateActiveSubscriptionStateChanged.bind(this, options));
    return this.currentZeroStateObserver;
  }

  dropScrollToHighlight?(): void {
    this.clientRemote.requestNoResponse('dropScrollToHighlight', undefined);
  }

  maybeRefreshUserStatus?(): void {
    this.cachedUserProfile = undefined;
    this.clientRemote.requestNoResponse('maybeRefreshUserStatus', undefined);
  }

  getAdditionalContext?(): Observable<AdditionalContext> {
    return this.additionalContextSubject;
  }

  getHostCapabilities(): Set<HostCapability> {
    return this.hostCapabilities;
  }

  getPageMetadata?
      (tabId: string, names: string[]): ObservableValueImpl<PageMetadata> {
    if (this.pageMetadataObservers.has(tabId)) {
      // Currently, we assume that names do not change and keep only
      // one observer per tabId.
      return this.pageMetadataObservers.get(tabId)!;
    }

    if (names.length === 0) {
      throw Error('names must not be empty');
    }

    const observableValue = ObservableValueImpl.withNoValue<PageMetadata>(
        async (isActive: boolean) => {
          // If the client subscribes to an Observable with an invalid tabId,
          // it will emit nothing, even if the tab later becomes valid.
          const {success} = await this.clientRemote.requestWithResponse(
              'subscribeToPageMetadata', {tabId, names: isActive ? names : []});
          if (!success) {
            if (!observableValue.isStopped()) {
              observableValue.complete();
            }
            this.pageMetadataObservers.delete(tabId);
          }
        });
    this.pageMetadataObservers.set(tabId, observableValue);
    return observableValue;
  }

  selectCredentialDialogRequestHandler?
      (): Observable<SelectCredentialDialogRequest> {
    return this.selectCredentialDialogRequestSubject;
  }

  selectUserConfirmationDialogRequestHandler():
      Observable<UserConfirmationDialogRequest> {
    return this.userConfirmationDialogRequestSubject;
  }

  selectNavigationConfirmationRequestHandler():
      Observable<NavigationConfirmationRequest> {
    return this.navigationConfirmationRequestSubject;
  }

  getActOnWebCapability?(): ObservableValue<boolean> {
    return this.actOnWebCapabilityValue;
  }

  selectAutofillSuggestionsDialogRequestHandler?
      (): Observable<SelectAutofillSuggestionsDialogRequest> {
    return this.selectAutofillSuggestionsDialogRequestSubject;
  }

  setOnboardingCompleted?(): void {
    return this.clientRemote.requestNoResponse(
        'setOnboardingCompleted', undefined);
  }

  isOnboardingCompleted?(): ObservableValue<boolean> {
    return this.onboardingCompleted;
  }

  actorTaskListRowClicked?(): Observable<number> {
    return this.actorTaskListRowClickedSubject;
  }
}

class GlicBrowserHostMetricsImpl implements GlicBrowserHostMetrics {
  constructor(private sender: PostMessageRemote<WebClientHost>) {}

  onOptinImpression(): void {
    this.sender.requestNoResponse('onOptinImpression', undefined);
  }

  onUserInputSubmitted(mode: number): void {
    this.sender.requestNoResponse('onUserInputSubmitted', {mode});
  }

  onReaction(reactionType: number): void {
    this.sender.requestNoResponse('onReaction', {reactionType});
  }

  onPerformActionResultSubmitted(isRetry?: boolean): void {
    this.sender.requestNoResponse('onActionSubmitted', {isRetry});
  }

  onContextUploadStarted(): void {
    this.sender.requestNoResponse('onContextUploadStarted', undefined);
  }

  onContextUploadCompleted(): void {
    this.sender.requestNoResponse('onContextUploadCompleted', undefined);
  }

  onResponseStarted(): void {
    this.sender.requestNoResponse('onResponseStarted', undefined);
  }

  onResponseStopped(details?: OnResponseStoppedDetails): void {
    this.sender.requestNoResponse('onResponseStopped', {details});
  }

  onSessionTerminated(): void {
    this.sender.requestNoResponse('onSessionTerminated', undefined);
  }

  onResponseRated(positive: boolean): void {
    this.sender.requestNoResponse('onResponseRated', {positive});
  }

  onClosedCaptionsShown?(): void {
    this.sender.requestNoResponse('onClosedCaptionsShown', undefined);
  }

  onTurnCompleted?(model: number, duration: number): void {
    this.sender.requestNoResponse('onTurnCompleted', {model, duration});
  }

  onRecordUseCounter?(counter: number): void {
    // Since the frontend can contain a newer version than what Chrome is
    // built against, we use a sparse histogram.
    this.sender.requestNoResponse(
        'recordHistogram', {name: 'Glic.Api.UseCounter', sparseValue: counter});
  }
}

export class IdGenerator {
  private nextId = 1;

  next(): number {
    return this.nextId++;
  }
}

class CaptureRegionObservable extends ObservableValueImpl<CaptureRegionResult> {
  observationId: number;

  constructor(
      observationId: number, private sender: PostMessageRemote<WebClientHost>,
      private params?: CaptureRegionParams) {
    super(false);
    this.observationId = observationId;
  }

  override activeSubscriptionChanged(hasActiveSubscription: boolean): void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    if (this.isStopped()) {
      return;
    }
    if (hasActiveSubscription) {
      this.sender.requestNoResponse('subscribeToCaptureRegion', {
        observationId: this.observationId,
        params: this.params,
      });
    } else {
      this.sender.requestNoResponse(
          'unsubscribeFromCaptureRegion', {observationId: this.observationId});
      // Unsubscribing from the client side is a terminal event.
      this.complete();
    }
  }

  override error(e: Error) {
    if (this.isStopped()) {
      return;
    }
    super.error(e);
  }

  override complete() {
    if (this.isStopped()) {
      return;
    }
    super.complete();
  }

  processUpdate(result: CaptureRegionResult) {
    this.assignAndSignal(result);
  }

  processError(reason: CaptureRegionErrorReason) {
    this.error(new ErrorWithReasonImpl('captureRegion', reason));
  }
}

class PinCandidatesObservable extends ObservableValueImpl<PinCandidate[]> {
  private isObsolete = false;

  constructor(
      private readonly observationId: number,
      private sender: PostMessageRemote<WebClientHost>,
      private options: GetPinCandidatesOptions) {
    super(false);
  }

  override activeSubscriptionChanged(hasActiveSubscription: boolean): void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    if (this.isObsolete) {
      console.warn(`getPinCandidates() observable is in use while obsolete.`);
      return;
    }
    if (hasActiveSubscription) {
      this.sender.requestNoResponse(
          'subscribeToPinCandidates',
          {options: this.options, observationId: this.observationId});
    } else {
      this.sender.requestNoResponse(
          'unsubscribeFromPinCandidates', {observationId: this.observationId});
    }
  }

  processUpdate(candidates: PinCandidatePrivate[], observationId: number) {
    if (this.observationId !== observationId) {
      return;
    }

    this.assignAndSignal(
        candidates.map(c => ({tabData: convertTabDataFromPrivate(c.tabData)})));
  }

  // Mark this observable as obsolete. It should not be used any further.
  // Only one PinCandidatesObservable is active at one time.
  setObsolete() {
    if (this.hasActiveSubscription()) {
      console.warn(
          `getPinCandidates() observable was made obsolete with subscribers.`);
    }
    this.isObsolete = true;
  }
}

export interface ObservableSetByTabIdDelegate {
  subscribe(
      sender: PostMessageRemote<WebClientHost>, observationId: number,
      tabId: string): void;
  unsubscribe(
      sender: PostMessageRemote<WebClientHost>, observationId: number,
      tabId: string): void;
  readonly unsubscribeDelay: number;
}

// Manages a set of observables which each observe a tab.
// When a tab is closed, the corresponding observable is completed, and
// removed from the set. Otherwise, observables are kept in the set,
// so they can be re-subscribed to later.
export class ObservableSetByTabId<ObservedType> {
  observablesById =
      new Map<number, ObservableSetByTabIdObservable<ObservedType>>();
  observableIdsByTabId = new Map<string, number>();

  constructor(
      private delegate: ObservableSetByTabIdDelegate,
      private sender: PostMessageRemote<WebClientHost>,
      private idGenerator: IdGenerator) {}

  completeObservable(observationId: number) {
    const obs = this.observablesById.get(observationId);
    if (!obs) {
      return;
    }
    obs.complete();
    // Prune a bit later, so that requests for a recently deleted tab
    // don't create another subscription. Note that this is just an
    // optimization, a new subscription would resolve appropriately.
    window.setTimeout(() => {
      this.prune(observationId);
    }, this.delegate.unsubscribeDelay);
  }

  assignAndSignal(observationId: number, value: ObservedType) {
    const obs = this.observablesById.get(observationId);
    if (!obs) {
      return;
    }
    obs.assignAndSignal(value);
  }

  getObservableByTabId(tabId: string):
      ObservableSetByTabIdObservable<ObservedType> {
    let obsId = this.observableIdsByTabId.get(tabId);
    if (obsId !== undefined) {
      return this.observablesById.get(obsId)!;
    }
    obsId = this.idGenerator.next();
    this.observableIdsByTabId.set(tabId, obsId);
    const obs = new ObservableSetByTabIdObservable<ObservedType>(
        tabId, this.sender, obsId, this.delegate);
    this.observablesById.set(obsId, obs);
    return obs;
  }

  private prune(observationId: number): void {
    const obs = this.observablesById.get(observationId);
    if (!obs) {
      return;
    }
    this.observableIdsByTabId.delete(obs.tabId);
    this.observablesById.delete(observationId);
  }
}

export class ObservableSetByTabIdObservable<ObservedType> extends
    ObservableValueImpl<ObservedType> {
  private delegateSubscribed = false;
  private unsubscribeTimer: OneShotTimer;
  constructor(
      public tabId: string, private sender: PostMessageRemote<WebClientHost>,
      private observationId: number,
      private delegate: ObservableSetByTabIdDelegate) {
    super(/*isSet=*/ false);
    this.unsubscribeTimer = new OneShotTimer(delegate.unsubscribeDelay);
  }

  override activeSubscriptionChanged(hasActiveSubscription: boolean): void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    if (!hasActiveSubscription) {
      this.unsubscribeTimer.start(() => {
        if (this.hasActiveSubscription()) {
          return;
        }
        this.delegateSubscribed = false;
        this.delegate.unsubscribe(this.sender, this.observationId, this.tabId);
      });
      return;
    }
    this.unsubscribeTimer.reset();
    if (!this.delegateSubscribed) {
      this.delegateSubscribed = true;
      this.delegate.subscribe(this.sender, this.observationId, this.tabId);
    }
  }
}

class GetTabByIdObservableSetImpl implements ObservableSetByTabIdDelegate {
  readonly unsubscribeDelay = 1000;
  subscribe(
      sender: PostMessageRemote<WebClientHost>, observationId: number,
      tabId: string): void {
    sender.requestNoResponse(
        'subscribeToTabData', {tabId, observationId, cancel: false});
  }

  unsubscribe(
      sender: PostMessageRemote<WebClientHost>, observationId: number,
      tabId: string): void {
    sender.requestNoResponse(
        'subscribeToTabData', {tabId, observationId, cancel: true});
  }
}

class GetTabFaviconByIdObservableSetImpl implements
    ObservableSetByTabIdDelegate {
  readonly unsubscribeDelay = 1000;
  subscribe(
      sender: PostMessageRemote<WebClientHost>, observationId: number,
      tabId: string): void {
    sender.requestNoResponse(
        'subscribeToTabFavicon', {tabId, observationId, cancel: false});
  }

  unsubscribe(
      sender: PostMessageRemote<WebClientHost>, observationId: number,
      tabId: string): void {
    sender.requestNoResponse(
        'subscribeToTabFavicon', {tabId, observationId, cancel: true});
  }
}

function convertTabDataFromPrivate(data: TabDataPrivate): TabData;
function convertTabDataFromPrivate(data: TabDataPrivate|undefined): TabData|
    undefined;
function convertTabDataFromPrivate(data: TabDataPrivate|undefined): TabData|
    undefined {
  if (!data) {
    return undefined;
  }
  let faviconResult: Promise<Blob>|undefined;
  const dataFavicon = data.favicon;
  async function getFavicon() {
    if (dataFavicon && !faviconResult) {
      faviconResult = Promise.resolve(rgbaImageToBlob(dataFavicon));
      return faviconResult;
    }
    return faviconResult;
  }

  const favicon = dataFavicon && getFavicon;
  return replaceProperties(data, {favicon});
}

function convertFocusedTabDataFromPrivate(data: FocusedTabDataPrivate):
    FocusedTabData {
  const result: FocusedTabData = {};
  if (data.hasFocus) {
    result.hasFocus = replaceProperties(data.hasFocus, {
      tabData: convertTabDataFromPrivate(data.hasFocus.tabData),
    });
  }
  if (data.hasNoFocus) {
    result.hasNoFocus = replaceProperties(data.hasNoFocus, {
      tabFocusCandidateData:
          convertTabDataFromPrivate(data.hasNoFocus.tabFocusCandidateData),
    });
  }
  return result;
}

function streamFromBuffer(buffer: Uint8Array): ReadableStream<Uint8Array> {
  return new ReadableStream<Uint8Array>({
    start(controller) {
      controller.enqueue(buffer);
      controller.close();
    },
  });
}

function convertPdfDocumentDataFromPrivate(data: PdfDocumentDataPrivate):
    PdfDocumentData {
  const pdfData =
      data.pdfData && streamFromBuffer(new Uint8Array(data.pdfData));
  return replaceProperties(data, {pdfData});
}

function convertAnnotatedPageDataFromPrivate(data: AnnotatedPageDataPrivate):
    AnnotatedPageData {
  const annotatedPageContent = data.annotatedPageContent &&
      streamFromBuffer(new Uint8Array(data.annotatedPageContent));
  return replaceProperties(data, {annotatedPageContent});
}

function convertTabContextResultFromPrivate(data: ResumeActorTaskResultPrivate):
    ResumeActorTaskResult;
function convertTabContextResultFromPrivate(data: TabContextResultPrivate):
    TabContextResult;
function convertTabContextResultFromPrivate(
    data: TabContextResultPrivate|
    ResumeActorTaskResultPrivate): TabContextResult|ResumeActorTaskResult {
  const tabData = convertTabDataFromPrivate(data.tabData);
  const pdfDocumentData = data.pdfDocumentData &&
      convertPdfDocumentDataFromPrivate(data.pdfDocumentData);
  const annotatedPageData = data.annotatedPageData &&
      convertAnnotatedPageDataFromPrivate(data.annotatedPageData);
  return replaceProperties(data, {tabData, pdfDocumentData, annotatedPageData});
}

function convertAdditionalContextFromPrivate(context: AdditionalContextPrivate):
    AdditionalContext {
  const parts = context.parts.map(p => {
    const annotatedPageData = p.annotatedPageData &&
        convertAnnotatedPageDataFromPrivate(p.annotatedPageData);
    const pdf = p.pdf && convertPdfDocumentDataFromPrivate(p.pdf);
    const data = p.data && new Blob([p.data.data], {type: p.data.mimeType});
    const filename = p.filename;
    const tabContext =
        p.tabContext && convertTabContextResultFromPrivate(p.tabContext);
    return {
      ...p,
      data,
      filename,
      annotatedPageData,
      pdf,
      tabContext,
    };
  });
  return {
    ...context,
    parts,
  };
}

function convertInvokeOptionsFromPrivate(options: InvokeOptionsPrivate):
    InvokeOptions {
  return {
    ...options,
    context: options.context ?
        convertAdditionalContextFromPrivate(options.context) :
        undefined,
  };
}
