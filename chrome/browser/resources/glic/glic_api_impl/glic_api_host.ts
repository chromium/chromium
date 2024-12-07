// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in
// glic_api_host/glic_api_impl.ts.

// TODO(crbug.com/379677413): Add tests for the API host.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Origin} from '//resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {BrowserProxy} from '../browser_proxy.js';
import type {WebClientHandlerInterface, WebClientInterface} from '../glic.mojom-webui.js';
import {WebClientHandlerRemote, WebClientReceiver} from '../glic.mojom-webui.js';
import type {Screenshot, WebPageData} from '../glic_api/glic_api.js';
import type {PostMessageRequestHandler} from '../glic_api/post_message_transport.js';
import {PostMessageRequestReceiver, PostMessageRequestSender} from '../glic_api/post_message_transport.js';
import type {HostRequestTypes} from '../glic_api/request_types.js';


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

class WebClientImpl implements WebClientInterface {
  constructor(private sender: PostMessageRequestSender) {}

  notifyPanelOpened(dockedToWindowId: (number|null)): void {
    this.sender.requestNoResponse('glicWebClientNotifyPanelOpened', {
      dockedToWindowId: optionalWindowIdToClient(dockedToWindowId),
    });
  }

  async notifyPanelClosed(): Promise<void> {
    await this.sender.requestWithResponse('glicWebClientNotifyPanelClosed', {});
  }
}

// Handles all requests to the host.
class HostMessageHandler implements HostMessageHandlerInterface {
  // Undefined until the web client is initialized.
  private receiver: WebClientReceiver|undefined;
  constructor(
      private handler: WebClientHandlerInterface,
      private sender: PostMessageRequestSender) {}

  destroy() {
    if (this.receiver) {
      this.receiver.$.close();
    }
  }

  glicBrowserWebClientInitialized() {
    this.receiver = new WebClientReceiver(new WebClientImpl(this.sender));
    this.handler.webClientInitialized(
        this.receiver.$.bindNewPipeAndPassRemote());
  }

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

  async glicBrowserCreateTab(request: {
    url: string,
    options: {openInBackground?: boolean, windowId?: string},
  }) {
    const response = await this.handler.createTab(
        urlFromClient(request.url),
        request.options.openInBackground !== undefined ?
            request.options.openInBackground :
            false,
        optionalWindowIdFromClient(request.options.windowId));
    const tabData = response.tabData;
    if (tabData) {
      return {
        tabData: {
          tabId: tabIdToClient(tabData.tabId),
          windowId: windowIdToClient(tabData.windowId),
          url: urlToClient(tabData.url),
          title: optionalToClient(tabData.title),
        },
      };
    }
    return {};
  }

  glicBrowserClosePanel() {
    return this.handler.closePanel();
  }

  async glicBrowserGetContextFromFocusedTab(
      request: {
        options: {innerText?: boolean, viewportScreenshot?: boolean},
      },
      transfer: Transferable[]) {
    const response = await this.handler.getContextFromFocusedTab(
        request.options.innerText || false,
        // Note: viewportScreenshot was previously an empty object to imply
        // true, this code works for either. Can be replaced with
        // "request.options.viewportScreenshot || false" after 2025/01/05.
        request.options.viewportScreenshot ? true : false);
    const tabContextResult = response.tabContextResult;
    if (!tabContextResult) {
      return {};
    }
    const tabData = tabContextResult.tabData;
    if (!tabData) {
      return {};
    }
    const tabDataResult = {
      tabId: tabIdToClient(tabData.tabId),
      windowId: windowIdToClient(tabData.windowId),
      url: urlToClient(tabData.url),
      title: optionalToClient(tabData.title),
    };
    const webPageData = tabContextResult.webPageData;
    let webPageDataResult: WebPageData|undefined = undefined;
    if (webPageData) {
      webPageDataResult = {
        mainDocument: {
          origin: originToClient(webPageData.mainDocument.origin),
          innerText: webPageData.mainDocument.innerText,
        },
      };
    }
    const viewportScreenshot = tabContextResult.viewportScreenshot;
    let viewportScreenshotResult: Screenshot|undefined = undefined;
    if (viewportScreenshot) {
      const screenshotArray = new Uint8Array(viewportScreenshot.data);
      viewportScreenshotResult = {
        widthPixels: viewportScreenshot.widthPixels,
        heightPixels: viewportScreenshot.heightPixels,
        data: screenshotArray.buffer,
        mimeType: viewportScreenshot.mimeType,
        originAnnotations: {},
      };
      transfer.push(screenshotArray.buffer);
    }

    return {
      tabContextResult: {
        tabData: tabDataResult,
        webPageData: webPageDataResult,
        viewportScreenshot: viewportScreenshotResult,
      },
    };
  }

