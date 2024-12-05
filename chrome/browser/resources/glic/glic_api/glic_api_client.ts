// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserHost, GlicHostRegistry, GlicWebClient, TabContextResult, TabData} from '../glic_api/glic_api.js';

import {PostMessageRequestReceiver, PostMessageRequestSender} from './post_message_transport.js';
import type {WebClientRequestTypes} from './request_types.js';

// Web client side of the Glic API.
// Communicates with the Chrome-WebUI-side in glic_api_host.ts

class GlicHostRegistryImpl implements GlicHostRegistry {
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
  constructor(private webClient: GlicWebClient) {}

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
}

class GlicBrowserHostImpl implements GlicBrowserHost {
  private sender: PostMessageRequestSender;
  private receiver: PostMessageRequestReceiver;
  private handlerFunctionNames: Set<string> = new Set();
  private webClientMessageHandler: WebClientMessageHandler;

  constructor(private webClient: GlicWebClient, windowProxy: WindowProxy) {
    this.sender = new PostMessageRequestSender(windowProxy, 'chrome://glic');
    this.receiver =
        new PostMessageRequestReceiver('chrome://glic', windowProxy, this);
    this.webClientMessageHandler = new WebClientMessageHandler(this.webClient);

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
    return result.tabData;
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
      throw new Error('getContextFromFocusedTab: failed');
    }
    return context.tabContextResult;
  }

  async resizeWindow(width: number, height: number) {
    return this.sender.requestWithResponse(
        'glicBrowserResizeWindow', {width, height});
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

// DEPRECATED: Provided for the old code-injected style of booting the API.
// Supports the test client. May be removed in the future.
export function boot(windowProxy: WindowProxy): GlicHostRegistry {
  return new GlicHostRegistryImpl(windowProxy);
}
