// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AdditionalContext, AnnotatedPageData, CaptureRegionErrorReason, CaptureRegionResult, ChromeVersion, ConversationInfo, CreateActorTabOptions, CreateTabOptions, DraggableArea, FocusedTabData, GetPinCandidatesOptions, GlicBrowserHost, GlicBrowserHostJournal, GlicBrowserHostMetrics, GlicHostRegistry, GlicWebClient, Journal, NavigationConfirmationRequest, Observable, ObservableValue, OnResponseStoppedDetails, OpenPanelInfo, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, PinCandidate, ResizeWindowOptions, ResumeActorTaskResult, Screenshot, ScrollToParams, SelectAutofillSuggestionsDialogRequest, SelectCredentialDialogRequest, TabContextOptions, TabContextResult, TabData, TaskOptions, UserConfirmationDialogRequest, UserProfileInfo, ViewChangedNotification, ViewChangeRequest, WebClientMode, ZeroStateSuggestions, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../../glic_api/glic_api.js';
import {ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason, HostCapability} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl, Subject} from '../../observable.js';

import {replaceProperties} from './../conversions.js';
import {newSenderId, PostMessageRequestReceiver, PostMessageRequestSender} from './../post_message_transport.js';
import type {ResponseExtras} from './../post_message_transport.js';
import type {AdditionalContextPrivate, AnnotatedPageDataPrivate, CredentialPrivate, FocusedTabDataPrivate, NavigationConfirmationRequestPrivate, NavigationConfirmationResponsePrivate, PdfDocumentDataPrivate, PinCandidatePrivate, RequestRequestType, RequestResponseType, ResumeActorTaskResultPrivate, RgbaImage, SelectAutofillSuggestionsDialogRequestPrivate, SelectAutofillSuggestionsDialogResponsePrivate, SelectCredentialDialogRequestPrivate, SelectCredentialDialogResponsePrivate, TabContextResultPrivate, TabDataPrivate, TransferableException, UserConfirmationDialogRequestPrivate, UserConfirmationDialogResponsePrivate, WebClientRequestTypes} from './../request_types.js';
import {ConfirmationRequestErrorReason, ErrorWithReasonImpl, ImageAlphaType, ImageColorType, newTransferableException, SelectAutofillSuggestionsDialogErrorReason, SelectCredentialDialogErrorReason} from './../request_types.js';


// Web client side of the Glic API.
// Communicates with the Chrome-WebUI-side in glic_api_host.ts

export class GlicHostRegistryImpl implements GlicHostRegistry {
  private host: GlicBrowserHostImpl|undefined;
  constructor(private windowProxy: WindowProxy) {}

