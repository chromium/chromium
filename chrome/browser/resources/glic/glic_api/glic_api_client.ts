// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DraggableArea, ErrorWithReason, GlicBrowserHost, GlicHostRegistry, GlicWebClient, Observable, PanelState, Subscriber, TabContextResult, TabData, UserProfileInfo} from '../glic_api/glic_api.js';
import {GetTabContextErrorReason, PanelStateKind} from '../glic_api/glic_api.js';

import {PostMessageRequestReceiver, PostMessageRequestSender} from './post_message_transport.js';
import type {RgbaImage, TabContextResultPrivate, TabDataPrivate, WebClientRequestTypes} from './request_types.js';
import {ImageAlphaType, ImageColorType} from './request_types.js';


// Web client side of the Glic API.
// Communicates with the Chrome-WebUI-side in glic_api_host.ts

export class GlicHostRegistryImpl implements GlicHostRegistry {
  constructor(private windowProxy: WindowProxy) {}

  async registerWebClient(webClient: GlicWebClient): Promise<void> {
    const host = new GlicBrowserHostImpl(webClient, this.windowProxy);
    await webClient.initialize(host);
    host.webClientInitialized();
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

  glicWebClientNotifyPanelOpened(payload: {dockedToWindowId: string|undefined}):
      void {
    if (this.webClient.notifyPanelOpened) {
      this.webClient.notifyPanelOpened(payload.dockedToWindowId);
    }
  }

  glicWebClientNotifyPanelClosed(): void {
    if (this.webClient.notifyPanelClosed) {
      this.webClient.notifyPanelClosed();
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
}

class GlicBrowserHostImpl implements GlicBrowserHost {
  private sender: PostMessageRequestSender;
  private receiver: PostMessageRequestReceiver;
  private handlerFunctionNames: Set<string> = new Set();
  private webClientMessageHandler: WebClientMessageHandler;
  private panelState = new ObservableValue<PanelState>({ kind: PanelStateKind.HIDDEN });
  private permissionStateMicrophone = new ObservableValue<boolean>(false);
  private permissionStateLocation = new ObservableValue<boolean>(false);
  private permissionStateTabContext = new ObservableValue<boolean>(false);

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

  webClientInitialized() {
    this.sender.requestNoResponse('glicBrowserWebClientInitialized', {});
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
    return this.sender.requestWithResponse('glicBrowserGetChromeVersion', {});
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

  closePanel(): Promise<void> {
    return this.sender.requestWithResponse('glicBrowserClosePanel', {});
  }

  async getContextFromFocusedTab(options: {
    innerText?: boolean|undefined,
    viewportScreenshot?: boolean|undefined,
  }): Promise<TabContextResult> {
    const context = await this.sender.requestWithResponse(
        'glicBrowserGetContextFromFocusedTab', {options});
    if (!context.tabContextResult) {
      throw new ErrorWithReasonImpl(
          'getContext failed',
          context.error || GetTabContextErrorReason.UNKNOWN);
    }
    return convertTabContextResultFromPrivate(context.tabContextResult);
  }

  async resizeWindow(width: number, height: number) {
    const result = await this.sender.requestWithResponse(
        'glicBrowserResizeWindow', {width, height});
    if (result.actualHeight !== undefined && result.actualWidth !== undefined) {
      return {
        actualWidth: result.actualWidth,
        actualHeight: result.actualHeight,
      };
    }
    throw new Error('Can\'t resize the widget while it\'s closed');
  }

  setWindowDraggableAreas(areas: DraggableArea[]) {
    return this.sender.requestWithResponse(
        'glicBrowserSetWindowDraggableAreas', {areas});
  }

  getPanelState(): ObservableValue<PanelState> {
    return this.panelState;
  }

  getMicrophonePermissionState(): ObservableValue<boolean> {
    return this.permissionStateMicrophone;
  }

  getLocationPermissionState(): ObservableValue<boolean> {
    return this.permissionStateLocation;
  }

  getTabContextPermissionState(): ObservableValue<boolean> {
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

  async getUserProfileInfo?(): Promise<UserProfileInfo> {
    const {profileInfo} = await this.sender.requestWithResponse(
        'glicBrowserGetUserProfileInfo', {});
    if (!profileInfo) {
      throw new Error('getUserProfileInfo failed');
    }
    const {displayName, email, avatarIconImage} = profileInfo;
    return {
      displayName,
      email,
      avatarIcon: async () =>
          avatarIconImage && rgbaImageToBlob(avatarIconImage),
    };
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

function convertTabDataFromPrivate(data: TabDataPrivate): TabData {
  const result = Object.assign({}, data) as TabData;
  if (data.rawFavicon) {
    const rawFavicon = data.rawFavicon;
    delete (result as any).rawFavicon;
    result.favicon = () => rgbaImageToBlob(rawFavicon);
  }
  return result;
}

function convertTabContextResultFromPrivate(data: TabContextResultPrivate):
    TabContextResult {
  const result = Object.assign({}, data) as TabContextResult;
  if (data.tabData) {
    result.tabData = convertTabDataFromPrivate(data.tabData);
  }
  return result;
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
