// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PinCandidatesObserverReceiver} from '../../glic.mojom-webui.js';
import type {PinCandidate as PinCandidateMojo, PinCandidatesObserverInterface, WebClientHandlerRemote} from '../../glic.mojom-webui.js';
import {CaptureRegionErrorReason, HostCapability} from '../../glic_api/glic_api.js';
import type {ActivateTabOptions, AdditionalContext, AnnotatedPageData, CaptureRegionParams, CaptureRegionResult, ChromeVersion, ClientCapabilities, ClientErrorDialogType, ConversationInfo, CounterAbuseVerdict, CreateTabOptions, FileUploadPolicyState, FocusedTabData, FormFactor, GeminiEnterpriseSettings, GetPinCandidatesOptions, GlicBrowserHost, GlicBrowserHostMetrics, GlicHostRegistry, GlicWebClient, ImageBytesResult, ImageInfo, InvokeOptions, MicrophoneStatus, Observable, ObservableValue, OnResponseStoppedDetails, OpenPanelInfo, OpenPinnedTabPickerOptions, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, PinCandidate, PinTabsOptions, Platform, PromptType, ResizeWindowOptions, ResumeActorTaskResult, Screenshot, TabContextOptions, TabContextResult, TabData, UnpinTabsOptions, UserProfileInfo, WebClientMode, ZeroStateSuggestions} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl, Subject} from '../../observable.js';
import {GlicBrowserHostActor} from '../actor/actor_client.js';
import {GlicBrowserHostAnnotation} from '../annotation/annotation_client.js';
import {GlicBrowserHostExperimentalTriggering} from '../experimental_triggering/experimental_triggering_client.js';
import {getPinCandidatesOptionsFromClient, pinCandidateToClient} from '../host/conversions.js';
import {PanelOpenState} from '../host/types.js';
import {GlicBrowserHostSkills} from '../skills/skills_client.js';
import {assertNever} from '../transport/messaging.js';
import type {createDirectMessagingPair, PendingRemote, PostMessageHandler, PostMessageReceiver, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';
import {GlicBrowserHostZeroStateSuggestions} from '../zero_state_suggestions/zero_state_suggestions_client.js';

import {replaceProperties} from './../conversions.js';
import {ErrorWithReasonImpl, newTransferableException, WebClientDef, WebClientRegionCaptureDef, WebClientTabDataObserverDef, WebClientTabFaviconObserverDef} from './../request_types.js';
import type {AdditionalContextPrivate, AnnotatedPageDataPrivate, FocusedTabDataPrivate, GlicException, ImageBytesResultPrivate, ImageInfoPrivate, InvokeOptionsPrivate, PdfDocumentDataPrivate, ResumeActorTaskResultPrivate, RgbaImage, TabContextResultPrivate, TabDataPrivate, WebClient, WebClientHost, WebClientRegionCapture, WebClientTabDataObserver, WebClientTabFaviconObserver} from './../request_types.js';
import type {GlicBrowserHostBaseContext} from './glic_client_common.js';
import {createDelegationProxy} from './glic_client_common.js';
import {rgbaImageToBlob} from './image_utils.js';
import type {ObservableSetByTabIdDelegate} from './observable_set_by_tab_id.js';
import {ObservableSetByTabId} from './observable_set_by_tab_id.js';

// Web client side of the Glic API.
// Communicates with the Chrome-WebUI-side in glic_api_host.ts

export class GlicHostRegistryImpl implements GlicHostRegistry {
  private host: GlicBrowserHostImpl|undefined;
  constructor(
      private directPair: ReturnType<
          typeof createDirectMessagingPair<WebClientHost, WebClient>>,
      private handler?: WebClientHandlerRemote,
  ) {}

  async registerWebClient(webClient: GlicWebClient): Promise<void> {
    this.host =
        new GlicBrowserHostImpl(webClient, this.directPair, this.handler);
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
class WebClientMessageHandler implements PostMessageHandler<WebClient> {
  private cachedPinnedTabs: TabData[]|undefined = undefined;

  constructor(
      private webClient: GlicWebClient, private host: GlicBrowserHostImpl) {}

  async notifyPanelWillOpen(payload: {
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
    } finally {
      this.host.panelOpenStateChanged(PanelOpenState.OPEN);
    }
    return {openPanelInfo};
  }

  async checkResponsive(): Promise<void> {
    await this.webClient.checkResponsive?.();
  }

  async notifyPanelWasClosed(): Promise<void> {
    this.host.panelOpenStateChanged(PanelOpenState.CLOSED);
    try {
      this.host.notifyPanelWillOpenCompleted = Promise.withResolvers<void>();
      await this.webClient.notifyPanelWasClosed?.();
    } catch (e) {
      console.warn(e);
    }
  }

  panelStateChanged(payload: {panelState: PanelState}): void {
    this.host.getPanelState?.().assignAndSignal(payload.panelState);
  }


  canAttachStateChanged(payload: {canAttach: boolean}): void {
    this.host.canAttachPanelValue.assignAndSignal(payload.canAttach);
  }

  notifyGeminiEnterpriseSettingsChanged(payload: {
    settings: GeminiEnterpriseSettings|undefined,
  }) {
    this.host.getGeminiEnterpriseSettings?.().assignAndSignal(payload.settings);
  }

  notifyMicrophonePermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getMicrophonePermissionState().assignAndSignal(payload.enabled);
  }

  async stopMicrophone(): Promise<void> {
    await this.webClient.stopMicrophone?.();
  }

  notifyLocationPermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getLocationPermissionState().assignAndSignal(payload.enabled);
  }

  notifyTabContextPermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getTabContextPermissionState().assignAndSignal(payload.enabled);
  }

  notifyDefaultTabContextPermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.defaultTabContextPermission.assignAndSignal(payload.enabled);
  }

  notifyOsLocationPermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getOsLocationPermissionState().assignAndSignal(payload.enabled);
  }

  notifyClosedCaptioningSettingChanged(payload: {
    enabled: boolean,
  }) {
    this.host.closedCaptioningState.assignAndSignal(payload.enabled);
  }

  async invoke(payload: {options: InvokeOptionsPrivate}): Promise<void> {
    try {
      const options = convertInvokeOptionsFromPrivate(payload.options);
      // Wait until notifyPanelWillOpen has resolved before invoking.
      await this.host.notifyPanelWillOpenCompleted.promise;
      await this.webClient.invoke?.(options);
    } catch (e) {
      console.warn(e);
    }
  }

  notifyActuationOnWebSettingChanged(payload: {
    enabled: boolean,
  }) {
    this.host.actuationOnWebState.assignAndSignal(payload.enabled);
  }

  notifyFileUploadStateChanged(payload: {
    state: FileUploadPolicyState,
  }) {
    this.host.fileUploadAllowedState.assignAndSignal(payload.state);
  }

  notifyFocusedTabChanged(payload: {
    focusedTabDataPrivate: FocusedTabDataPrivate,
  }) {
    const focusedTabData =
        convertFocusedTabDataFromPrivate(payload.focusedTabDataPrivate);
    this.host.getFocusedTabStateV2().assignAndSignal(focusedTabData);
  }

  notifyZoomLevelChanged(payload: {zoomFactor: number}) {
    this.host.getZoomLevel().assignAndSignal(payload.zoomFactor);
  }

  notifyPanelActiveChanged(payload: {panelActive: boolean}): void {
    this.host.panelActiveValue.assignAndSignal(payload.panelActive);
  }

  notifyManualResizeChanged(payload: {resizing: boolean}) {
    this.host.isManuallyResizing().assignAndSignal(payload.resizing);
  }

  browserIsOpenChanged(payload: {browserIsOpen: boolean}) {
    this.host.isBrowserOpenValue.assignAndSignal(payload.browserIsOpen);
  }

  notifyOsHotkeyStateChanged(payload: {hotkey: string}) {
    this.host.getOsHotkeyState().assignAndSignal(payload);
  }

  notifyPinnedTabsChanged(payload: {tabData: TabDataPrivate[]}): void {
    this.cachedPinnedTabs =
        payload.tabData.map((x) => convertTabDataFromPrivate(x));
    this.host.pinnedTabs?.assignAndSignal(this.cachedPinnedTabs);
  }

  notifyPinnedTabDataChanged(payload: {tabData: TabDataPrivate}): void {
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


  pageMetadataChanged(
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

  notifyAdditionalContext(payload: {
    context: AdditionalContextPrivate,
  }): void {
    const context = convertAdditionalContextFromPrivate(payload.context);
    this.host.additionalContextSubject.next(context);
  }

  notifyActOnWebCapabilityChanged(payload: {
    canActOnWeb: boolean,
  }): void {
    this.host.actorClient.actOnWebCapabilityValue.assignAndSignal(
        payload.canActOnWeb);
  }

  onboardingCompletedChanged(payload: {completed: boolean}): void {
    this.host.onboardingCompleted.assignAndSignal(payload.completed);
  }

  notifyActorTaskListRowClicked(payload: {taskId: number}): void {
    this.host.actorClient.actorTaskListRowClickedSubject.next(payload.taskId);
  }
}

class WebClientRegionCaptureHandler implements
    PostMessageHandler<WebClientRegionCapture> {
  constructor(private observable: CaptureRegionObservable) {}

  captureRegionUpdate(payload: {
    result?: CaptureRegionResult,
    reason?: CaptureRegionErrorReason,
  }): void {
    if (payload.result) {
      this.observable.processUpdate(payload.result);
    } else if (payload.reason !== undefined) {
      this.observable.processError(payload.reason);
    }
  }
}

export class GlicBrowserHostImpl implements GlicBrowserHostBaseContext,
                                            GlicBrowserHost {
  readonly router: PostMessageRouter;
  protected webClientMessageHandler: WebClientMessageHandler;
  readonly clientRemote: PostMessageRemote<WebClientHost>;

  readonly actorClient: GlicBrowserHostActor;
  readonly annotationClient: GlicBrowserHostAnnotation;
  readonly skillsClient: GlicBrowserHostSkills;
  readonly experimentalTriggeringClient =
      new GlicBrowserHostExperimentalTriggering();
  readonly suggestionsClient: GlicBrowserHostZeroStateSuggestions;

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
  fileUploadAllowedState =
      ObservableValueImpl.withNoValue<FileUploadPolicyState>();
  private osHotkeyState = ObservableValueImpl.withNoValue<{hotkey: string}>();
  onboardingCompleted = ObservableValueImpl.withNoValue<boolean>();
  panelActiveValue = ObservableValueImpl.withNoValue<boolean>();
  isBrowserOpenValue = ObservableValueImpl.withNoValue<boolean>();
  private metrics: GlicBrowserHostMetricsImpl;
  private manuallyResizing = ObservableValueImpl.withValue<boolean>(false);
  private cachedUserProfile?: Promise<UserProfileInfo>;
  private enableCachedGetUserProfileInfo?: boolean;
  pinnedTabs = ObservableValueImpl.withNoValue<TabData[]>();
  pinCandidates: PinCandidatesObservable|undefined;
  captureRegionObservable?: CaptureRegionObservable;

  private hostCapabilities: Set<HostCapability> = new Set();
  readonly additionalContextSubject = new Subject<AdditionalContext>();
  pageMetadataObservers: Map<string, ObservableValueImpl<PageMetadata>> =
      new Map();

  getTabByIdObservableSet:
      ObservableSetByTabId<TabData, WebClientTabDataObserver>;
  getTabFaviconByIdObservableSet:
      ObservableSetByTabId<Blob|undefined, WebClientTabFaviconObserver>;
  notifyPanelWillOpenCompleted = Promise.withResolvers<void>();
  private panelOpenState = PanelOpenState.CLOSED;

  constructor(
      public webClient: GlicWebClient,
      directPair: ReturnType<
          typeof createDirectMessagingPair<WebClientHost, WebClient>>,
      private handler?: WebClientHandlerRemote,
  ) {
    this.webClientMessageHandler =
        new WebClientMessageHandler(this.webClient, this);
    this.router = directPair.client.router;
    this.clientRemote = directPair.client.rootRemote;
    directPair.client.rootReceiver.setMessageHandler(
        this.webClientMessageHandler, WebClientDef);

    this.actorClient = new GlicBrowserHostActor(this);
    this.annotationClient = new GlicBrowserHostAnnotation(this);
    this.skillsClient = new GlicBrowserHostSkills();
    this.suggestionsClient = new GlicBrowserHostZeroStateSuggestions(this);

    this.getTabByIdObservableSet =
        new ObservableSetByTabId<TabData, WebClientTabDataObserver>(
            new GetTabByIdObservableSetImpl(), this.clientRemote, this.router);
    this.getTabFaviconByIdObservableSet =
        new ObservableSetByTabId<Blob|undefined, WebClientTabFaviconObserver>(
            new GetTabFaviconByIdObservableSetImpl(), this.clientRemote,
            this.router);
    this.metrics = new GlicBrowserHostMetricsImpl(this.clientRemote);

    const proxy = createDelegationProxy(this as GlicBrowserHostImpl, [
      this.actorClient,
      this.annotationClient,
      this.skillsClient,
      this.suggestionsClient,
    ]);
    type UnimplementedApis = Exclude<keyof GlicBrowserHost, keyof typeof proxy>;
    assertNever<UnimplementedApis>();
    return proxy as unknown as GlicBrowserHostImpl;
  }

  isPanelOpen(): boolean {
    return this.panelOpenState === PanelOpenState.OPEN;
  }

  panelOpenStateChanged(state: PanelOpenState) {
    this.panelOpenState = state;
    if (state === PanelOpenState.CLOSED) {
      this.pinCandidates?.disconnectFromSource();
    } else {
      if (this.pinCandidates?.hasActiveSubscription()) {
        this.pinCandidates.connectToSource();
      }
    }
  }

  getHandler(): WebClientHandlerRemote|undefined {
    return this.handler;
  }

  destroy() {
    this.pinCandidates?.setObsolete();
    this.skillsClient.destroySkills();
    this.router.destroy();
  }

  async webClientCreated(clientCapabilities: Set<ClientCapabilities>) {
    const response = await this.clientRemote.requestWithResponse(
        'webClientCreated',
        {clientCapabilities: Array.from(clientCapabilities)});
    this.actorClient.initialize(
        response.initialState, response.actorRemote, response.actorReceiver);
    this.annotationClient.initialize(response.initialState);
    this.skillsClient.initialize(response.initialState, this.handler);
    this.experimentalTriggeringClient.initialize(
        this.router, response.experimentalTriggeringReceiver, this.webClient,
        this.clientRemote);
    this.suggestionsClient.initialize(
        response.initialState, response.zeroStateSuggestionsRemote);

    const state = response.initialState;
    this.geminiEnterpriseSettings.assignAndSignal(
        state.geminiEnterpriseSettings ?? undefined);
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
    this.fileUploadAllowedState.assignAndSignal(
        state.fileUploadPolicyState as unknown as FileUploadPolicyState);
    for (const capability of state.hostCapabilities) {
      this.hostCapabilities.add(capability);
    }
    this.actorClient.actOnWebCapabilityValue.assignAndSignal(state.canActOnWeb);
    this.onboardingCompleted.assignAndSignal(state.onboardingCompleted);

    // Set the method to undefined since it's gated behind a mojo
    // RuntimeFeature. Calling a such a method when the feature is disabled
    // results in a mojo pipe closure.
    if (!this.hostCapabilities.has(
            HostCapability.GET_MODEL_QUALITY_CLIENT_ID)) {
      // MOJO_RUNTIME_FEATURE_GATED GetModelQualityClientId
      this.getModelQualityClientId = undefined;
    }

    if (!state.enableZeroStateSuggestions) {
      this.getZeroStateSuggestionsForFocusedTab = undefined;
    }

    if (!state.enableDefaultTabContextSettingFeature) {
      this.getDefaultTabContextPermissionState = undefined;
    }

    if (!state.enableMaybeRefreshUserStatus) {
      this.maybeRefreshUserStatus = undefined;
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

  // GlicBrowserHost implementation.

  getChromeVersion() {
    return Promise.resolve(this.chromeVersion!);
  }

  experimentalTriggering() {
    return this.experimentalTriggeringClient;
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

  async activateTabWithUrl(exactUrl: string, options: ActivateTabOptions = {}):
      Promise<TabData> {
    const result =
        await this.clientRemote.requestWithResponse('activateTabWithUrl', {
          exactUrl,
          options,
        });
    if (!result.tabData) {
      throw new Error('activateTabWithUrl: failed');
    }
    return convertTabDataFromPrivate(result.tabData);
  }

  openGlicSettingsPage(options?: OpenSettingsOptions): void {
    this.clientRemote.requestNoResponse('openGlicSettingsPage', {options});
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

  async getImageBytesFromTab?
      (tabId: string, documentId: string, domNodeId: number):
          Promise<ImageBytesResult> {
    const response = await this.clientRemote.requestWithResponse(
        'getImageBytesFromTab', {tabId, documentId, domNodeId});
    if (!response.result) {
      throw new Error('Failed to get image bytes');
    }
    return convertImageBytesResultFromPrivate(response.result);
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
    this.captureRegionObservable =
        new CaptureRegionObservable(this.clientRemote, this.router, params);
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

  getFileUploadAllowedCapability?
      (): ObservableValueImpl<FileUploadPolicyState> {
    return this.fileUploadAllowedState;
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

  getMetrics(): GlicBrowserHostMetrics {
    return this.metrics;
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


  unpinAllTabs?(options?: UnpinTabsOptions): void {
    this.clientRemote.requestNoResponse('unpinAllTabs', {options});
  }

  async openPinnedTabPicker?
      (options?: OpenPinnedTabPickerOptions): Promise<void> {
    await this.clientRemote.requestWithResponse(
        'openPinnedTabPicker', {options});
  }

  getPinCandidates?
      (options: GetPinCandidatesOptions): ObservableValue<PinCandidate[]> {
    this.pinCandidates?.setObsolete();
    return this.pinCandidates = new PinCandidatesObservable(this, options);
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


  setOnboardingCompleted?(): void {
    return this.clientRemote.requestNoResponse(
        'setOnboardingCompleted', undefined);
  }

  isOnboardingCompleted?(): ObservableValue<boolean> {
    return this.onboardingCompleted;
  }
}

class GlicBrowserHostMetricsImpl implements GlicBrowserHostMetrics {
  constructor(private sender: PostMessageRemote<WebClientHost>) {}

  onOptinImpression(): void {
    this.sender.requestNoResponse('onOptinImpression', undefined);
  }

  onUserInputSubmitted(mode: number, promptType?: PromptType): void {
    this.sender.requestNoResponse('onUserInputSubmitted', {mode, promptType});
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


export class CaptureRegionObservable extends
    ObservableValueImpl<CaptureRegionResult> {
  private receiver?: PostMessageReceiver;
  constructor(
      private remote: PostMessageRemote<WebClientHost>,
      private router: PostMessageRouter, private params?: CaptureRegionParams) {
    super(false);
  }

  private close() {
    this.receiver?.close();
    this.receiver = undefined;
  }

  override activeSubscriptionChanged(hasActiveSubscription: boolean): void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    if (this.isStopped()) {
      return;
    }
    if (hasActiveSubscription) {
      const {receiver, remote} =
          this.router.newPipeWithReceiver<WebClientRegionCapture>(
              new WebClientRegionCaptureHandler(this),
              WebClientRegionCaptureDef);
      this.receiver = receiver;
      this.receiver.addCloseHandler(() => {
        this.processError(CaptureRegionErrorReason.UNKNOWN);
      });
      this.remote.requestNoResponse('subscribeToCaptureRegion', {
        remote,
        params: this.params,
      });
    } else {
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
    this.close();
  }

  processUpdate(result: CaptureRegionResult) {
    this.assignAndSignal(result);
  }

  processError(reason: CaptureRegionErrorReason) {
    this.error(new ErrorWithReasonImpl('captureRegion', reason));
    this.close();
  }
}

// TODO(harringtond): Instead of watching for panel state changes in typescript,
// we should probably just have c++ code not send updates when the panel is not
// visible.
class PinCandidatesObservable extends ObservableValueImpl<PinCandidate[]>
    implements PinCandidatesObserverInterface {
  private isObsolete = false;
  private isConnected = false;
  private receiver?: PinCandidatesObserverReceiver;

  constructor(
      private host: GlicBrowserHostImpl,
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
      if (this.host.isPanelOpen()) {
        this.connectToSource();
      }
    } else {
      this.disconnectFromSource();
    }
  }

  connectToSource(): void {
    if (this.isObsolete || this.isConnected) {
      return;
    }
    const handler = this.host.getHandler();
    if (!handler) {
      return;
    }
    this.isConnected = true;
    this.receiver = new PinCandidatesObserverReceiver(this);
    handler.subscribeToPinCandidates(
        getPinCandidatesOptionsFromClient(this.options),
        this.receiver.$.bindNewPipeAndPassRemote());
  }

  disconnectFromSource(): void {
    if (!this.isConnected) {
      return;
    }
    this.isConnected = false;
    this.receiver?.$.close();
    this.receiver = undefined;
  }

  onPinCandidatesChanged(candidates: PinCandidateMojo[]): void {
    this.assignAndSignal(candidates.map(c => pinCandidateToClient(c)));
  }

  // Mark this observable as obsolete. It should not be used any further.
  // Only one PinCandidatesObservable is active at one time.
  setObsolete() {
    if (this.hasActiveSubscription()) {
      console.warn(
          `getPinCandidates() observable was made obsolete with subscribers.`);
    }
    this.isObsolete = true;
    this.disconnectFromSource();
  }
}



class GetTabByIdObservableSetImpl implements
    ObservableSetByTabIdDelegate<TabData, WebClientTabDataObserver> {
  readonly interfaceDef = WebClientTabDataObserverDef;
  readonly unsubscribeDelay = 1000;

  subscribe(
      clientRemote: PostMessageRemote<WebClientHost>, tabId: string,
      remote: PendingRemote<WebClientTabDataObserver>): void {
    clientRemote.requestNoResponse('subscribeToTabData', {tabId, remote});
  }
  createHandler(observable: ObservableValueImpl<TabData>):
      PostMessageHandler<WebClientTabDataObserver> {
    return new WebClientTabDataObserverHandler(observable);
  }
}

class WebClientTabDataObserverHandler implements
    PostMessageHandler<WebClientTabDataObserver> {
  constructor(private observable: ObservableValueImpl<TabData>) {}
  tabDataChanged(payload: {tabData: TabDataPrivate}): void {
    this.observable.assignAndSignal(convertTabDataFromPrivate(payload.tabData));
  }
}

class GetTabFaviconByIdObservableSetImpl implements
    ObservableSetByTabIdDelegate<Blob|undefined, WebClientTabFaviconObserver> {
  readonly interfaceDef = WebClientTabFaviconObserverDef;
  readonly unsubscribeDelay = 1000;
  subscribe(
      clientRemote: PostMessageRemote<WebClientHost>, tabId: string,
      remote: PendingRemote<WebClientTabFaviconObserver>): void {
    clientRemote.requestNoResponse('subscribeToTabFavicon', {tabId, remote});
  }
  createHandler(observable: ObservableValueImpl<Blob|undefined>):
      PostMessageHandler<WebClientTabFaviconObserver> {
    return new WebClientTabFaviconObserverHandler(observable);
  }
}

class WebClientTabFaviconObserverHandler implements
    PostMessageHandler<WebClientTabFaviconObserver> {
  constructor(private observable: ObservableValueImpl<Blob|undefined>) {}
  tabFaviconChanged(payload: {favicon?: RgbaImage}): void {
    if (payload.favicon === undefined) {
      this.observable.assignAndSignal(undefined);
    } else {
      this.observable.assignAndSignal(rgbaImageToBlob(payload.favicon));
    }
  }
}

export function convertTabDataFromPrivate(data: TabDataPrivate): TabData;
export function convertTabDataFromPrivate(data: TabDataPrivate|undefined):
    TabData|undefined;
export function convertTabDataFromPrivate(data: TabDataPrivate|undefined):
    TabData|undefined {
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

export function convertTabContextResultFromPrivate(
    data: ResumeActorTaskResultPrivate): ResumeActorTaskResult;
export function convertTabContextResultFromPrivate(
    data: TabContextResultPrivate): TabContextResult;
export function convertTabContextResultFromPrivate(
    data: TabContextResultPrivate|
    ResumeActorTaskResultPrivate): TabContextResult|ResumeActorTaskResult {
  const tabData = convertTabDataFromPrivate(data.tabData);
  const screenshotInfo = data.screenshotInfo &&
      streamFromBuffer(new Uint8Array(data.screenshotInfo));
  const pdfDocumentData = data.pdfDocumentData &&
      convertPdfDocumentDataFromPrivate(data.pdfDocumentData);
  const annotatedPageData = data.annotatedPageData &&
      convertAnnotatedPageDataFromPrivate(data.annotatedPageData);
  return replaceProperties(
      data, {tabData, screenshotInfo, pdfDocumentData, annotatedPageData});
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

function convertImageInfoFromPrivate(data: ImageInfoPrivate): ImageInfo {
  return {
    caption: data.caption,
    sourceOrigin: data.sourceOrigin,
    url: data.url,
    mimeType: data.mimeType,
  };
}

function convertImageBytesResultFromPrivate(data: ImageBytesResultPrivate):
    ImageBytesResult {
  return {
    bytes: data.bytes,
    imageInfo: convertImageInfoFromPrivate(data.imageInfo),
  };
}