  async glicBrowserResizeWindow(
      resizeDimensions:
          HostRequestTypes['glicBrowserResizeWindow']['request']) {
    const response = await this.handler.resizeWidget(resizeDimensions);
    if (!response.actualSize) {
      throw new Error('Can\'t resize the widget while it\'s closed');
    }
    return {
      actualWidth: response.actualSize.width,
      actualHeight: response.actualSize.height,
    };
  }
}

export class GlicApiHost implements PostMessageRequestHandler {
  private messageHandler: HostMessageHandler;
  private readonly postMessageReceiver: PostMessageRequestReceiver;
  private sender: PostMessageRequestSender;
  private handler: WebClientHandlerRemote;
  private bootstrapPingIntervalId: number|undefined;
  constructor(
      private browserProxy: BrowserProxy, private windowProxy: WindowProxy,
      private embeddedOrigin: string) {
    this.postMessageReceiver =
        new PostMessageRequestReceiver(embeddedOrigin, windowProxy, this);
    this.sender = new PostMessageRequestSender(windowProxy, embeddedOrigin);
    this.handler = new WebClientHandlerRemote();
    this.browserProxy.handler.createWebClient(
        this.handler.$.bindNewPipeAndPassReceiver());
    this.messageHandler = new HostMessageHandler(this.handler, this.sender);

    this.bootstrapPingIntervalId =
        window.setInterval(this.bootstrapPing.bind(this), 50);
    this.bootstrapPing();
  }

  destroy() {
    window.clearInterval(this.bootstrapPingIntervalId);
    this.postMessageReceiver.destroy();
    this.messageHandler.destroy();
    this.sender.destroy();
  }

  // Called when the webview page is loaded.
  contentLoaded() {
    // Send the ping message one more time. At this point, the webview should
    // be able to handle the message, if it hasn't already.
    this.bootstrapPing();
    this.stopBootstrapPing();
  }

  // Sends a message to the webview which is required to initialize the client.
  // Because we don't know when the client will be ready to receive this
  // message, we start sending this every 50ms as soon as navigation commits on
  // the webview, and stop sending this when the page loads, or we receive a
  // request from the client.
  bootstrapPing() {
    if (this.bootstrapPingIntervalId === undefined) {
      return;
    }
    this.windowProxy.postMessage(
        {
          type: 'glic-bootstrap',
          glicApiSource: loadTimeData.getString('glicGuestAPISource'),
        },
        this.embeddedOrigin);
  }

  stopBootstrapPing() {
    if (this.bootstrapPingIntervalId !== undefined) {
      window.clearInterval(this.bootstrapPingIntervalId);
      this.bootstrapPingIntervalId = undefined;
    }
  }

  async openLinkInNewTab(url: string) {
    await this.handler.createTab(urlFromClient(url), false, null);
  }

  // PostMessageRequestHandler implementation.
  async handleRawRequest(type: string, payload: any):
      Promise<{payload: any, transfer: Transferable[]}|undefined> {
    const handlerFunction = (this.messageHandler as any)[type];
    if (typeof handlerFunction !== 'function') {
      console.error(`GlicApiHost: Unknown message type ${type}`);
      return;
    }

    this.stopBootstrapPing();
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


// Utility functions for converting from mojom types to message types.
// Summary of changes:
// * Window and tab IDs are sent using int32 in mojo, but made opaque
//   strings for the public API. This allows Chrome to change the ID
//   representation later.
// * Optional types in Mojo use null, but optional types in the public API use
//   undefined.
function windowIdToClient(windowId: number): string {
  return `${windowId}`;
}

function windowIdFromClient(windowId: string): number {
  return parseInt(windowId);
}

function tabIdToClient(tabId: number): string {
  return `${tabId}`;
}

function optionalWindowIdToClient(windowId: number|null): string|undefined {
  if (windowId === null) {
    return undefined;
  }
  return windowIdToClient(windowId);
}

function optionalWindowIdFromClient(windowId: string|undefined): number|null {
  if (windowId === undefined) {
    return null;
  }
  return windowIdFromClient(windowId);
}

function optionalToClient<T>(value: T|null) {
  if (value === null) {
    return undefined;
  }
  return value;
}

function urlToClient(url: Url): string {
  return url.url;
}

function urlFromClient(url: string): Url {
  return {url};
}

function originToClient(origin: Origin): string {
  if (!origin.scheme) {
    return '';
  }
  const originBase = `${origin.scheme}://${origin.host}`;
  if (origin.port) {
    return `${originBase}:${origin.port}`;
  }
  return originBase;
}
