// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeVersion, DraggableArea, ErrorWithReason, GlicBrowserHost, GlicHostRegistry, GlicWebClient, Observable, PanelState, PdfDocumentData, Subscriber, TabContextOptions, TabContextResult, TabData, UserProfileInfo} from '../glic_api/glic_api.js';
import {GetTabContextErrorReason} from '../glic_api/glic_api.js';

import {PostMessageRequestReceiver, PostMessageRequestSender} from './post_message_transport.js';
import type {PdfDocumentDataPrivate, RgbaImage, TabContextResultPrivate, TabDataPrivate, WebClientRequestTypes} from './request_types.js';
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
      Promise<void> {
    try {
      await this.webClient.notifyPanelWillOpen?.(payload.panelState);
    } catch (e) {
      console.error(e);
    }
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
    focusedTab: TabDataPrivate|undefined,
  }) {
    const tabData = !payload.focusedTab ?
        undefined :
        convertTabDataFromPrivate(payload.focusedTab);
    this.host.getFocusedTabState().assignAndSignal(tabData);
  }
}

class GlicBrowserHostImpl implements GlicBrowserHost {
  private sender: PostMessageRequestSender;
  private receiver: PostMessageRequestReceiver;
  private handlerFunctionNames: Set<string> = new Set();
  private webClientMessageHandler: WebClientMessageHandler;
  private chromeVersion?: ChromeVersion;
  private panelState?: ObservableValue<PanelState>;
  private focusedTabState?: ObservableValue<TabData|undefined>;
  private permissionStateMicrophone?: ObservableValue<boolean>;
  private permissionStateLocation?: ObservableValue<boolean>;
  private permissionStateTabContext?: ObservableValue<boolean>;

  constructor(private webClient: GlicWebClient, windowProxy: WindowProxy) {
    this.sender = new PostMessageRequestSender(windowProxy, 'chrome://glic');
    this.receiver =
        new PostMessageRequestReceiver('chrome://glic', windowProxy, this);
    this.webClientMessageHandler =
        new WebClientMessageHandler(this.webClient, this);

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
    this.panelState = new ObservableValue<PanelState>(state.panelState);
    this.focusedTabState = new ObservableValue<TabData|undefined>(
        state.focusedTab ? convertTabDataFromPrivate(state.focusedTab) :
                           undefined);
    this.permissionStateMicrophone =
        new ObservableValue(state.microphonePermissionEnabled);
    this.permissionStateLocation =
        new ObservableValue(state.locationPermissionEnabled);
    this.permissionStateTabContext =
        new ObservableValue(state.tabContextPermissionEnabled);
    this.chromeVersion = state.chromeVersion;
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

  async getChromeVersion() {
    return this.chromeVersion!;
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
    if (!context.tabContextResult) {
      throw new ErrorWithReasonImpl(
          'getContext failed',
          context.error || GetTabContextErrorReason.UNKNOWN);
    }
    return convertTabContextResultFromPrivate(context.tabContextResult);
  }

  async resizeWindow(width: number, height: number, options?: {
    durationMs?: number,
  }): Promise<void> {
    const durationMs = options?.durationMs;
    if (durationMs !== undefined && !Number.isFinite(durationMs)) {
      throw new Error('Invalid resize duration: ' + durationMs);
    }

    return this.sender.requestWithResponse(
        'glicBrowserResizeWindow', {size: {width, height}, options});
  }

  setWindowDraggableAreas(areas: DraggableArea[]) {
    return this.sender.requestWithResponse(
        'glicBrowserSetWindowDraggableAreas', {areas});
  }

  getPanelState(): ObservableValue<PanelState> {
    return this.panelState!;
  }

  getFocusedTabState(): ObservableValue<TabData|undefined> {
    return this.focusedTabState!;
  }

  getMicrophonePermissionState(): ObservableValue<boolean> {
    return this.permissionStateMicrophone!;
  }

  getLocationPermissionState(): ObservableValue<boolean> {
    return this.permissionStateLocation!;
  }

  getTabContextPermissionState(): ObservableValue<boolean> {
    return this.permissionStateTabContext!;
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
}

// Returns a promise which resolves to the `GlicHostRegistry`. This promise
// never resolves if a message from Chromium glic is not received.
// This should be called on or before page load.
export function createGlicHostRegistryOnLoad(): Promise<GlicHostRegistry> {
  const {promise, resolve} = Promise.withResolvers<GlicHostRegistry>();
  const messageHandler = async (event: MessageEvent) => {
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

class ErrorWithReasonImpl<T> extends Error implements ErrorWithReason<T> {
  constructor(message: string, public reason: T) {
    super(message);
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

// Helper function to shallow-copy an object and replace some properties.
// Useful to convert from these private types to public types. This will fail to
// compile if a property is missed.
function replaceProperties<O, R>(
    original: O, replacements: R): Omit<O, keyof R>&R {
  return Object.assign(Object.assign({}, original) as any, replacements);
}

function convertTabDataFromPrivate(data: TabDataPrivate): TabData {
  async function getFavicon() {
    if (data.favicon) {
      return rgbaImageToBlob(data.favicon);
    }
    return undefined;
  }

  const favicon = data.favicon && getFavicon;
  return replaceProperties(data, {favicon});
}

function convertPdfDocumentDataFromPrivate(data: PdfDocumentDataPrivate):
    PdfDocumentData {
  const pdfData = data.pdfData && new ReadableStream({
                    start(controller) {
                      controller.enqueue(data.pdfData);
                      controller.close();
                    },
                  });
  return replaceProperties(data, {pdfData});
}

function convertTabContextResultFromPrivate(data: TabContextResultPrivate):
    TabContextResult {
  const tabData = convertTabDataFromPrivate(data.tabData);
  const pdfDocumentData = data.pdfDocumentData &&
      convertPdfDocumentDataFromPrivate(data.pdfDocumentData);
  return replaceProperties(data, {tabData, pdfDocumentData});
}

class ObservableSubscription<T> implements Subscriber {
  constructor(
      public onChange: (newValue: T) => void,
      private onUnsubscribe: (self: ObservableSubscription<T>) => void) {}

  unsubscribe(): void {
    this.onUnsubscribe(this);
  }
}

class ObservableValue<T> implements Observable<T> {
  private subscribers: Set<ObservableSubscription<T>> = new Set();
  constructor(private value: T) {}

  assignAndSignal(v: T) {
    this.value = v;
    this.subscribers.forEach((sub) => {
      // Ignore if removed since forEach was called.
      if (this.subscribers.has(sub)) {
        sub.onChange(v);
      }
    });
  }

  // Observable impl.
  getValue(): T {
    return this.value;
  }

  subscribe(change: (newValue: T) => void): Subscriber {
    const newSub = new ObservableSubscription(
        change, (sub) => this.subscribers.delete(sub));
    this.subscribers.add(newSub);
    return newSub;
  }
}
