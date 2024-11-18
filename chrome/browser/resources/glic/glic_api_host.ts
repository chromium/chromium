// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in
// glic_api_host/glic_api_impl.ts.

// TODO(crbug.com/379677413): Add tests for the API host.

import type {PostMessageRequestHandler} from '//glic/glic_api_host/post_message_transport.js';
import {PostMessageRequestReceiver} from '//glic/glic_api_host/post_message_transport.js';
import type {HostRequestTypes} from '//glic/glic_api_host/request_types.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxy} from './browser_proxy.js';
import type {PageHandlerInterface} from './glic.mojom-webui.js';

// Turn everything except void into a promise.
type Promisify<T> = T extends void ? void : Promise<T>;

// A type which the host should implement. This helps verify that
// `HostMessageHandler` is implemented with the correct parameter and return
// types.
type HostMessageHandlerInterface = {
  [Property in keyof HostRequestTypes]:
      // `payload` is the message payload.
      // `responseTransfer` is populated by objects that should be transferred
      // when sending the message.
  (payload: HostRequestTypes[Property]['request'],
   responseTransfer: Transferable[]) =>
      Promisify<HostRequestTypes[Property]['response']>;
};

// Handles all requests to the host.
class HostMessageHandler implements HostMessageHandlerInterface {
  constructor(private handler: PageHandlerInterface) {}

  async glicBrowserGetChromeVersion() {
    const response = await this.handler.getChromeVersion();
    const c = response.version.components;
    return {
      major: c[0] || 0,
      minor: c[1] || 0,
      build: c[2] || 0,
      patch: c[3] || 0,
    };
  }
}

export class GlicApiHost implements PostMessageRequestHandler {
  private messageHandler: HostMessageHandler;
  private readonly postMessageReceiver: PostMessageRequestReceiver;

  constructor(
      private browserProxy: BrowserProxy, private windowProxy: WindowProxy,
      private embeddedOrigin: string) {
    this.postMessageReceiver =
        new PostMessageRequestReceiver(embeddedOrigin, windowProxy, this);
    this.messageHandler = new HostMessageHandler(this.browserProxy.handler);

    this.windowProxy.postMessage(
        {
          type: 'glic-bootstrap',
          glicApiSource: loadTimeData.getString('glicGuestAPISource'),
        },
        this.embeddedOrigin);
  }

  destroy() {
    this.postMessageReceiver.destroy();
  }

  async handleRawRequest(type: string, payload: any):
      Promise<{payload: any, transfer: Transferable[]}|undefined> {
    const handlerFunction = (this.messageHandler as any)[type];
    if (typeof handlerFunction !== 'function') {
      console.error(`GlicApiHost: Unknown message type ${type}`);
      return;
    }

    const transfer: Transferable[] = [];
    const response =
        await handlerFunction.call(this.messageHandler, payload, transfer);
    if (!response) {
      // Not all request types require a return value.
      return;
    }
    return {payload: response, transfer};
  }
}
