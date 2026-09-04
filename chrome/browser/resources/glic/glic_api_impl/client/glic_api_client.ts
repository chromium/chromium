// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageMetadata as PageMetadataMojo} from '../../ai_page_content_metadata.mojom-webui.js';
import {ContentSettingsType} from '../../content_settings_types.mojom-webui.js';
import {enumFromClient, enumToClient} from '../../enum_conversions.js';
import {PinCandidatesObserverReceiver, SettingsPageField as SettingsPageFieldMojo, WebClientReceiver} from '../../glic.mojom-webui.js';
import type {AdditionalContext as AdditionalContextMojo, FileUploadPolicyState as FileUploadPolicyStateMojo, FocusedTabData as FocusedTabDataMojo, GeminiEnterpriseSettings as GeminiEnterpriseSettingsMojo, InvokeOptions as InvokeOptionsMojo, OpenPanelInfo as OpenPanelInfoMojo, PanelOpeningData as PanelOpeningDataMojo, PanelState as PanelStateMojo, PinCandidate as PinCandidateMojo, PinCandidatesObserverInterface, TabData as TabDataMojo, WebClientHandlerRemote, WebClientInterface} from '../../glic.mojom-webui.js';
import {CaptureRegionErrorReason, ClientCapabilities, HostCapability} from '../../glic_api/glic_api.js';
import type {ActivateTabOptions, AdditionalContext, AnnotatedPageData, CaptureRegionParams, CaptureRegionResult, ChromeVersion, ClientErrorDialogType, ConversationInfo, CounterAbuseVerdict, CreateTabOptions, FileUploadPolicyState, FocusedTabData, FormFactor, GeminiEnterpriseSettings, GetPinCandidatesOptions, GlicBrowserHost, GlicBrowserHostMetrics, GlicHostRegistry, GlicWebClient, ImageBytesResult, ImageInfo, InvokeOptions, MicrophoneStatus, Observable, ObservableValue, OnResponseStoppedDetails, OpenPanelInfo, OpenPinnedTabPickerOptions, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, PinCandidate, PinTabsOptions, Platform, PromptType, ResizeWindowOptions, ResumeActorTaskResult, Screenshot, TabContextOptions, TabContextResult, TabData, UnpinTabsOptions, UserProfileInfo, WebClientMode, ZeroStateSuggestions} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl, Subject} from '../../observable.js';
import {GlicBrowserHostActor} from '../actor/actor_client.js';
import {GlicBrowserHostAnnotation} from '../annotation/annotation_client.js';
import {GlicBrowserHostExperimentalTriggering} from '../experimental_triggering/experimental_triggering_client.js';
import {additionalContextToClient, conversionSettings, createTabOptionsFromClient, fileUploadPolicyStateToClient, focusedTabDataToClient, getPinCandidatesOptionsFromClient, idFromClient, idToClient, invokeOptionsToClient, pageMetadataToClient, panelOpeningDataToClient, panelStateToClient, pinCandidateToClient, tabDataToClient, timeDeltaFromClient, urlFromClient, webClientModeToMojo} from '../host/conversions.js';
import type {GlicApiHost} from '../host/glic_api_host.js';
import {PanelOpenState} from '../host/types.js';
import {GlicBrowserHostSkills} from '../skills/skills_client.js';
import {GlicBrowserHostTools} from '../tools/tools_client.js';
import {assertNever, ResponseExtras} from '../transport/messaging.js';
import type {createDirectMessagingPair, PendingRemote, PostMessageHandler, PostMessageReceiver, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';
import {GlicBrowserHostZeroStateSuggestions} from '../zero_state_suggestions/zero_state_suggestions_client.js';

import {replaceProperties} from './../conversions.js';
import {ErrorWithReasonImpl, newTransferableException, WebClientRegionCaptureDef, WebClientTabDataObserverDef, WebClientTabFaviconObserverDef} from './../request_types.js';
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
      private hostApi: GlicApiHost,
  ) {}

  async registerWebClient(webClient: GlicWebClient): Promise<void> {
    this.host =
        new GlicBrowserHostImpl(webClient, this.directPair, this.hostApi);
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
                                            GlicBrowserHost,
                                            WebClientInterface {
  readonly router: PostMessageRouter;
  readonly clientRemote: PostMessageRemote<WebClientHost>;

  readonly actorClient: GlicBrowserHostActor;
  readonly annotationClient: GlicBrowserHostAnnotation;
  readonly skillsClient: GlicBrowserHostSkills;
  readonly toolsClient: GlicBrowserHostTools;
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
  private zoomLevel = ObservableValueImpl.withNoValue<number>();
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
  private cachedPinnedTabs: TabData[]|undefined = undefined;
  private webClientReceiver?: WebClientReceiver;

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

  private handler: WebClientHandlerRemote;

  constructor(
      public webClient: GlicWebClient,
      directPair: ReturnType<
          typeof createDirectMessagingPair<WebClientHost, WebClient>>,
      private hostApi: GlicApiHost,
  ) {
    this.handler = hostApi.handler;
    this.router = directPair.client.router;
    this.clientRemote = directPair.client.rootRemote;

    this.actorClient = new GlicBrowserHostActor(this);
    this.annotationClient = new GlicBrowserHostAnnotation(this);
    this.skillsClient = new GlicBrowserHostSkills();
    this.suggestionsClient = new GlicBrowserHostZeroStateSuggestions(this);
    this.toolsClient = new GlicBrowserHostTools();

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
      this.toolsClient,
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

  getHandler(): WebClientHandlerRemote {
    return this.handler;
  }

  destroy() {
    this.pinCandidates?.setObsolete();
    this.skillsClient.destroySkills();
    this.toolsClient.destroyTools();
    if (this.webClientReceiver) {
      this.webClientReceiver.$.close();
      this.webClientReceiver = undefined;
    }
    this.router.destroy();
  }

  async webClientCreated(clientCapabilities: Set<ClientCapabilities>) {
    conversionSettings.omitFaviconInTabData =
        clientCapabilities.has(ClientCapabilities.IGNORES_TAB_DATA_FAVICONS);
    this.webClientReceiver = new WebClientReceiver(this);
    const {initialState} = await this.handler.webClientCreated(
        this.webClientReceiver.$.bindNewPipeAndPassRemote());
    const initialPipes =
        this.hostApi.setInitialState(initialState, clientCapabilities);
    this.actorClient.initialize(
        initialState, initialPipes.actorRemote, initialPipes.actorReceiver);
    this.annotationClient.initialize(initialState);
    this.skillsClient.initialize(initialState, this.handler);
    this.experimentalTriggeringClient.initialize(
        this.router, initialPipes.experimentalTriggeringReceiver,
        this.webClient, this.clientRemote);
    this.suggestionsClient.initialize(
        initialState, initialPipes.zeroStateSuggestionsRemote);
    this.toolsClient.initialize(initialState, this.handler);

    const state = initialState;
    this.geminiEnterpriseSettings.assignAndSignal(
        state.geminiEnterpriseSettings ?? undefined);
    this.zoomLevel.assignAndSignal(state.zoomFactor);
    this.panelState.assignAndSignal(panelStateToClient(state.panelState));
    const extras = new ResponseExtras();
    const focusedTabDataPrivate =
        focusedTabDataToClient(state.focusedTabData, extras);
    this.focusedTabStateV2.assignAndSignal(
        convertFocusedTabDataFromPrivate(focusedTabDataPrivate));
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
    const chromeVersion = state.chromeVersion.components;
    this.chromeVersion = {
      major: chromeVersion[0] || 0,
      minor: chromeVersion[1] || 0,
      build: chromeVersion[2] || 0,
      patch: chromeVersion[3] || 0,
    };
    this.platform = enumToClient(state.platform);
    this.formFactor = enumToClient(state.formFactor);
    this.enableCachedGetUserProfileInfo = state.enableCachedGetUserProfileInfo;
    this.panelActiveValue.assignAndSignal(state.panelIsActive);
    this.isBrowserOpenValue.assignAndSignal(state.browserIsOpen);
    this.osHotkeyState.assignAndSignal({hotkey: state.hotkey});
    this.closedCaptioningState.assignAndSignal(
        state.closedCaptioningSettingEnabled);
    this.actuationOnWebState.assignAndSignal(
        state.actuationOnWebSettingEnabled);
    this.fileUploadAllowedState.assignAndSignal(
        fileUploadPolicyStateToClient(state.fileUploadPolicyState));
    for (const capability of state.hostCapabilities) {
      this.hostCapabilities.add(enumToClient(capability));
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

    if (!state.enableOpenContactInfoSettingsPage) {
      this.openContactInfoSettingsPage = undefined;
    }

    if (!state.enableGetTabFaviconById) {
      this.getTabFaviconById = undefined;
    }

    if (!state.enableProcessCounterAbuseVerdict) {
      this.processCounterAbuseVerdict = undefined;
    }
  }

  webClientInitialized(success: boolean, exception: GlicException|undefined) {
    if (success) {
      this.handler.webClientInitialized();
    } else {
      this.handler.webClientInitializeFailed();
    }
    this.clientRemote.requestNoResponse(
        'webClientInitialized', {success, exception});
  }

  // WebClientInterface implementation.

  async checkResponsive(): Promise<void> {
    await this.webClient.checkResponsive?.();
  }

  async notifyPanelWillOpen(panelOpeningData: PanelOpeningDataMojo):
      Promise<{openPanelInfo: OpenPanelInfoMojo}> {
    let openPanelInfo: OpenPanelInfo|undefined;
    const clientOpeningData = panelOpeningDataToClient(panelOpeningData);
    try {
      const mergedArgument: PanelOpeningData&PanelState =
          Object.assign({}, clientOpeningData, clientOpeningData.panelState);
      const completedPromise = this.notifyPanelWillOpenCompleted;
      const result = await this.webClient.notifyPanelWillOpen?.(mergedArgument);
      completedPromise.resolve();

      if (result) {
        openPanelInfo = result;
      }
    } catch (e) {
      console.warn(e);
    } finally {
      this.panelOpenStateChanged(PanelOpenState.OPEN);
    }

    const openPanelInfoMojo: OpenPanelInfoMojo = {
      webClientMode: webClientModeToMojo(openPanelInfo?.startingMode),
      panelSize: null,
      resizeDuration:
          timeDeltaFromClient(openPanelInfo?.resizeParams?.options?.durationMs),
      canUserResize: openPanelInfo?.canUserResize ?? true,
    };
    if (openPanelInfo?.resizeParams) {
      const size = {
        width: openPanelInfo?.resizeParams?.width,
        height: openPanelInfo?.resizeParams?.height,
      };
      openPanelInfoMojo.panelSize = size;
    }
    return {openPanelInfo: openPanelInfoMojo};
  }

  async notifyPanelWasClosed(): Promise<void> {
    this.panelOpenStateChanged(PanelOpenState.CLOSED);
    try {
      this.notifyPanelWillOpenCompleted = Promise.withResolvers<void>();
      await this.webClient.notifyPanelWasClosed?.();
    } catch (e) {
      console.warn(e);
    }
  }

  async invoke(options: InvokeOptionsMojo): Promise<void> {
    try {
      const extras = new ResponseExtras();
      const clientOptions = convertInvokeOptionsFromPrivate(
          invokeOptionsToClient(options, extras));
      // Wait until notifyPanelWillOpen has resolved before invoking.
      await this.notifyPanelWillOpenCompleted.promise;
      await this.webClient.invoke?.(clientOptions);
    } catch (e) {
      console.warn(e);
    }
  }

  notifyPanelStateChange(panelState: PanelStateMojo): void {
    this.panelState.assignAndSignal(panelStateToClient(panelState));
  }

  notifyPanelActiveChange(panelActive: boolean): void {
    this.panelActiveValue.assignAndSignal(panelActive);
  }

  notifyPanelCanAttachChange(canAttach: boolean): void {
    this.canAttachPanelValue.assignAndSignal(canAttach);
  }

  notifyGeminiEnterpriseSettingsChanged(
      settings: GeminiEnterpriseSettingsMojo|null): void {
    this.geminiEnterpriseSettings.assignAndSignal(settings || undefined);
  }

  notifyMicrophonePermissionStateChanged(enabled: boolean): void {
    this.permissionStateMicrophone.assignAndSignal(enabled);
  }

  async stopMicrophone(): Promise<void> {
    await this.webClient.stopMicrophone?.();
  }

  notifyLocationPermissionStateChanged(enabled: boolean): void {
    this.permissionStateLocation.assignAndSignal(enabled);
  }

  notifyTabContextPermissionStateChanged(enabled: boolean): void {
    this.permissionStateTabContext.assignAndSignal(enabled);
  }

  notifyOsLocationPermissionStateChanged(enabled: boolean): void {
    this.permissionStateOsLocation.assignAndSignal(enabled);
  }

  notifyClosedCaptioningSettingChanged(enabled: boolean): void {
    this.closedCaptioningState.assignAndSignal(enabled);
  }

  notifyDefaultTabContextPermissionStateChanged(enabled: boolean): void {
    this.defaultTabContextPermission.assignAndSignal(enabled);
  }

  notifyActuationOnWebSettingChanged(enabled: boolean): void {
    this.actuationOnWebState.assignAndSignal(enabled);
  }

  notifyFileUploadStateChanged(state: FileUploadPolicyStateMojo): void {
    this.fileUploadAllowedState.assignAndSignal(
        fileUploadPolicyStateToClient(state));
  }

  notifyZoomLevelChanged(zoomFactor: number): void {
    this.zoomLevel.assignAndSignal(zoomFactor);
  }

  notifyFocusedTabChanged(focusedTabData: FocusedTabDataMojo): void {
    const extras = new ResponseExtras();
    const focusedTabDataPrivate =
        focusedTabDataToClient(focusedTabData, extras);
    this.focusedTabStateV2.assignAndSignal(
        convertFocusedTabDataFromPrivate(focusedTabDataPrivate));
  }

  notifyManualResizeChanged(resizing: boolean): void {
    this.manuallyResizing.assignAndSignal(resizing);
  }

  notifyBrowserIsOpenChanged(browserIsOpen: boolean): void {
    this.isBrowserOpenValue.assignAndSignal(browserIsOpen);
  }

  notifyInstanceActivationChanged(instanceIsActive: boolean): void {
    this.hostApi.setInstanceIsActive(instanceIsActive);
  }

  notifyOsHotkeyStateChanged(hotkey: string): void {
    this.osHotkeyState.assignAndSignal({hotkey});
  }

  notifyPinnedTabsChanged(tabData: TabDataMojo[]): void {
    this.cachedPinnedTabs = tabData.map((x) => tabDataToClient(x));
    this.pinnedTabs.assignAndSignal(this.cachedPinnedTabs);
  }

  notifyPinnedTabDataChanged(tabData: TabDataMojo): void {
    if (!this.cachedPinnedTabs) {
      return;
    }
    const convertedTab = tabDataToClient(tabData);
    this.cachedPinnedTabs = this.cachedPinnedTabs.map((cachedTab) => {
      if (cachedTab.tabId === convertedTab.tabId) {
        return convertedTab;
      }
      return cachedTab;
    });
    this.pinnedTabs.assignAndSignal(this.cachedPinnedTabs);
  }

  notifyPageMetadataChanged(tabId: number, metadata: PageMetadataMojo|null):
      void {
    const tabIdStr = idToClient(tabId);
    const observable = this.pageMetadataObservers.get(tabIdStr);
    if (!observable) {
      return;
    }

    const clientMetadata = pageMetadataToClient(metadata);
    if (clientMetadata) {
      observable.assignAndSignal(clientMetadata);
    } else {
      if (!observable.isStopped()) {
        observable.complete();
      }
      this.pageMetadataObservers.delete(tabIdStr);
    }
  }

  notifyAdditionalContext(context: AdditionalContextMojo): void {
    const extras = new ResponseExtras();
    const clientContext = convertAdditionalContextFromPrivate(
        additionalContextToClient(context, extras));
    this.additionalContextSubject.next(clientContext);
  }

  notifyActOnWebCapabilityChanged(canActOnWeb: boolean): void {
    this.actorClient.actOnWebCapabilityValue.assignAndSignal(canActOnWeb);
  }

  notifyOnboardingCompletedChanged(completed: boolean): void {
    this.onboardingCompleted.assignAndSignal(completed);
  }

  notifyActorTaskListRowClicked(taskId: number): void {
    this.actorClient.actorTaskListRowClickedSubject.next(taskId);
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

  async createTab(url: string, options: CreateTabOptions = {}):
      Promise<TabData> {
    const response = await this.handler.createTab(
        urlFromClient(url), createTabOptionsFromClient(options));
    if (!response.tabData) {
      throw new Error('createTab: failed');
    }
    return tabDataToClient(response.tabData);
  }

  async activateTabWithUrl(exactUrl: string, options: ActivateTabOptions = {}):
      Promise<TabData> {
    const response =
        await this.handler.activateTabWithUrl(urlFromClient(exactUrl), {
          pattern: options.pattern ?? '',
          fallbackWindowId: idFromClient(options.fallbackWindowId),
        });
    if (!response.tabData) {
      throw new Error('activateTabWithUrl: failed');
    }
    return tabDataToClient(response.tabData);
  }

  openGlicSettingsPage(options?: OpenSettingsOptions): void {
    this.handler.openGlicSettingsPage({
      highlightField: enumFromClient(options?.highlightField) ??
          SettingsPageFieldMojo.kNone,
    });
  }

  openPasswordManagerSettingsPage?(): void {
    this.handler.openPasswordManagerSettingsPage();
  }

  openContactInfoSettingsPage?(): void {
    this.handler.openContactInfoSettingsPage();
  }

  reportClientTransientError(abslStatus: number): void {
    this.clientRemote.requestNoResponse(
        'reportClientTransientError', {abslStatus});
  }

  processCounterAbuseVerdict?
      (tabId: string, verdict: CounterAbuseVerdict): void {
    this.clientRemote.requestNoResponse(
        'processCounterAbuseVerdict', {tabId, verdict});
  }

  async closePanel(): Promise<void> {
    this.handler.closePanel();
  }

  closePanelAndShutdown(): void {
    this.handler.closePanelAndShutdown();
  }

  attachPanel?(): void {
    this.handler.attachPanel();
  }

  detachPanel?(): void {
    if (this.hostCapabilities.has(HostCapability.NO_LIVE_MODE)) {
      throw new Error('NO_LIVE_MODE: detachPanel not supported');
    }
    this.handler.detachPanel();
  }

  showProfilePicker(): void {
    this.handler.showProfilePicker();
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
    // Warning: calling openOsPermissionSettingsMenu with unsupported content
    // setting type will terminate the render process (bad mojo message).
    // Update GlicWebClientHandler:OpenOsPermissionSettingsMenu with any new
    // types.
    switch (permission) {
      case 'media':
        this.handler.openOsPermissionSettingsMenu(
            ContentSettingsType.MEDIASTREAM_MIC);
        break;
      case 'geolocation':
        this.handler.openOsPermissionSettingsMenu(
            ContentSettingsType.GEOLOCATION);
        break;
      default:
        this.handler.openOsPermissionSettingsMenu(ContentSettingsType.COOKIES);
        break;
    }
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
    this.observable.assignAndSignal(
        payload.favicon ? rgbaImageToBlob(payload.favicon) : undefined);
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