  async registerWebClient(webClient: GlicWebClient): Promise<void> {
    this.host = new GlicBrowserHostImpl(webClient, this.windowProxy);
    await this.host.webClientCreated();
    let success = false;
    let exception: TransferableException|undefined;
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

type Promisify<T> = T extends void ? void : Promise<T>;

// A type which the guest should implement.
// This helps verify that WebClientMessageHandler is implemented with the
// correct parameter and return types.
type WebClientMessageHandlerInterface = {
  [Property in keyof WebClientRequestTypes]:
      // `payload` is the message payload.
  (payload: RequestRequestType<Property>, extras: ResponseExtras) =>
      Promisify<RequestResponseType<Property>>;
};

class WebClientMessageHandler implements WebClientMessageHandlerInterface {
  private cachedPinnedTabs: TabData[]|undefined = undefined;

  constructor(
      private webClient: GlicWebClient, private host: GlicBrowserHostImpl) {}

  async glicWebClientNotifyPanelWillOpen(payload: {
    panelOpeningData: PanelOpeningData,
  }): Promise<{openPanelInfo?: OpenPanelInfo}> {
    let openPanelInfo: OpenPanelInfo|undefined;
    try {
      const mergedArgument: PanelOpeningData&PanelState = Object.assign(
          {}, payload.panelOpeningData, payload.panelOpeningData.panelState);
      const result = await this.webClient.notifyPanelWillOpen?.(mergedArgument);
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
      await this.webClient.notifyPanelWasClosed?.();
    } catch (e) {
      console.warn(e);
    }
  }

  glicWebClientPanelStateChanged(payload: {panelState: PanelState}): void {
    this.host.getPanelState?.().assignAndSignal(payload.panelState);
  }

  glicWebClientRequestViewChange(payload: {request: ViewChangeRequest}): void {
    this.host.viewChangeRequestsSubject.next(payload.request);
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

  glicWebClientNotifyMicrophonePermissionStateChanged(payload: {
    enabled: boolean,
  }) {
    this.host.getMicrophonePermissionState().assignAndSignal(payload.enabled);
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

  glicWebClientNotifyPanelActiveChanged(payload: {panelActive: boolean}): void {
    this.host.panelActiveValue.assignAndSignal(payload.panelActive);
  }

  async glicWebClientCheckResponsive(): Promise<void> {
    return this.webClient.checkResponsive?.();
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

  glicWebClientNotifyActorTaskStateChanged(
      payload: {taskId: number, state: ActorTaskState}): void {
    this.host.setActorTaskState(payload.taskId, payload.state);
  }

  glicWebClientNotifyTabDataChanged(payload: {tabData: TabDataPrivate}): void {
    this.host.setTabData(payload.tabData);
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

  async glicWebClientRequestToShowDialog(payload: {
    request: SelectCredentialDialogRequestPrivate,
  }): Promise<{response: SelectCredentialDialogResponsePrivate}> {
    const request = payload.request;
    return new Promise(resolve => {
      if (!this.host.selectCredentialDialogRequestSubject
               .hasActiveSubscription()) {
        // Since there is no subscriber, respond to the browser immediately as
        // if no credential is selected.
        window.console.warn(
            'GlicWebClient: no subscriber for' +
            ' selectCredentialDialogRequest()!');
        resolve({
          response: {
            taskId: request.taskId,
            errorReason:
                SelectCredentialDialogErrorReason.DIALOG_PROMISE_NO_SUBSCRIBER,
          },
        });
        return;
      }
      const iconsGetter = new Map<string, () => Promise<Blob>>();
      for (const [id, image] of payload.request.icons.entries()) {
        let promise: Promise<Blob>|undefined;
        iconsGetter.set(id, () => {
          if (!promise) {
            promise = rgbaImageToBlob(image);
          }
          return promise;
        });
      }
      const credentials =
          request.credentials.map((credential: CredentialPrivate) => {
            const getIcon = iconsGetter.get(credential.sourceSiteOrApp);
            if (getIcon) {
              return {
                ...credential,
                getIcon,
              };
            }
            return credential;
          });
      const requestWithCallback: SelectCredentialDialogRequest = {
        ...request,
        credentials,
        onDialogClosed: resolve,
      };
      this.host.selectCredentialDialogRequestSubject.next(requestWithCallback);
    });
  }

  glicWebClientRequestToShowConfirmationDialog(payload: {
    request: UserConfirmationDialogRequestPrivate,
  }): Promise<{response: UserConfirmationDialogResponsePrivate}> {
    return new Promise(resolve => {
      if (!this.host.userConfirmationDialogRequestSubject
               .hasActiveSubscription()) {
        // Since there is no subscriber, respond to the browser immediately as
        // if the user denied the request.
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'userConfirmationDialogRequest()!');
        resolve({
          response: {
            permissionGranted: false,
            errorReason:
                ConfirmationRequestErrorReason.REQUEST_PROMISE_NO_SUBSCRIBER,
          },
        });
        return;
      }
      const requestWithCallback: UserConfirmationDialogRequest = {
        ...payload.request,
        onDialogClosed: resolve,
      };
      this.host.userConfirmationDialogRequestSubject.next(requestWithCallback);
    });
  }

  glicWebClientRequestToConfirmNavigation(payload: {
    request: NavigationConfirmationRequestPrivate,
  }): Promise<{response: NavigationConfirmationResponsePrivate}> {
    return new Promise(resolve => {
      if (!this.host.navigationConfirmationRequestSubject
               .hasActiveSubscription()) {
        // Since there is no subscriber, respond to the browser immediately as
        // if the user denied the request.
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'navigationConfirmationRequest()!');
        resolve({
          response: {
            errorReason:
                ConfirmationRequestErrorReason.REQUEST_PROMISE_NO_SUBSCRIBER,
          },
        });
        return;
      }
      const requestWithCallback: NavigationConfirmationRequest = {
        ...payload.request,
        onConfirmationDecision: resolve,
      };
      this.host.navigationConfirmationRequestSubject.next(requestWithCallback);
    });
  }

  glicWebClientNotifyAdditionalContext(payload: {
    context: AdditionalContextPrivate,
  }): void {
    const context = payload.context;
    const parts = context.parts.map(p => {
      const annotatedPageData = p.annotatedPageData &&
          convertAnnotatedPageDataFromPrivate(p.annotatedPageData);
      const pdf = p.pdf && convertPdfDocumentDataFromPrivate(p.pdf);
      const data = p.data && new Blob([p.data.data], {type: p.data.mimeType});
      const tabContext =
          p.tabContext && convertTabContextResultFromPrivate(p.tabContext);
      return {
        ...p,
        data,
        annotatedPageData,
        pdf,
        tabContext,
      };
    });
    this.host.additionalContextSubject.next({
      name: context.name,
      tabId: context.tabId,
      origin: context.origin,
      frameUrl: context.frameUrl,
      parts,
    });
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

  async glicWebClientRequestToShowAutofillSuggestionsDialog(payload: {
    request: SelectAutofillSuggestionsDialogRequestPrivate,
  }): Promise<{response: SelectAutofillSuggestionsDialogResponsePrivate}> {
    const request = payload.request;
    return new Promise(resolve => {
      if (!this.host.selectAutofillSuggestionsDialogRequestSubject
               .hasActiveSubscription()) {
        resolve({
          response: {
            taskId: request.taskId,
            errorReason: SelectAutofillSuggestionsDialogErrorReason
                             .DIALOG_PROMISE_NO_SUBSCRIBER,
            selectedSuggestions: [],
          },
        });
        return;
      }
      const requestWithCallback: SelectAutofillSuggestionsDialogRequest = {
        ...request,
        formFillingRequests: request.formFillingRequests.map(
            formFillingRequest => ({
              ...formFillingRequest,
              suggestions: formFillingRequest.suggestions.map(suggestion => {
                const icon = suggestion.icon;
                const getIcon = icon ? () => rgbaImageToBlob(icon) : undefined;
                return {...suggestion, getIcon};
              }),
            })),
        onDialogClosed: (result) => {
          const response: SelectAutofillSuggestionsDialogResponsePrivate = {
            ...result.response,
            taskId: request.taskId,
          };
          resolve({
            response: response,
          });
        },
      };
      this.host.selectAutofillSuggestionsDialogRequestSubject.next(
          requestWithCallback);
    });
  }
}

class GlicBrowserHostImpl implements GlicBrowserHost {
  private readonly hostId = newSenderId();
  private sender: PostMessageRequestSender;
  private receiver: PostMessageRequestReceiver;
  private handlerFunctionNames: Set<string> = new Set();
  private webClientMessageHandler: WebClientMessageHandler;
  private chromeVersion?: ChromeVersion;
  private panelState = ObservableValueImpl.withNoValue<PanelState>();
  canAttachPanelValue = ObservableValueImpl.withNoValue<boolean>();
  private focusedTabStateV2 = ObservableValueImpl.withNoValue<FocusedTabData>();
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
  panelActiveValue = ObservableValueImpl.withNoValue<boolean>();
  isBrowserOpenValue = ObservableValueImpl.withNoValue<boolean>();
  private journalHost: GlicBrowserHostJournalImpl;
  private metrics: GlicBrowserHostMetricsImpl;
  private manuallyResizing = ObservableValueImpl.withValue<boolean>(false);
  pinnedTabs = ObservableValueImpl.withNoValue<TabData[]>();
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
  private observedTabData = new Map<string, ObservableValueImpl<TabData>>();
  readonly viewChangeRequestsSubject = new Subject<ViewChangeRequest>();
  readonly additionalContextSubject = new Subject<AdditionalContext>();
  pageMetadataObservers: Map<string, ObservableValueImpl<PageMetadata>> =
      new Map();
  readonly selectCredentialDialogRequestSubject =
      new Subject<SelectCredentialDialogRequest>();
  readonly userConfirmationDialogRequestSubject =
      new Subject<UserConfirmationDialogRequest>();

  readonly navigationConfirmationRequestSubject =
      new Subject<NavigationConfirmationRequest>();
  actOnWebCapabilityValue = ObservableValueImpl.withNoValue<boolean>();

  readonly selectAutofillSuggestionsDialogRequestSubject =
      new Subject<SelectAutofillSuggestionsDialogRequest>();

  constructor(public webClient: GlicWebClient, windowProxy: WindowProxy) {
    // TODO(harringtond): Ideally, we could ensure we only process requests from
    // the single senderId used by the web client. This would avoid accidental
    // processing of requests from a previous client. This risk is very minimal,
    // as it would require reloading the webview page and initializing a new
    // web client very quickly, and in normal operation, the webview does not
    // reload after successful load.
    this.sender = new PostMessageRequestSender(
        windowProxy, 'chrome://glic', this.hostId, 'glic_api_client');
    this.receiver = new PostMessageRequestReceiver(
        'chrome://glic', this.hostId, windowProxy, this, 'glic_api_client');
    this.webClientMessageHandler =
        new WebClientMessageHandler(this.webClient, this);
    this.journalHost = new GlicBrowserHostJournalImpl(this.sender);
    this.metrics = new GlicBrowserHostMetricsImpl(this.sender);

    for (const name of Object.getOwnPropertyNames(
             WebClientMessageHandler.prototype)) {
      if (name !== 'constructor') {
        this.handlerFunctionNames.add(name);
      }
    }
  }

  destroy() {
    this.receiver.destroy();
  }

  async webClientCreated() {
    const response = await this.sender.requestWithResponse(
        'glicBrowserWebClientCreated', undefined);
    const state = response.initialState;
    this.receiver.setLoggingEnabled(state.loggingEnabled);
    this.sender.setLoggingEnabled(state.loggingEnabled);
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

    // Set the method to undefined since it's gated behind a mojo
    // RuntimeFeature. Calling a such a method when the feature is disabled
    // results in a mojo pipe closure.
    if (!this.hostCapabilities.has(
            HostCapability.GET_MODEL_QUALITY_CLIENT_ID)) {
      // MOJO_RUNTIME_FEATURE_GATED GetModelQualityClientId
      this.getModelQualityClientId = undefined;
    }

    if (!state.enableScrollTo) {
      this.scrollTo = undefined;
      this.dropScrollToHighlight = undefined;
    }

    if (!state.enableActInFocusedTab) {
      this.createTask = undefined;
      this.performActions = undefined;
      this.stopActorTask = undefined;
      this.pauseActorTask = undefined;
      this.resumeActorTask = undefined;
      this.interruptActorTask = undefined;
      this.uninterruptActorTask = undefined;
      this.getActOnWebCapability = undefined;
      this.createActorTab = undefined;
    }

    if (state.alwaysDetachedMode) {
      this.attachPanel = undefined;
      this.detachPanel = undefined;
      this.canAttachPanel = undefined;
      this.getPanelState = undefined;
    }

    if (!state.enableZeroStateSuggestions) {
      this.getZeroStateSuggestionsForFocusedTab = undefined;
      // MOJO_RUNTIME_FEATURE_GATED GetZeroStateSuggestionsAndSubscribe
      this.getZeroStateSuggestions = undefined;
    }

    if (!state.enableDefaultTabContextSettingFeature) {
      this.getDefaultTabContextPermissionState = undefined;
    }

    if (!state.enableClosedCaptioningFeature) {
      this.getClosedCaptioningSetting = undefined;
      this.setClosedCaptioningSetting = undefined;
      this.metrics.onClosedCaptionsShown = undefined;
    }

    if (!state.enableMaybeRefreshUserStatus) {
      this.maybeRefreshUserStatus = undefined;
    }

    if (!state.enableMultiTab) {
      // MOJO_RUNTIME_FEATURE_GATED GetContextFromTab
      this.getContextFromTab = undefined;
      this.getPinnedTabs = undefined;
      // MOJO_RUNTIME_FEATURE_GATED SubscribeToPinCandidates
      this.getPinCandidates = undefined;
      // MOJO_RUNTIME_FEATURE_GATED PinTabs
      this.pinTabs = undefined;
      // MOJO_RUNTIME_FEATURE_GATED SetMaximumNumberOfPinnedTabs
      this.setMaximumNumberOfPinnedTabs = undefined;
      // MOJO_RUNTIME_FEATURE_GATED UnpinTabs
      this.unpinTabs = undefined;
      // MOJO_RUNTIME_FEATURE_GATED UnpinAllTabs
      this.unpinAllTabs = undefined;
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

    if (!state.enableLoadAndExtractContent) {
      // TODO(crbug.com/458761731): Mark this as MOJO_RUNTIME_FEATURE_GATED once
      // `loadAndExtractContent` is defined in the handler interface.
      this.loadAndExtractContent = undefined;
    }
  }

  webClientInitialized(
      success: boolean, exception: TransferableException|undefined) {
    this.sender.requestNoResponse(
        'glicBrowserWebClientInitialized', {success, exception});
  }

  async handleRawRequest(type: string, payload: any, extras: ResponseExtras):
      Promise<{payload: any}|undefined> {
    if (!this.handlerFunctionNames.has(type)) {
      return;
    }
    const handlerFunction = (this.webClientMessageHandler as any)[type];
    const response = await handlerFunction.call(
        this.webClientMessageHandler, payload, extras);
    if (!response) {
      return;
    }
    return {payload: response};
  }

  setActorTaskState(taskId: number, state: ActorTaskState): void {
    this.getActorTaskState(taskId).assignAndSignal(state);

    if (state === ActorTaskState.STOPPED) {
      this.actorTaskState.delete(taskId);
    }
  }

  setTabData(tabData: TabDataPrivate): void {
    const data = convertTabDataFromPrivate(tabData);
    this.getTabById?.(data.tabId).assignAndSignal(data);
  }

  onRequestReceived(_type: string): void {}
  onRequestHandlerException(_type: string): void {}
  onRequestCompleted(_type: string): void {}

  // GlicBrowserHost implementation.

  getChromeVersion() {
    return Promise.resolve(this.chromeVersion!);
  }

  async createTab(url: string, options: CreateTabOptions): Promise<TabData> {
    const result =
        await this.sender.requestWithResponse('glicBrowserCreateTab', {
          url,
          options,
        });
    if (!result.tabData) {
      throw new Error('createTab: failed');
    }
    return convertTabDataFromPrivate(result.tabData);
  }

  openGlicSettingsPage(options?: OpenSettingsOptions): void {
    this.sender.requestNoResponse('glicBrowserOpenGlicSettingsPage', {options});
  }

  openPasswordManagerSettingsPage?(): void {
    this.sender.requestNoResponse(
        'glicBrowserOpenPasswordManagerSettingsPage', undefined);
  }

  closePanel(): Promise<void> {
    return this.sender.requestWithResponse('glicBrowserClosePanel', undefined);
  }

  closePanelAndShutdown(): void {
    this.sender.requestNoResponse(
        'glicBrowserClosePanelAndShutdown', undefined);
  }

  attachPanel?(): void {
    this.sender.requestNoResponse('glicBrowserAttachPanel', undefined);
  }

  detachPanel?(): void {
    this.sender.requestNoResponse('glicBrowserDetachPanel', undefined);
  }

  showProfilePicker(): void {
    this.sender.requestNoResponse('glicBrowserShowProfilePicker', undefined);
  }

  async getModelQualityClientId?(): Promise<string> {
    const result = await this.sender.requestWithResponse(
        'glicBrowserGetModelQualityClientId', undefined);
    return result.modelQualityClientId;
  }

  async switchConversation(info?: ConversationInfo): Promise<void> {
    if (info && !info.conversationId) {
      throw new Error('conversationId cannot be empty.');
    }
    await this.sender.requestWithResponse(
        'glicBrowserSwitchConversation', {info});
  }

  async registerConversation(info: ConversationInfo): Promise<void> {
    await this.sender.requestWithResponse(
        'glicBrowserRegisterConversation', {info});
  }

  async getContextFromFocusedTab(options: TabContextOptions):
      Promise<TabContextResult> {
    const context = await this.sender.requestWithResponse(
        'glicBrowserGetContextFromFocusedTab', {options});
    return convertTabContextResultFromPrivate(context.tabContextResult);
  }

  async setMaximumNumberOfPinnedTabs?(requestedMax: number): Promise<number> {
    const result = await this.sender.requestWithResponse(
        'glicBrowserSetMaximumNumberOfPinnedTabs', {requestedMax});
    return result.effectiveMax;
  }

  async getContextFromTab?
      (tabId: string, options: TabContextOptions): Promise<TabContextResult> {
    const result = await this.sender.requestWithResponse(
        'glicBrowserGetContextFromTab', {tabId, options});
    return convertTabContextResultFromPrivate(result.tabContextResult);
  }

  async getContextForActorFromTab?
      (tabId: string, options: TabContextOptions): Promise<TabContextResult> {
    const result = await this.sender.requestWithResponse(
        'glicBrowserGetContextForActorFromTab', {tabId, options});
    return convertTabContextResultFromPrivate(result.tabContextResult);
  }

  async createTask?(taskOptions?: TaskOptions): Promise<number> {
    const result = await this.sender.requestWithResponse(
        'glicBrowserCreateTask', {taskOptions});
    return result.taskId;
  }

  async performActions?(actions: ArrayBuffer): Promise<ArrayBuffer> {
    const result = await this.sender.requestWithResponse(
        'glicBrowserPerformActions', {actions});
    return result.actionsResult;
  }

  stopActorTask?(taskId?: number, stopReason?: ActorTaskStopReason): void {
    this.sender.requestNoResponse('glicBrowserStopActorTask', {
      taskId: taskId ?? 0,
      stopReason: stopReason ?? ActorTaskStopReason.TASK_COMPLETE,
    });
  }

  pauseActorTask?
      (taskId: number, pauseReason?: ActorTaskPauseReason, tabId?: string):
          void {
    this.sender.requestNoResponse('glicBrowserPauseActorTask', {
      taskId,
      pauseReason: pauseReason ?? ActorTaskPauseReason.PAUSED_BY_MODEL,
      tabId: tabId ?? '',
    });
  }

  async resumeActorTask?(taskId: number, tabContextOptions: TabContextOptions):
      Promise<ResumeActorTaskResult> {
    const response = await this.sender.requestWithResponse(
        'glicBrowserResumeActorTask', {taskId, tabContextOptions});
    return convertTabContextResultFromPrivate(response.resumeActorTaskResult);
  }

  interruptActorTask?(taskId: number): void {
    this.sender.requestNoResponse('glicBrowserInterruptActorTask', {
      taskId,
    });
  }

  uninterruptActorTask?(taskId: number): void {
    this.sender.requestNoResponse('glicBrowserUninterruptActorTask', {
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
    const result = await this.sender.requestWithResponse(
        'glicBrowserCreateActorTab', {taskId, options});
    if (!result.tabData) {
      throw new Error('createActorTab: failed');
    }
    return convertTabDataFromPrivate(result.tabData);
  }

  getTabById?(tabId: string): ObservableValueImpl<TabData> {
    const tabObs = this.observedTabData.get(tabId);
    if (tabObs) {
      return tabObs;
    }

    // TODO(mcnee): We don't communicate to the browser that we want to start
    // observing this tab. This is done implicitly by it being a tab associated
    // with an actor task. We need to properly notify the browser for this API
    // to actually be generic.
    // TODO(mcnee): Handle the closing of an observed tab.
    // TODO(mcnee): The client could pass an id that will never have updates.
    // Consider removing these cases from the map when all subscribers are
    // removed.
    const newObs = ObservableValueImpl.withNoValue<TabData>();
    this.observedTabData.set(tabId, newObs);
    return newObs;
  }

  activateTab?(tabId: string): void {
    this.sender.requestNoResponse('glicBrowserActivateTab', {tabId});
  }

  onModeChange?(newMode: WebClientMode): void {
    this.sender.requestNoResponse('glicBrowserOnModeChange', {newMode});
  }

  async resizeWindow(
      width: number, height: number,
      options?: ResizeWindowOptions): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserResizeWindow', {size: {width, height}, options});
  }

  enableDragResize?(enabled: boolean): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserEnableDragResize', {enabled});
  }

  async captureScreenshot(): Promise<Screenshot> {
    const screenshotResult = await this.sender.requestWithResponse(
        'glicBrowserCaptureScreenshot', undefined);
    return screenshotResult.screenshot;
  }

  captureRegion?(): ObservableValue<CaptureRegionResult> {
    if (this.captureRegionObservable) {
      this.captureRegionObservable.complete();
    }
    this.captureRegionObservable =
        new CaptureRegionObservable(this.idGenerator.next(), this.sender);
    return this.captureRegionObservable;
  }

  setWindowDraggableAreas(areas: DraggableArea[]): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetWindowDraggableAreas', {areas});
  }

  setMinimumWidgetSize(width: number, height: number): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetMinimumWidgetSize', {size: {width, height}});
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
    return this.sender.requestWithResponse(
        'glicBrowserSetMicrophonePermissionState', {enabled});
  }

  setLocationPermissionState(enabled: boolean): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetLocationPermissionState', {enabled});
  }

  setTabContextPermissionState(enabled: boolean): Promise<void> {
    if (this.enableDefaultTabContextSettingFeature) {
      this.permissionStateTabContext.assignAndSignal(enabled);
      return Promise.resolve();
    }
    return this.sender.requestWithResponse(
        'glicBrowserSetTabContextPermissionState', {enabled});
  }

  setClosedCaptioningSetting?(enabled: boolean): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetClosedCaptioningSetting', {enabled});
  }

  setContextAccessIndicator(show: boolean): void {
    this.sender.requestWithResponse(
        'glicBrowserSetContextAccessIndicator', {show});
  }

  setActuationOnWebSetting?(enabled: boolean): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetActuationOnWebSetting', {enabled});
  }

  async getUserProfileInfo?(): Promise<UserProfileInfo> {
    const {profileInfo} = await this.sender.requestWithResponse(
        'glicBrowserGetUserProfileInfo', undefined);
    if (!profileInfo) {
      throw new Error('getUserProfileInfo failed');
    }
    const {avatarIcon} = profileInfo;
    return replaceProperties(
        profileInfo,
        {avatarIcon: async () => avatarIcon && rgbaImageToBlob(avatarIcon)});
  }

  async refreshSignInCookies(): Promise<void> {
    const result = await this.sender.requestWithResponse(
        'glicBrowserRefreshSignInCookies', undefined);
    if (!result.success) {
      throw Error('refreshSignInCookies failed');
    }
  }

  setAudioDucking?(enabled: boolean): void {
    this.sender.requestNoResponse('glicBrowserSetAudioDucking', {enabled});
  }

  getJournalHost(): GlicBrowserHostJournal {
    return this.journalHost;
  }

  getMetrics(): GlicBrowserHostMetrics {
    return this.metrics;
  }

  scrollTo?(params: ScrollToParams): Promise<void> {
    return this.sender.requestWithResponse('glicBrowserScrollTo', {params});
  }

  setSyntheticExperimentState(trialName: string, groupName: string): void {
    this.sender.requestNoResponse(
        'glicBrowserSetSyntheticExperimentState', {trialName, groupName});
  }

  openOsPermissionSettingsMenu?(permission: string): void {
    this.sender.requestNoResponse(
        'glicBrowserOpenOsPermissionSettingsMenu', {permission});
  }

  async getOsMicrophonePermissionStatus(): Promise<boolean> {
    return (await this.sender.requestWithResponse(
                'glicBrowserGetOsMicrophonePermissionStatus', undefined))
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

  async pinTabs?(tabIds: string[]): Promise<boolean> {
    return (await this.sender.requestWithResponse(
                'glicBrowserPinTabs', {tabIds}))
        .pinnedAll;
  }

  async unpinTabs?(tabIds: string[]): Promise<boolean> {
    return (await this.sender.requestWithResponse(
                'glicBrowserUnpinTabs', {tabIds}))
        .unpinnedAll;
  }

  unpinAllTabs?(): void {
    this.sender.requestNoResponse('glicBrowserUnpinAllTabs', undefined);
  }

  getPinCandidates?
      (options: GetPinCandidatesOptions): ObservableValue<PinCandidate[]> {
    this.pinCandidates?.setObsolete();
    return this.pinCandidates = new PinCandidatesObservable(
               this.idGenerator.next(), this.sender, options);
  }

  async getZeroStateSuggestionsForFocusedTab?
      (isFirstRun?: boolean): Promise<ZeroStateSuggestions> {
    const zeroStateResult = await this.sender.requestWithResponse(
        'glicBrowserGetZeroStateSuggestionsForFocusedTab', {isFirstRun});
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
    const zeroStateResult = await this.sender.requestWithResponse(
        'glicBrowserGetZeroStateSuggestionsAndSubscribe', {
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
    this.sender.requestNoResponse(
        'glicBrowserDropScrollToHighlight', undefined);
  }

  maybeRefreshUserStatus?(): void {
    this.sender.requestNoResponse(
        'glicBrowserMaybeRefreshUserStatus', undefined);
  }

  getAdditionalContext?(): Observable<AdditionalContext> {
    return this.additionalContextSubject;
  }

  getHostCapabilities(): Set<HostCapability> {
    return this.hostCapabilities;
  }

  getViewChangeRequests(): Observable<ViewChangeRequest> {
    return this.viewChangeRequestsSubject;
  }

  onViewChanged(notification: ViewChangedNotification) {
    this.sender.requestNoResponse('glicBrowserOnViewChanged', {notification});
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
          const {success} = await this.sender.requestWithResponse(
              'glicBrowserSubscribeToPageMetadata',
              {tabId, names: isActive ? names : []});
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

  async loadAndExtractContent?(urls: string[], options: TabContextOptions[]):
      Promise<TabContextResult[]> {
    const response = await this.sender.requestWithResponse(
        'glicBrowserLoadAndExtractContent', {urls, options});

    return response.results.map(convertTabContextResultFromPrivate);
  }
}

class GlicBrowserHostJournalImpl implements GlicBrowserHostJournal {
  constructor(private sender: PostMessageRequestSender) {}

  beginAsyncEvent(
      asyncEventId: number, taskId: number, event: string,
      details: string): void {
    this.sender.requestNoResponse(
        'glicBrowserLogBeginAsyncEvent',
        {asyncEventId, taskId, event, details});
  }

  clear(): void {
    this.sender.requestNoResponse('glicBrowserJournalClear', undefined);
  }

  endAsyncEvent(asyncEventId: number, details: string): void {
    this.sender.requestNoResponse(
        'glicBrowserLogEndAsyncEvent', {asyncEventId, details});
  }

  instantEvent(taskId: number, event: string, details: string): void {
    this.sender.requestNoResponse(
        'glicBrowserLogInstantEvent', {taskId, event, details});
  }

  async snapshot(clear: boolean): Promise<Journal> {
    const snapshotResult = await this.sender.requestWithResponse(
        'glicBrowserJournalSnapshot', {clear});
    return snapshotResult.journal;
  }

  start(maxBytes: number, captureScreenshots: boolean): void {
    this.sender.requestNoResponse(
        'glicBrowserJournalStart', {maxBytes, captureScreenshots});
  }

  stop(): void {
    this.sender.requestNoResponse('glicBrowserJournalStop', undefined);
  }

  recordFeedback(positive: boolean, reason: string) {
    this.sender.requestNoResponse(
        'glicBrowserJournalRecordFeedback',
        {positive, reason},
    );
  }
}

class GlicBrowserHostMetricsImpl implements GlicBrowserHostMetrics {
  constructor(private sender: PostMessageRequestSender) {}

  onUserInputSubmitted(mode: number): void {
    this.sender.requestNoResponse('glicBrowserOnUserInputSubmitted', {mode});
  }

  onReaction(reactionType: number): void {
    this.sender.requestNoResponse('glicBrowserOnReaction', {reactionType});
  }

  onContextUploadStarted(): void {
    this.sender.requestNoResponse(
        'glicBrowserOnContextUploadStarted', undefined);
  }

  onContextUploadCompleted(): void {
    this.sender.requestNoResponse(
        'glicBrowserOnContextUploadCompleted', undefined);
  }

  onResponseStarted(): void {
    this.sender.requestNoResponse('glicBrowserOnResponseStarted', undefined);
  }

  onResponseStopped(details?: OnResponseStoppedDetails): void {
    this.sender.requestNoResponse('glicBrowserOnResponseStopped', {details});
  }

  onSessionTerminated(): void {
    this.sender.requestNoResponse('glicBrowserOnSessionTerminated', undefined);
  }

  onResponseRated(positive: boolean): void {
    this.sender.requestNoResponse('glicBrowserOnResponseRated', {positive});
  }

  onClosedCaptionsShown?(): void {
    this.sender.requestNoResponse(
        'glicBrowserOnClosedCaptionsShown', undefined);
  }

  onTurnCompleted?(model: number, duration: number): void {
    this.sender.requestNoResponse(
        'glicBrowserOnTurnCompleted', {model, duration});
  }

  onModelChanged?(model: number): void {
    this.sender.requestNoResponse('glicBrowserOnModelChanged', {model});
  }

  onRecordUseCounter?(counter: number): void {
    this.sender.requestNoResponse('glicBrowserOnRecordUseCounter', {counter});
  }
}

class IdGenerator {
  private nextId = 1;

  next(): number {
    return this.nextId++;
  }
}

class CaptureRegionObservable extends ObservableValueImpl<CaptureRegionResult> {
  observationId: number;

  constructor(observationId: number, private sender: PostMessageRequestSender) {
    super(false);
    this.observationId = observationId;
  }

  override activeSubscriptionChanged(hasActiveSubscription: boolean): void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    if (this.isStopped()) {
      return;
    }
    if (hasActiveSubscription) {
      this.sender.requestNoResponse(
          'glicBrowserSubscribeToCaptureRegion',
          {observationId: this.observationId});
    } else {
      this.sender.requestNoResponse(
          'glicBrowserUnsubscribeFromCaptureRegion',
          {observationId: this.observationId});
      // Unsubscribing from the client side is a terminal event.
      this.complete();
    }
  }

  override error(e: any) {
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
      private sender: PostMessageRequestSender,
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
          'glicBrowserSubscribeToPinCandidates',
          {options: this.options, observationId: this.observationId});
    } else {
      this.sender.requestNoResponse(
          'glicBrowserUnsubscribeFromPinCandidates',
          {observationId: this.observationId});
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

// Converts an RgbaImage into a Blob through the canvas API. Output is a PNG.
async function rgbaImageToBlob(image: RgbaImage): Promise<Blob> {
  const canvas = document.createElement('canvas');
  canvas.width = image.width;
  canvas.height = image.height;
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    throw Error('getContext error');
  }
  if (image.colorType !== ImageColorType.BGRA) {
    throw Error('unsupported colorType');
  }
  // Note that for either alphaType, we swap bytes from BGRA to RGBA order.
  const pixelData = new Uint8ClampedArray(image.dataRGBA);
  if (image.alphaType === ImageAlphaType.PREMUL) {
    for (let i = 0; i + 3 < pixelData.length; i += 4) {
      const alphaInt = pixelData[i + 3]!;
      if (alphaInt === 0) {
        // Don't divide by zero. In this case, RGB should already be zero, so
        // there's no purpose in swapping bytes.
        continue;
      }
      const alpha = alphaInt / 255.0;
      const [B, G, R] = [pixelData[i]!, pixelData[i + 1]!, pixelData[i + 2]!];
      pixelData[i] = R / alpha;
      pixelData[i + 1] = G / alpha;
      pixelData[i + 2] = B / alpha;
    }
  } else {
    for (let i = 0; i + 3 < pixelData.length; i += 4) {
      const [B, R] = [pixelData[i]!, pixelData[i + 2]!];
      pixelData[i] = R;
      pixelData[i + 2] = B;
    }
  }

  ctx.putImageData(new ImageData(pixelData, image.width, image.height), 0, 0);
  return new Promise((resolve) => {
    canvas.toBlob((result) => {
      if (!result) {
        throw Error('toBlob failed');
      }
      resolve(result);
    });
  });
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
      faviconResult = rgbaImageToBlob(dataFavicon);
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
