// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import type {BitmapN32} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';

import type {PageMetadata as PageMetadataMojo} from '../ai_page_content_metadata.mojom-webui.js';
import {ContentSettingsType} from '../content_settings_types.mojom-webui.js';
import {enumFromClient, enumToClient} from '../enum_conversions.js';
import {                                       //
  CaptureRegionObserverReceiver,               //
  PinCandidatesObserverReceiver,               //
  PromptType as PromptTypeMojo,                //
  ResponseStopCause as ResponseStopCauseMojo,  //
  SettingsPageField as SettingsPageFieldMojo,  //
  TabDataHandlerReceiver,                      //
  TabFaviconHandlerReceiver,                   //
  WebClientHandlerRemote,                      //
  WebClientReceiver,                           //
} from '../glic.mojom-webui.js';
import type {                                                          //
             AdditionalContext as AdditionalContextMojo,               //
             CaptureRegionErrorReason as CaptureRegionErrorReasonMojo, //
             CaptureRegionObserverInterface,                           //
             CaptureRegionResult as CaptureRegionResultMojo,           //
             FileUploadPolicyState as FileUploadPolicyStateMojo,       //
             FocusedTabData as FocusedTabDataMojo,                     //
             GeminiEnterpriseSettings as GeminiEnterpriseSettingsMojo, //
             InvokeOptions as InvokeOptionsMojo,                       //
             OpenPanelInfo as OpenPanelInfoMojo,                       //
             PanelOpeningData as PanelOpeningDataMojo,                 //
             PanelState as PanelStateMojo,                             //
             PinCandidate as PinCandidateMojo,                         //
             PinCandidatesObserverInterface,                           //
             TabData as TabDataMojo,                                   //
             TabDataHandlerInterface,                                  //
             TabFaviconHandlerInterface,                               //
             WebClientInterface,                                       //
} from '../glic.mojom-webui.js';
import {CaptureRegionErrorReason, ClientCapabilities, HostCapability, ResponseStopCause} from '../glic_api/glic_api.js';
import type {                            //
             ActivateTabOptions,         //
             AdditionalContext,          //
             CaptureRegionParams,        //
             CaptureRegionResult,        //
             ChromeVersion,              //
             ClientErrorDialogType,      //
             ConversationInfo,           //
             CounterAbuseVerdict,        //
             CreateTabOptions,           //
             FileUploadPolicyState,      //
             FocusedTabData,             //
             FormFactor,                 //
             GeicBrowserHost,            //
             GeminiEnterpriseSettings,   //
             GetPinCandidatesOptions,    //
             GlicBrowserHost,            //
             GlicBrowserHostMetrics,     //
             GlicClientStorage,          //
             GlicHostRegistry,           //
             GlicWebClient,              //
             ImageBytesResult,           //
             MicrophoneStatus,           //
             Observable,                 //
             ObservableValue,            //
             OnResponseStoppedDetails,   //
             OpenPanelInfo,              //
             OpenPinnedTabPickerOptions, //
             OpenSettingsOptions,        //
             PageMetadata,               //
             PanelOpeningData,           //
             PanelState,                 //
             PinCandidate,               //
             PinTabsOptions,             //
             Platform,                   //
             PromptType,                 //
             ResizeWindowOptions,        //
             SendTabToSelfDevice,        //
             TabContextOptions,          //
             TabContextResult,           //
             TabData,                    //
             UnpinTabsOptions,           //
             UserProfileInfo,            //
             WebClientMode,              //
             ZeroStateSuggestions,       //
} from '../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl, Subject} from '../observable.js';

