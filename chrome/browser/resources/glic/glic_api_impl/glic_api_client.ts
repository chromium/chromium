// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotatedPageData, ChromeVersion, DraggableArea, FocusedTabCandidate, FocusedTabData, GlicBrowserHost, GlicBrowserHostMetrics, GlicHostRegistry, GlicWebClient, ObservableValue, OpenPanelInfo, PanelState, PdfDocumentData, Screenshot, ScrollToParams, Subscriber, TabContextOptions, TabContextResult, TabData, UserProfileInfo} from '../glic_api/glic_api.js';

import {PostMessageRequestReceiver, PostMessageRequestSender} from './post_message_transport.js';
import type {AnnotatedPageDataPrivate, FocusedTabCandidatePrivate, FocusedTabDataPrivate, PdfDocumentDataPrivate, RgbaImage, TabContextResultPrivate, TabDataPrivate, WebClientRequestTypes} from './request_types.js';
import {ImageAlphaType, ImageColorType} from './request_types.js';


// Web client side of the Glic API.
// Communicates with the Chrome-WebUI-side in glic_api_host.ts

export class GlicHostRegistryImpl implements GlicHostRegistry {
  constructor(private windowProxy: WindowProxy) {}

  async registerWebClient(webClient: GlicWebClient): Promise<void> {
    const host = new GlicBrowserHostImpl(webClient, this.windowProxy);
    await host.webClientCreated();
    let success = false;
    try {
      await webClient.initialize(host);
      success = true;
    } catch (e) {
      console.error(e);
    }
    host.webClientInitialized(success);
  }
}

type Promisify<T> = T extends void ? void : Promise<T>;

// A type which the guest should implement.
// This helps verify that WebClientMessageHandler is implemented with the
// correct parameter and return types.
type WebClientMessageHandlerInterface = {
  [Property in keyof WebClientRequestTypes]:
      // `payload` is the message payload.
      // `responseTransfer` is populated by objects that should be transferred
      // when sending the message.
  (payload: WebClientRequestTypes[Property]['request'],
   responseTransfer: Transferable[]) =>
      Promisify<WebClientRequestTypes[Property]['response']>;
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
      console.error(e);
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
      console.error(e);
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
}

class GlicBrowserHostImpl implements GlicBrowserHost {
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
  private metrics: GlicBrowserHostMetricsImpl;

  constructor(private webClient: GlicWebClient, windowProxy: WindowProxy) {
    this.sender = new PostMessageRequestSender(windowProxy, 'chrome://glic');
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
    const state = await this.sender.requestWithResponse(
        'glicBrowserWebClientCreated', {});
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

    if (!state.scrollToEnabled) {
      (this as GlicBrowserHost).scrollTo = undefined;
    }
  }

  webClientInitialized(success: boolean) {
    this.sender.requestNoResponse('glicBrowserWebClientInitialized', {success});
  }

  async handleRawRequest(type: string, payload: any):
      Promise<{payload: any, transfer: Transferable[]}|undefined> {
    if (!this.handlerFunctionNames.has(type)) {
      return;
    }
    const handlerFunction = (this.webClientMessageHandler as any)[type];
    const transfer: Transferable[] = [];
    const response = await handlerFunction.call(
        this.webClientMessageHandler, payload, transfer);
    if (!response) {
      return;
    }
    return {payload: response, transfer};
  }

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
    this.sender.requestNoResponse('glicBrowserOpenGlicSettingsPage', {});
  }

  closePanel(): Promise<void> {
    return this.sender.requestWithResponse('glicBrowserClosePanel', {});
  }

  attachPanel(): void {
    return this.sender.requestNoResponse('glicBrowserAttachPanel', {});
  }

  detachPanel(): void {
    return this.sender.requestNoResponse('glicBrowserDetachPanel', {});
  }

  showProfilePicker(): void {
    this.sender.requestNoResponse('glicBrowserShowProfilePicker', {});
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
        'glicBrowserCaptureScreenshot', {});
    return screenshotResult.screenshot;
  }

  setWindowDraggableAreas(areas: DraggableArea[]) {
    return this.sender.requestWithResponse(
        'glicBrowserSetWindowDraggableAreas', {areas});
  }

  getPanelState(): ObservableValueImpl<PanelState> {
    return this.panelState;
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
        'glicBrowserGetUserProfileInfo', {});
    if (!profileInfo) {
      throw new Error('getUserProfileInfo failed');
    }
    const {displayName, email, avatarIcon} = profileInfo;
    return {
      displayName,
      email,
      avatarIcon: async () => avatarIcon && rgbaImageToBlob(avatarIcon),
    };
  }

  async refreshSignInCookies(): Promise<void> {
    const result = await this.sender.requestWithResponse(
        'glicBrowserRefreshSignInCookies', {});
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
}

class GlicBrowserHostMetricsImpl implements GlicBrowserHostMetrics {
  constructor(private sender: PostMessageRequestSender) {}

  onUserInputSubmitted(mode: number): void {
    this.sender.requestNoResponse('glicBrowserOnUserInputSubmitted', {mode});
  }

  onResponseStarted(): void {
    this.sender.requestNoResponse('glicBrowserOnResponseStarted', {});
  }

  onResponseStopped(): void {
    this.sender.requestNoResponse('glicBrowserOnResponseStopped', {});
  }

  onSessionTerminated(): void {
    this.sender.requestNoResponse('glicBrowserOnSessionTerminated', {});
  }

  onResponseRated(positive: boolean): void {
    this.sender.requestNoResponse('glicBrowserOnResponseRated', {positive});
  }
}

// Returns a promise which resolves to the `GlicHostRegistry`. This promise
// never resolves if a message from Chromium glic is not received.
// This should be called on or before page load.
export function createGlicHostRegistryOnLoad(): Promise<GlicHostRegistry> {
  const {promise, resolve} = Promise.withResolvers<GlicHostRegistry>();
  const messageHandler = (event: MessageEvent) => {
    if (event.origin !== 'chrome://glic' || event.source === null) {
      return;
    }
    if (event.data && event.data['type'] === 'glic-bootstrap') {
      resolve(new GlicHostRegistryImpl(event.source as WindowProxy));
      window.removeEventListener('message', messageHandler);
    }
  };
  window.addEventListener('message', messageHandler);
  return promise;
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

// Helper function to shallow-copy an object and replace some properties.
// Useful to convert from these private types to public types. This will fail to
// compile if a property is missed.
function replaceProperties<O, R>(
    original: O, replacements: R): Omit<O, keyof R>&R {
  return Object.assign(Object.assign({}, original) as any, replacements);
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
        sub.onChange(v);
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
