// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserHost, GlicHostRegistry, GlicWebClient} from '../glic_api/glic_api.js';

import {PostMessageRequestReceiver, PostMessageRequestSender} from './post_message_transport.js';
import type {WebClientRequestTypes} from './request_types.js';

// Web client side of the Glic API.
// Communicates with the Chrome-WebUI-side in
// ../glic_api_host.ts

class GlicHostRegistryImpl implements GlicHostRegistry {
  constructor(private windowProxy: WindowProxy) {}
  registerWebClient(webClient: GlicWebClient): Promise<void> {
    const host = new GlicBrowserHostImpl(webClient, this.windowProxy);
    return webClient.initialize(host);
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

  async glicWebClientNotifyPanelOpened(request: {
    dockedToWindowId: string|undefined,
  }) {
    if (this.webClient.notifyPanelOpened) {
      return this.webClient.notifyPanelOpened(request.dockedToWindowId);
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
    return this.sender.requestWithResponse('glicBrowserGetChromeVersion', {});
  }
}

export function boot(windowProxy: WindowProxy): GlicHostRegistry {
  return new GlicHostRegistryImpl(windowProxy);
}