import {GlicBrowserHostActor} from './actor/actor_client.js';
import {GlicBrowserHostAnnotation} from './annotation/annotation_client.js';
import {                              //
  additionalContextToClient,          //
  bitmapN32ToRGBAImage,               //
  captureRegionResultToClient,        //
  conversationInfoFromClient,         //
  conversionSettings,                 //
  counterAbuseVerdictFromClient,      //
  createTabOptionsFromClient,         //
  fileUploadPolicyStateToClient,      //
  focusedTabDataToClient,             //
  getPinCandidatesOptionsFromClient,  //
  idFromClient,                       //
  idToClient,                         //
  imageBytesResultToClient,           //
  invokeOptionsToClient,              //
  microphoneStatusToMojo,             //
  openPinnedTabPickerOptionsToMojo,   //
  optionalFromClient,                 //
  pageMetadataToClient,               //
  panelOpeningDataToClient,           //
  panelStateToClient,                 //
  pinCandidateToClient,               //
  pinTabsOptionsToMojo,               //
  replaceProperties,                  //
  sendTabToSelfDeviceToClient,        //
  tabContextOptionsFromClient,        //
  tabContextToClient,                 //
  tabDataToClient,                    //
  timeDeltaFromClient,                //
  unpinTabsOptionsToMojo,             //
  urlFromClient,                      //
  urlToClient,                        //
  webClientModeToMojo,                //
} from './conversions.js';
import {GlicBrowserHostExperimentalTriggering} from './experimental_triggering/experimental_triggering_client.js';
import {GlicBrowserHostGeic} from './geic/geic_client.js';
import {assertNever, createDelegationProxy, ErrorWithReasonImpl} from './glic_client_common.js';
import {rgbaImageToBlob} from './image_utils.js';
import {maybeWrapWithLogging} from './mojo_logging.js';
import {TabObservable, TabObservableSet} from './observable_set_by_tab_id.js';
import {GlicBrowserHostSkills} from './skills/skills_client.js';
import {GlicBrowserHostTools} from './tools/tools_client.js';
import {GlicBrowserHostZeroStateSuggestions} from './zero_state_suggestions/zero_state_suggestions_client.js';

enum PanelOpenState {
  OPEN,
  CLOSED,
}

// Web client side of the Glic API.

export class GlicHostRegistryImpl implements GlicHostRegistry {
  private host: GlicBrowserHostImpl|undefined;

  async registerWebClient(webClient: GlicWebClient): Promise<void> {
    this.host = new GlicBrowserHostImpl(webClient);

    window.addEventListener('click', (e: MouseEvent) => {
      const target = (e.target as HTMLElement)?.closest?.('a');
      if (target && target.getAttribute('target') === '_blank' && target.href) {
        e.preventDefault();
        this.host?.getHandler().createTab(urlFromClient(target.href), {
          openInBackground: false,
          windowId: null,
        });
      }
    }, true);

    const clientCapabilities = webClient.getClientCapabilities?.() ?? new Set();
    await this.host.webClientCreated(clientCapabilities);
    let success = false;
    try {
      await webClient.initialize(this.host);
      success = true;
    } catch (e) {
      console.warn(e);
    }
    if (this.host) {
      this.host.webClientInitialized(success);
    }
  }
}

