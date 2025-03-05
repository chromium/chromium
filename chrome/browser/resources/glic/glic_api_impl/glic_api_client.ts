// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotatedPageData, ChromeVersion, DraggableArea, FocusedTabCandidate, FocusedTabData, GlicBrowserHost, GlicBrowserHostMetrics, GlicHostRegistry, GlicWebClient, ObservableValue, OpenPanelInfo, PanelState, PdfDocumentData, Screenshot, ScrollToParams, Subscriber, TabContextOptions, TabContextResult, TabData, UserProfileInfo} from '../glic_api/glic_api.js';

import {replaceProperties} from './conversions.js';
import {newSenderId, PostMessageRequestReceiver, PostMessageRequestSender} from './post_message_transport.js';
import type {ResponseExtras} from './post_message_transport.js';
import type {AnnotatedPageDataPrivate, FocusedTabCandidatePrivate, FocusedTabDataPrivate, PdfDocumentDataPrivate, RequestRequestType, RequestResponseType, RgbaImage, TabContextResultPrivate, TabDataPrivate, TransferableException, WebClientRequestTypes} from './request_types.js';
import {ImageAlphaType, ImageColorType, newTransferableException} from './request_types.js';


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
  constructor(
      private webClient: GlicWebClient, private host: GlicBrowserHostImpl) {}

  glicWebClientNotifyPanelOpened(payload: {
    attachedToWindowId: string|undefined,
  }): void {
    if (this.webClient.notifyPanelOpened) {
      this.webClient.notifyPanelOpened(payload.attachedToWindowId);
    }
  }

  async glicWebClientNotifyPanelWillOpen(payload: {panelState: PanelState}):
      Promise<{openPanelInfo?: OpenPanelInfo}> {
    let openPanelInfo: OpenPanelInfo|undefined;
    try {
      const result =
          await this.webClient.notifyPanelWillOpen?.(payload.panelState);
      if (result) {
        openPanelInfo = result;
      }
    } catch (e) {
      console.warn(e);
    }
    return {openPanelInfo};
  }

  glicWebClientNotifyPanelClosed(): void {
    if (this.webClient.notifyPanelClosed) {
      this.webClient.notifyPanelClosed();
    }
  }

  async glicWebClientNotifyPanelWasClosed(): Promise<void> {
    try {
      await this.webClient.notifyPanelWasClosed?.();
    } catch (e) {
      console.warn(e);
    }
  }

  glicWebClientPanelStateChanged(payload: {panelState: PanelState}): void {
    this.host.getPanelState().assignAndSignal(payload.panelState);
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

  glicWebClientNotifyFocusedTabChanged(payload: {
    focusedTabDataPrivate: FocusedTabDataPrivate,
  }) {
    const focusedTabData =
        convertFocusedTabDataFromPrivate(payload.focusedTabDataPrivate);
    this.host.getFocusedTabStateV2().assignAndSignal(focusedTabData);
    // Keep below for backwards compatibility.
    this.host.getFocusedTabState().assignAndSignal(focusedTabData.focusedTab);
  }

  glicWebClientNotifyPanelActiveChanged(payload: {panelActive: boolean}): void {
    this.host.panelActiveValue.assignAndSignal(payload.panelActive);
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
  private focusedTabState =
      ObservableValueImpl.withNoValue<TabData|undefined>();
  private focusedTabStateV2 = ObservableValueImpl.withNoValue<FocusedTabData>();
  private permissionStateMicrophone =
      ObservableValueImpl.withNoValue<boolean>();
  private permissionStateLocation = ObservableValueImpl.withNoValue<boolean>();
  private permissionStateTabContext =
      ObservableValueImpl.withNoValue<boolean>();
  panelActiveValue = ObservableValueImpl.withNoValue<boolean>();
  private metrics: GlicBrowserHostMetricsImpl;

  constructor(public webClient: GlicWebClient, windowProxy: WindowProxy) {
    // TODO(harringtond): Ideally, we could ensure we only process requests from
    // the single senderId used by the web client. This would avoid accidental
    // processing of requests from a previous client. This risk is very minimal,
    // as it would require reloading the webview page and initializing a new
    // web client very quickly, and in normal operation, the webview does not
    // reload after successful load.
    this.sender =
        new PostMessageRequestSender(windowProxy, 'chrome://glic', this.hostId);
    this.receiver =
        new PostMessageRequestReceiver('chrome://glic', windowProxy, this);
    this.webClientMessageHandler =
        new WebClientMessageHandler(this.webClient, this);
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
    this.panelState.assignAndSignal(state.panelState);
    const focusedTabData =
        convertFocusedTabDataFromPrivate(state.focusedTabData);
    this.focusedTabState.assignAndSignal(focusedTabData.focusedTab);
    this.focusedTabStateV2.assignAndSignal(focusedTabData);
    this.permissionStateMicrophone.assignAndSignal(
        state.microphonePermissionEnabled);
    this.permissionStateLocation.assignAndSignal(
        state.locationPermissionEnabled);
    this.permissionStateTabContext.assignAndSignal(
        state.tabContextPermissionEnabled);
    this.canAttachPanelValue.assignAndSignal(state.canAttach);
    this.chromeVersion = state.chromeVersion;
    this.panelActiveValue.assignAndSignal(state.panelIsActive);

    if (!state.scrollToEnabled) {
      (this as GlicBrowserHost).scrollTo = undefined;
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

  onRequestReceived(_type: string): void {}
  onRequestHandlerException(_type: string): void {}
  onRequestCompleted(_type: string): void {}

  // GlicBrowserHost implementation.

  getChromeVersion() {
    return Promise.resolve(this.chromeVersion!);
  }

  async createTab(
      url: string,
      options: {openInBackground?: boolean, windowId?: string},
      ): Promise<TabData> {
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

  openGlicSettingsPage(): void {
    this.sender.requestNoResponse('glicBrowserOpenGlicSettingsPage', undefined);
  }

  closePanel(): Promise<void> {
    return this.sender.requestWithResponse('glicBrowserClosePanel', undefined);
  }

  attachPanel(): void {
    this.sender.requestNoResponse('glicBrowserAttachPanel', undefined);
  }

  detachPanel(): void {
    this.sender.requestNoResponse('glicBrowserDetachPanel', undefined);
  }

  showProfilePicker(): void {
    this.sender.requestNoResponse('glicBrowserShowProfilePicker', undefined);
  }

  async getContextFromFocusedTab(options: TabContextOptions):
      Promise<TabContextResult> {
    const context = await this.sender.requestWithResponse(
        'glicBrowserGetContextFromFocusedTab', {options});
    return convertTabContextResultFromPrivate(context.tabContextResult);
  }

  async resizeWindow(width: number, height: number, options?: {
    durationMs?: number,
  }): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserResizeWindow', {size: {width, height}, options});
  }

  async captureScreenshot(): Promise<Screenshot> {
    const screenshotResult = await this.sender.requestWithResponse(
        'glicBrowserCaptureScreenshot', undefined);
    return screenshotResult.screenshot;
  }

  setWindowDraggableAreas(areas: DraggableArea[]): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetWindowDraggableAreas', {areas});
  }

  getPanelState(): ObservableValueImpl<PanelState> {
    return this.panelState;
  }

  panelActive(): ObservableValueImpl<boolean> {
    return this.panelActiveValue;
  }

  canAttachPanel(): ObservableValue<boolean> {
    return this.canAttachPanelValue;
  }

  getFocusedTabState(): ObservableValueImpl<TabData|undefined> {
    return this.focusedTabState;
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

  setMicrophonePermissionState(enabled: boolean): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetMicrophonePermissionState', {enabled});
  }

  setLocationPermissionState(enabled: boolean): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetLocationPermissionState', {enabled});
  }

  setTabContextPermissionState(enabled: boolean): Promise<void> {
    return this.sender.requestWithResponse(
        'glicBrowserSetTabContextPermissionState', {enabled});
  }

  setContextAccessIndicator(show: boolean): void {
    this.sender.requestWithResponse(
        'glicBrowserSetContextAccessIndicator', {show});
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

  getMetrics(): GlicBrowserHostMetrics {
    return this.metrics;
  }

  scrollTo(params: ScrollToParams): Promise<void> {
    return this.sender.requestWithResponse('glicBrowserScrollTo', {params});
  }

  setSyntheticExperimentState(trialName: string, groupName: string): void {
    this.sender.requestNoResponse(
        'glicBrowserSetSyntheticExperimentState', {trialName, groupName});
  }
}

class GlicBrowserHostMetricsImpl implements GlicBrowserHostMetrics {
  constructor(private sender: PostMessageRequestSender) {}

  onUserInputSubmitted(mode: number): void {
    this.sender.requestNoResponse('glicBrowserOnUserInputSubmitted', {mode});
  }

  onResponseStarted(): void {
    this.sender.requestNoResponse('glicBrowserOnResponseStarted', undefined);
  }

  onResponseStopped(): void {
    this.sender.requestNoResponse('glicBrowserOnResponseStopped', undefined);
  }

  onSessionTerminated(): void {
    this.sender.requestNoResponse('glicBrowserOnSessionTerminated', undefined);
  }

  onResponseRated(positive: boolean): void {
    this.sender.requestNoResponse('glicBrowserOnResponseRated', {positive});
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

function convertTabDataFromPrivate(data: TabDataPrivate): TabData {
  let faviconResult: Promise<Blob>|undefined;
  async function getFavicon() {
    if (data.favicon && !faviconResult) {
      faviconResult = rgbaImageToBlob(data.favicon);
      return faviconResult;
    }
    return faviconResult;
  }

  const favicon = data.favicon && getFavicon;
  return replaceProperties(data, {favicon});
}

function convertFocusedTabCandidateFromPrivate(
    data: FocusedTabCandidatePrivate): FocusedTabCandidate {
  const focusedTabCandidateData = data.focusedTabCandidateData &&
      convertTabDataFromPrivate(data.focusedTabCandidateData);
  return replaceProperties(data, {focusedTabCandidateData});
}

function convertFocusedTabDataFromPrivate(data: FocusedTabDataPrivate):
    FocusedTabData {
  const focusedTab =
      data.focusedTab && convertTabDataFromPrivate(data.focusedTab);
  const focusedTabCandidate = data.focusedTabCandidate &&
      convertFocusedTabCandidateFromPrivate(data.focusedTabCandidate);
  return replaceProperties(data, {focusedTab, focusedTabCandidate});
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

function convertTabContextResultFromPrivate(data: TabContextResultPrivate):
    TabContextResult {
  const tabData = convertTabDataFromPrivate(data.tabData);
  const pdfDocumentData = data.pdfDocumentData &&
      convertPdfDocumentDataFromPrivate(data.pdfDocumentData);
  const annotatedPageData = data.annotatedPageData &&
      convertAnnotatedPageDataFromPrivate(data.annotatedPageData);
  return replaceProperties(data, {tabData, pdfDocumentData, annotatedPageData});
}

class ObservableSubscription<T> implements Subscriber {
  constructor(
      public onChange: (newValue: T) => void,
      private onUnsubscribe: (self: ObservableSubscription<T>) => void) {}

  unsubscribe(): void {
    this.onUnsubscribe(this);
  }
}

/**
 * A observable value that can change over time. If value is initialized, sends
 * it to new subscribers upon subscribe().
 */
class ObservableValueImpl<T> implements ObservableValue<T> {
  private subscribers: Set<ObservableSubscription<T>> = new Set();

  private constructor(private isSet: boolean, private value: T|undefined) {}

  /** Create an ObservableValue which has an initial value. */
  static withValue<T>(value: T) {
    return new ObservableValueImpl(true, value);
  }

  /**
   * Create an Observable which has no initial value. Subscribers will not be
   * called until after assignAndSignal() is called the first time.
   */
  static withNoValue<T>() {
    return new ObservableValueImpl<T>(false, undefined);
  }

  assignAndSignal(v: T) {
    this.isSet = true;
    this.value = v;
    this.subscribers.forEach((sub) => {
      // Ignore if removed since forEach was called.
      if (this.subscribers.has(sub)) {
        try {
          sub.onChange(v);
        } catch (e) {
          console.warn(e);
        }
      }
    });
  }

  // Observable impl.
  subscribe(change: (newValue: T) => void): Subscriber {
    const newSub = new ObservableSubscription(
        change, (sub) => this.subscribers.delete(sub));
    this.subscribers.add(newSub);
    if (this.isSet) {
      change(this.value!);
    }
    return newSub;
  }
}