export class GlicBrowserHostImpl implements GlicBrowserHost,
                                            WebClientInterface {
  readonly actorClient: GlicBrowserHostActor;
  readonly annotationClient: GlicBrowserHostAnnotation;
  readonly skillsClient: GlicBrowserHostSkills;
  readonly toolsClient: GlicBrowserHostTools;
  readonly experimentalTriggeringClient =
      new GlicBrowserHostExperimentalTriggering();
  readonly suggestionsClient: GlicBrowserHostZeroStateSuggestions;
  readonly geicClient: GlicBrowserHostGeic;

  private chromeVersion?: ChromeVersion;
  private platform?: Platform;
  private formFactor?: FormFactor;
  private panelState = ObservableValueImpl.withNoValue<PanelState>();
  canAttachPanelValue = ObservableValueImpl.withNoValue<boolean>();
  private focusedTabStateV2 = ObservableValueImpl.withNoValue<FocusedTabData>();
  // @deprecated Legacy settings no longer populated; returns undefined.
  // TODO(b/570016708): Remove once web client usages are eliminated.
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
  private clientStorage: GlicClientStorageImpl;
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

  private readonly getTabByIdObservableSet: TabDataObservableSet;
  private readonly getTabFaviconByIdObservableSet: TabFaviconObservableSet;
  notifyPanelWillOpenCompleted = Promise.withResolvers<void>();
  private panelOpenState = PanelOpenState.CLOSED;

  private handler: WebClientHandlerRemote;
  private isDestroyed = false;

  constructor(
      public webClient: GlicWebClient,
  ) {
    this.handler = maybeWrapWithLogging(
        new WebClientHandlerRemote(), {prefix: 'WebClientHandler'});
    this.handler.onConnectionError.addListener(() => {
      if (this.isDestroyed) {
        return;
      }
      console.warn(`Mojo connection error in glic host`);
    });
    const receiver = this.handler.$.bindNewPipeAndPassReceiver();
    receiver.bindInBrowser();

    this.actorClient = new GlicBrowserHostActor();
    this.annotationClient = new GlicBrowserHostAnnotation();
    this.skillsClient = new GlicBrowserHostSkills();
    this.suggestionsClient = new GlicBrowserHostZeroStateSuggestions();
    this.toolsClient = new GlicBrowserHostTools();
    this.geicClient = new GlicBrowserHostGeic();

    this.getTabByIdObservableSet = new TabDataObservableSet(this.handler);
    this.getTabFaviconByIdObservableSet =
        new TabFaviconObservableSet(this.handler);
    this.metrics = new GlicBrowserHostMetricsImpl(this.handler);
    this.clientStorage = new GlicClientStorageImpl(this.handler);

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
    this.isDestroyed = true;
    this.pinCandidates?.setObsolete();
    this.actorClient.destroyActor();
    this.skillsClient.destroySkills();
    this.toolsClient.destroyTools();
    this.annotationClient.destroyAnnotation();
    this.geicClient.destroy();
    this.suggestionsClient.destroySuggestions();
    this.experimentalTriggeringClient.destroy();
    if (this.webClientReceiver) {
      this.webClientReceiver.$.close();
      this.webClientReceiver = undefined;
    }
  }

  async webClientCreated(clientCapabilities: Set<ClientCapabilities>) {
    conversionSettings.omitFaviconInTabData =
        clientCapabilities.has(ClientCapabilities.IGNORES_TAB_DATA_FAVICONS);
    this.webClientReceiver = new WebClientReceiver(this);
    const {initialState} = await this.handler.webClientCreated(
        this.webClientReceiver.$.bindNewPipeAndPassRemote());
    conversionSettings.platform = enumToClient(initialState.platform);
    this.actorClient.initialize(initialState, this.handler);
    this.annotationClient.initialize(initialState, this.handler);
    this.skillsClient.initialize(initialState, this.handler);
    this.experimentalTriggeringClient.initialize(this.webClient, this.handler);
    this.suggestionsClient.initialize(initialState, this.handler);
    this.toolsClient.initialize(initialState, this.handler);
    this.geicClient.initialize(initialState, this.handler);

    const state = initialState;
    this.geminiEnterpriseSettings.assignAndSignal(
        state.geminiEnterpriseSettings ?? undefined);
    this.zoomLevel.assignAndSignal(state.zoomFactor);
    this.panelState.assignAndSignal(panelStateToClient(state.panelState));
    this.focusedTabStateV2.assignAndSignal(
        focusedTabDataToClient(state.focusedTabData));
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
    if (state.enableClientStorage) {
      this.clientStorage.initialize(state.clientStorage);
    } else {
      this.getClientStorage = undefined;
    }

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

    if (!this.hostCapabilities.has(HostCapability.SEND_TAB_TO_SELF)) {
      // MOJO_RUNTIME_FEATURE_GATED GetSendTabToSelfDevices
      this.getSendTabToSelfDevices = undefined;
    }
  }

  webClientInitialized(success: boolean) {
    if (success) {
      this.handler.webClientInitialized();
    } else {
      this.handler.webClientInitializeFailed();
    }
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
      const clientOptions = invokeOptionsToClient(options);
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

  // @deprecated
  // TODO(b/570016708): Remove once web client usages are eliminated.
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
    this.focusedTabStateV2.assignAndSignal(
        focusedTabDataToClient(focusedTabData));
  }

  notifyManualResizeChanged(resizing: boolean): void {
    this.manuallyResizing.assignAndSignal(resizing);
  }

  notifyBrowserIsOpenChanged(browserIsOpen: boolean): void {
    this.isBrowserOpenValue.assignAndSignal(browserIsOpen);
  }

  notifyInstanceActivationChanged(_instanceIsActive: boolean): void {}

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
    this.additionalContextSubject.next(additionalContextToClient(context));
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

  notifyClientStorageItemChanged(key: string, value: string|null): void {
    this.clientStorage.onRemoteItemChanged(key, value);
  }

  notifyClientStorageCleared(): void {
    this.clientStorage.onRemoteCleared();
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
    this.handler.reportClientTransientError(abslStatus);
  }

  processCounterAbuseVerdict?
      (tabId: string, verdict: CounterAbuseVerdict): void {
    const mojoVerdict = counterAbuseVerdictFromClient(verdict);
    this.handler.processCounterAbuseVerdict(idFromClient(tabId), mojoVerdict);
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

  async getSendTabToSelfDevices?(): Promise<SendTabToSelfDevice[]> {
    const {devices} = await this.handler.getSendTabToSelfDevices();
    return devices.map(sendTabToSelfDeviceToClient);
  }

  async getModelQualityClientId?(): Promise<string> {
    const {modelQualityClientId} = await this.handler.getModelQualityClientId();
    return modelQualityClientId;
  }

  // @deprecated
  // TODO(b/570016708): Remove once web client usages are eliminated.
  getGeminiEnterpriseSettings?
      (): ObservableValueImpl<GeminiEnterpriseSettings|undefined> {
    return this.geminiEnterpriseSettings;
  }

  async switchConversation(info?: ConversationInfo): Promise<void> {
    const {errorReason} = await this.handler.switchConversation(
        conversationInfoFromClient(info ?? {
          conversationId: '',
          conversationTitle: '',
          clientData: undefined,
        }));
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl(
          'switchConversation', errorReason.valueOf());
    }
  }

  async registerConversation(info: ConversationInfo): Promise<void> {
    const {errorReason} = await this.handler.registerConversation(
        conversationInfoFromClient(info));
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl(
          'registerConversation', errorReason.valueOf());
    }
  }

  async getContextFromFocusedTab(options: TabContextOptions):
      Promise<TabContextResult> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextFromFocusedTab(
            tabContextOptionsFromClient(options));
    if (!tabContext) {
      throw new Error(`getContextFromFocusedTab failed: ${errorReason}`);
    }
    return tabContextToClient(tabContext);
  }

  async setMaximumNumberOfPinnedTabs?(requestedMax: number): Promise<number> {
    const {effectiveMax} = await this.handler.setMaximumNumberOfPinnedTabs(
        requestedMax >= 0 ? requestedMax : 0);
    return effectiveMax;
  }

  async getContextFromTab?
      (tabId: string, options: TabContextOptions): Promise<TabContextResult> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextFromTab(
            idFromClient(tabId), tabContextOptionsFromClient(options));
    if (!tabContext) {
      throw new Error(`getContextFromTab failed: ${errorReason}`);
    }
    return tabContextToClient(tabContext);
  }

  async getImageBytesFromTab?
      (tabId: string, documentId: string, domNodeId: number):
          Promise<ImageBytesResult> {
    const {result: {errorReason, imageBytes}} =
        await this.handler.getImageBytesFromTab(
            idFromClient(tabId), documentId, domNodeId);
    if (!imageBytes) {
      throw new Error(`getImageBytesFromTab failed: ${errorReason}`);
    }
    const result = imageBytesResultToClient(imageBytes);
    if (!result) {
      throw new Error('Failed to get image bytes');
    }
    return result;
  }

  getTabById?(tabId: string): ObservableValueImpl<TabData> {
    return this.getTabByIdObservableSet.getObservableByTabId(tabId);
  }

  getTabFaviconById?(tabId: string): ObservableValueImpl<Blob|undefined> {
    return this.getTabFaviconByIdObservableSet.getObservableByTabId(tabId);
  }

  activateTab?(tabId: string): void {
    this.handler.activateTab(idFromClient(tabId));
  }

  onModeChange?(newMode: WebClientMode): void {
    this.handler.onModeChange(webClientModeToMojo(newMode));
  }

  onMicrophoneStatusChange?(status: MicrophoneStatus): void {
    this.handler.onMicrophoneStatusChange(microphoneStatusToMojo(status));
  }

  setErrorDialogState?(shownDialogType?: ClientErrorDialogType): void {
    this.handler.clientErrorDialogStateChanged(enumFromClient(shownDialogType));
    // TODO(b/506142920): Avoid showing error panels to the user if it is
    // presented while the panel is backgrounded. Automatically reload the
    // page instead.
  }

  resizeWindow(width: number, height: number, options?: ResizeWindowOptions):
      Promise<void> {
    return this.handler.resizeWidget(
        {width, height}, timeDeltaFromClient(options?.durationMs));
  }

  async enableDragResize?(enabled: boolean): Promise<void> {
    this.handler.enableDragResize(enabled);
  }

  captureRegion?
      (params?: CaptureRegionParams): ObservableValue<CaptureRegionResult> {
    if (this.captureRegionObservable) {
      this.captureRegionObservable.complete();
    }
    this.captureRegionObservable =
        new CaptureRegionObservable(this.handler, params);
    return this.captureRegionObservable;
  }

  deleteCapturedRegion?(tabId: string, regionId: string): void {
    this.handler.deleteCapturedRegion(idFromClient(tabId), regionId);
  }

  async setMinimumWidgetSize(width: number, height: number): Promise<void> {
    this.handler.setMinimumPanelSize({width, height});
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
    return this.handler.setMicrophonePermissionState(enabled);
  }

  setLocationPermissionState(enabled: boolean): Promise<void> {
    return this.handler.setLocationPermissionState(enabled);
  }

  setTabContextPermissionState(enabled: boolean): Promise<void> {
    if (this.enableDefaultTabContextSettingFeature) {
      this.permissionStateTabContext.assignAndSignal(enabled);
      return Promise.resolve();
    }
    return this.handler.setTabContextPermissionState(enabled);
  }

  setClosedCaptioningSetting?(enabled: boolean): Promise<void> {
    return this.handler.setClosedCaptioningSetting(enabled);
  }

  setContextAccessIndicator(show: boolean): void {
    this.handler.setContextAccessIndicator(show);
  }

  setActuationOnWebSetting?(enabled: boolean): Promise<void> {
    return this.handler.setActuationOnWebSetting(enabled);
  }

  async getUserProfileInfo?(): Promise<UserProfileInfo> {
    return this.enableCachedGetUserProfileInfo ? this.fetchUserProfileCached() :
                                                 this.fetchUserProfileDirect();
  }

  private async fetchUserProfileDirect(): Promise<UserProfileInfo> {
    const {profileInfo} = await this.handler.getUserProfileInfo();
    if (!profileInfo) {
      throw new Error('getUserProfileInfo failed');
    }
    const avatarIcon = profileInfo.avatarIcon ?
        bitmapN32ToRGBAImage(profileInfo.avatarIcon) :
        undefined;
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
        const {profileInfo} = await this.handler.getUserProfileInfo();

        if (!profileInfo) {
          throw new Error('getUserProfileInfo failed');
        }

        let avatarIcon = profileInfo.avatarIcon ?
            bitmapN32ToRGBAImage(profileInfo.avatarIcon) :
            undefined;
        profileInfo.avatarIcon = null;

        let blobPromise: Promise<Blob|undefined>|undefined;
        return replaceProperties(profileInfo, {
          avatarIcon: () => {
            if (blobPromise) {
              return blobPromise;
            }
            if (!avatarIcon) {
              blobPromise = Promise.resolve(undefined);
              return blobPromise;
            }
            const newBlob = rgbaImageToBlob(avatarIcon);
            // Clear memory after conversion
            avatarIcon = undefined;
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
    const {success} = await this.handler.syncCookies();
    if (!success) {
      throw Error('refreshSignInCookies failed');
    }
  }

  setAudioDucking?(enabled: boolean): void {
    this.handler.setAudioDucking(enabled);
  }

  getMetrics(): GlicBrowserHostMetrics {
    return this.metrics;
  }

  getClientStorage?(): GlicClientStorage {
    return this.clientStorage;
  }

  getGeicClient(): GeicBrowserHost {
    return this.geicClient;
  }

  setSyntheticExperimentState(trialName: string, groupName: string): void {
    this.handler.setSyntheticExperimentState(trialName, groupName);
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
    const {enabled} = await this.handler.getOsMicrophonePermissionStatus();
    return enabled;
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
    const {pinnedAll} = await this.handler.pinTabs(
        tabIds.map((x) => idFromClient(x)), pinTabsOptionsToMojo(options));
    return pinnedAll;
  }

  async unpinTabs?
      (tabIds: string[], options?: UnpinTabsOptions): Promise<boolean> {
    const {unpinnedAll} = await this.handler.unpinTabs(
        tabIds.map((x) => idFromClient(x)), unpinTabsOptionsToMojo(options));
    return unpinnedAll;
  }

  unpinAllTabs?(options?: UnpinTabsOptions): void {
    this.handler.unpinAllTabs(unpinTabsOptionsToMojo(options));
  }

  openPinnedTabPicker?(options?: OpenPinnedTabPickerOptions): Promise<void> {
    return this.handler.openPinnedTabPicker(
        openPinnedTabPickerOptionsToMojo(options));
  }

  getPinCandidates?
      (options: GetPinCandidatesOptions): ObservableValue<PinCandidate[]> {
    this.pinCandidates?.setObsolete();
    return this.pinCandidates = new PinCandidatesObservable(this, options);
  }

  async getZeroStateSuggestionsForFocusedTab?
      (isFirstRun?: boolean): Promise<ZeroStateSuggestions> {
    const zeroStateResult =
        await this.handler.getZeroStateSuggestionsForFocusedTab(
            optionalFromClient(isFirstRun));
    const zeroStateData = zeroStateResult.suggestions;
    if (!zeroStateData) {
      return {
        suggestions: [],
        tabId: '',
        url: '',
      };
    }
    return {
      tabId: idToClient(zeroStateData.tabId),
      url: urlToClient(zeroStateData.url),
      suggestions: zeroStateData.suggestions,
    };
  }

  maybeRefreshUserStatus?(): void {
    this.cachedUserProfile = undefined;
    this.handler.maybeRefreshUserStatus();
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
          const {success} = await this.handler.subscribeToPageMetadata(
              idFromClient(tabId), isActive ? names : []);
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
    this.handler.setOnboardingCompleted();
  }

  isOnboardingCompleted?(): ObservableValue<boolean> {
    return this.onboardingCompleted;
  }
}

class GlicBrowserHostMetricsImpl implements GlicBrowserHostMetrics {
  constructor(private handler: WebClientHandlerRemote) {}

  onOptinImpression(): void {
    this.handler.onOptinImpression();
  }

  onInputFocused(): void {
    this.handler.onInputFocused();
  }

  onUserInputSubmitted(mode: number, promptType?: PromptType): void {
    this.handler.onUserInputSubmitted(
        webClientModeToMojo(mode),
        enumFromClient(promptType) ?? PromptTypeMojo.kUnspecified);
  }

  onReaction(reactionType: number): void {
    this.handler.onReaction(reactionType);
  }

  onPerformActionResultSubmitted(isRetry?: boolean): void {
    this.handler.onActionSubmitted(isRetry ?? false);
  }

  onContextUploadStarted(): void {
    this.handler.onContextUploadStarted();
  }

  onContextUploadCompleted(): void {
    this.handler.onContextUploadCompleted();
  }

  onResponseStarted(): void {
    this.handler.onResponseStarted();
  }

  onResponseStopped(details?: OnResponseStoppedDetails): void {
    let cause: ResponseStopCauseMojo;
    switch (details?.cause) {
      case undefined:
        cause = ResponseStopCauseMojo.kUnknown;
        break;
      case ResponseStopCause.USER:
        cause = ResponseStopCauseMojo.kUser;
        break;
      case ResponseStopCause.OTHER:
        cause = ResponseStopCauseMojo.kOther;
        break;
      default:
        assertNotReached();
    }
    this.handler.onResponseStopped({cause});
  }

  onSessionTerminated(): void {
    this.handler.onSessionTerminated();
  }

  onResponseRated(positive: boolean): void {
    this.handler.onResponseRated(positive);
  }

  onClosedCaptionsShown(): void {
    this.handler.onClosedCaptionsShown();
  }

  onTurnCompleted(model: number, duration: number): void {
    this.handler.onTurnCompleted(model, timeDeltaFromClient(duration));
  }

  onRecordUseCounter(counter: number): void {
    // Since the frontend can contain a newer version than what Chrome is
    // built against, we use a sparse histogram.
    this.handler.recordSparseValue('Glic.Api.UseCounter', counter);
  }
}

export interface ClientStorageHandler {
  setClientStorageItem(key: string, value: string|null): Promise<unknown>;
  clearClientStorage(): Promise<unknown>;
}

export class GlicClientStorageImpl implements GlicClientStorage {
  private storage = new Map<string, string>();
  // While `GlicWebClientHandler` already suppresses echo notifications on the
  // originating handler, `pendingClears` and `pendingKeyMutations` guard
  // against stale in-flight notifications from other writers arriving before
  // the browser processes this client's mutation (at the tradeoff of
  // potentially ignoring a concurrent remote mutation that occurs before the
  // `WebClientHandler` reply resolves across the separate Mojo pipe).
  private pendingClears = 0;
  private pendingKeyMutations =
      new Map<string, {count: number, latestValue: string|null}>();

  constructor(private handler: ClientStorageHandler) {}

  initialize(initialStorage: {[key: string]: string}): void {
    this.storage.clear();
    for (const [k, v] of Object.entries(initialStorage)) {
      this.storage.set(k, v);
    }
  }

  onRemoteItemChanged(key: string, value: string|null): void {
    if (this.pendingClears > 0) {
      return;
    }
    const pending = this.pendingKeyMutations.get(key);
    if (pending && pending.count > 0) {
      return;
    }
    if (value !== null) {
      this.storage.set(key, value);
    } else {
      this.storage.delete(key);
    }
  }

  onRemoteCleared(): void {
    this.storage.clear();
    for (const [k, pending] of this.pendingKeyMutations.entries()) {
      if (pending.count > 0 && pending.latestValue !== null) {
        this.storage.set(k, pending.latestValue);
      }
    }
  }

  get length(): number {
    return this.storage.size;
  }

  key(index: number): string|null {
    if (index < 0 || index >= this.storage.size) {
      return null;
    }
    return Array.from(this.storage.keys())[index] ?? null;
  }

  getItem(key: string): string|null {
    return this.storage.get(key) ?? null;
  }

  setItem(key: string, value: string): void {
    this.storage.set(key, value);
    this.enqueueKeyMutation(key, value);
  }

  removeItem(key: string): void {
    this.storage.delete(key);
    this.enqueueKeyMutation(key, null);
  }

  clear(): void {
    this.storage.clear();
    this.pendingKeyMutations.clear();
    this.pendingClears++;
    this.handler.clearClientStorage().finally(() => {
      this.pendingClears = Math.max(0, this.pendingClears - 1);
    });
  }

  private enqueueKeyMutation(key: string, value: string|null): void {
    const existing = this.pendingKeyMutations.get(key);
    const entry = existing ?? {count: 0, latestValue: value};
    entry.count++;
    entry.latestValue = value;
    if (!existing) {
      this.pendingKeyMutations.set(key, entry);
    }
    this.handler.setClientStorageItem(key, value).finally(() => {
      entry.count--;
      if (entry.count <= 0 && this.pendingKeyMutations.get(key) === entry) {
        this.pendingKeyMutations.delete(key);
      }
    });
  }
}

export class CaptureRegionObservable extends
    ObservableValueImpl<CaptureRegionResult> implements
        CaptureRegionObserverInterface {
  private receiver?: CaptureRegionObserverReceiver;
  constructor(
      private handler: WebClientHandlerRemote,
      private params?: CaptureRegionParams) {
    super(false);
  }

  private close() {
    this.receiver?.$.close();
    this.receiver = undefined;
  }

  override activeSubscriptionChanged(hasActiveSubscription: boolean): void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    if (this.isStopped()) {
      return;
    }
    if (hasActiveSubscription) {
      this.close();
      this.receiver = new CaptureRegionObserverReceiver(this);
      this.receiver.onConnectionError.addListener(() => {
        if (!this.isStopped()) {
          this.processError(CaptureRegionErrorReason.UNKNOWN);
        }
      });
      const remote = this.receiver.$.bindNewPipeAndPassRemote();
      this.handler.captureRegion(
          remote,
          this.params ? {
            tabId: idFromClient(this.params.tabId),
            options: tabContextOptionsFromClient(this.params.options),
          } :
                        null);
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

  onUpdate(
      result: CaptureRegionResultMojo|null,
      reason: CaptureRegionErrorReasonMojo|null): void {
    if (this.isStopped()) {
      return;
    }
    const captureResult = captureRegionResultToClient(result);
    if (captureResult) {
      this.assignAndSignal(captureResult);
    } else {
      const errorReason = reason !== null ?
          enumToClient(reason) as CaptureRegionErrorReason :
          CaptureRegionErrorReason.UNKNOWN;
      this.processError(errorReason);
    }
  }

  processError(reason: CaptureRegionErrorReason) {
    if (this.isStopped()) {
      return;
    }
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

export class TabDataObservableSet extends
    TabObservableSet<TabData, TabDataObservable> {
  constructor(handler: WebClientHandlerRemote) {
    super(
        (tabId, onComplete) =>
            new TabDataObservable(tabId, handler, onComplete));
  }
}

export class TabDataObservable extends TabObservable<TabData> implements
    TabDataHandlerInterface {
  private receiver?: TabDataHandlerReceiver;

  constructor(
      tabId: string, private handler: WebClientHandlerRemote,
      onComplete: () => void) {
    super(tabId, onComplete);
  }

  protected override isConnected(): boolean {
    return this.receiver !== undefined;
  }

  protected override connectToSource(): void {
    this.receiver = new TabDataHandlerReceiver(this);
    this.receiver.onConnectionError.addListener(() => {
      this.complete();
    });
    this.handler.subscribeToTabData(
        idFromClient(this.tabId), this.receiver.$.bindNewPipeAndPassRemote());
  }

  protected override disconnectFromSource(): void {
    this.receiver?.$.close();
    this.receiver = undefined;
  }

  onTabDataChanged(tabData: TabDataMojo): void {
    if (this.isStopped()) {
      return;
    }
    this.assignAndSignal(tabDataToClient(tabData));
  }
}

export class TabFaviconObservableSet extends
    TabObservableSet<Blob|undefined, TabFaviconObservable> {
  constructor(handler: WebClientHandlerRemote) {
    super(
        (tabId, onComplete) =>
            new TabFaviconObservable(tabId, handler, onComplete));
  }
}

export class TabFaviconObservable extends TabObservable<Blob|undefined>
    implements TabFaviconHandlerInterface {
  private receiver?: TabFaviconHandlerReceiver;

  constructor(
      tabId: string, private handler: WebClientHandlerRemote,
      onComplete: () => void) {
    super(tabId, onComplete);
  }

  protected override isConnected(): boolean {
    return this.receiver !== undefined;
  }

  protected override connectToSource(): void {
    this.receiver = new TabFaviconHandlerReceiver(this);
    this.receiver.onConnectionError.addListener(() => {
      this.complete();
    });
    this.handler.subscribeToTabFavicon(
        idFromClient(this.tabId), this.receiver.$.bindNewPipeAndPassRemote());
  }

  protected override disconnectFromSource(): void {
    this.receiver?.$.close();
    this.receiver = undefined;
  }

  onTabFaviconChanged(favicon: BitmapN32|null): void {
    if (this.isStopped()) {
      return;
    }
    let blob: Blob|undefined;
    if (favicon) {
      const rgba = bitmapN32ToRGBAImage(favicon);
      if (rgba) {
        blob = rgbaImageToBlob(rgba);
      }
    }
    this.assignAndSignal(blob);
  }
}
