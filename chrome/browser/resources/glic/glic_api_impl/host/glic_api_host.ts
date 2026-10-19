// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in ../client/.

import {enumToClient} from '../../enum_conversions.js';
import {GlicRequestEvent as MojomGlicRequestEvent, WebClientHandlerRemote} from '../../glic.mojom-webui.js';
import type {WebClientInitialState} from '../../glic.mojom-webui.js';
import {ClientCapabilities} from '../../glic_api/glic_api.js';
import {TaskQueue} from '../../task_queue.js';
import {maybeWrapWithLogging} from '../mojo_logging.js';
import {getHostRequestHistogramInfo} from '../request_types.js';
import type {WebClient} from '../request_types.js';
import type {ResponseExtras} from '../transport/messaging.js';
import type {                              //
             InterfaceDef,                 //
             PostMessageLifecycleObserver, //
             PostMessageRemote,            //
             PostMessageRouter,            //
} from '../transport/post_message_transport.js';

import {conversionSettings, urlFromClient} from './conversions.js';
import {PanelOpenState} from './types.js';

type HandlerFunction = (payload: unknown, extras: ResponseExtras) =>
    Promise<unknown>;

/**
 * The host side of the Glic API.
 *
 * Its primary job is to route calls between the client (over postMessage) and
 * the browser (over Mojo).
 */
export class GlicApiHost implements PostMessageLifecycleObserver {
  sender: PostMessageRemote<WebClient>;
  panelIsActive = false;

  readonly handler: WebClientHandlerRemote;
  get handlerForTesting(): WebClientHandlerRemote {
    return this.handler;
  }
  openCloseTasks = new TaskQueue();
  private waitingOnPanelWillOpenValue = false;
  // The open state as understood by the client, this is delayed
  // from the notifyPanelWillOpen and notifyPanelWasClosed calls because
  // processing is async.
  private panelOpenState = PanelOpenState.CLOSED;
  private instanceIsActive = true;

  readonly router: PostMessageRouter;
  private isDestroyed = false;

  constructor(
      hostRemote: PostMessageRemote<WebClient>,
      hostRouter: PostMessageRouter,
  ) {
    this.router = hostRouter;
    this.sender = hostRemote;
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

    if (this.router.receiver) {
      this.router.receiver.requestObserver = this;
      this.router.receiver.setHandlerWrapper(this.handlerWrapper.bind(this));
    }
  }

  destroy() {
    this.isDestroyed = true;
  }

  setInitialState(
      initialState: WebClientInitialState,
      clientCapabilities: Set<ClientCapabilities>): void {
    this.panelIsActive = initialState.panelIsActive;
    this.instanceIsActive = initialState.instanceIsActive;
    conversionSettings.platform = enumToClient(initialState.platform);
    conversionSettings.omitFaviconInTabData =
        clientCapabilities.has(ClientCapabilities.IGNORES_TAB_DATA_FAVICONS);
  }

  waitingOnPanelWillOpen() {
    return this.waitingOnPanelWillOpenValue;
  }

  setWaitingOnPanelWillOpen(value: boolean): void {
    this.waitingOnPanelWillOpenValue = value;
  }

  panelOpenStateChanged(state: PanelOpenState) {
    this.panelOpenState = state;
  }

  setInstanceIsActive(instanceIsActive: boolean) {
    this.instanceIsActive = instanceIsActive;
  }

  getInstanceIsActive(): boolean {
    return this.instanceIsActive;
  }

  isPanelOpen(): boolean {
    return this.panelOpenState === PanelOpenState.OPEN;
  }

  openLinkInPopup(url: string, initialWidth: number, initialHeight: number) {
    this.handler.openLinkInPopup(
        urlFromClient(url), initialWidth, initialHeight);
  }

  async openLinkInNewTab(url: string) {
    await this.handler.createTab(urlFromClient(url), {
      openInBackground: false,
      windowId: null,
    });
  }

  async handlerWrapper(
      type: string, interfaceDef: InterfaceDef|undefined, payload: unknown,
      extras: ResponseExtras,
      handlerFunction: HandlerFunction): Promise<unknown> {
    const startTime = performance.now();
    const response = await handlerFunction(payload, extras);
    if (response) {
      // Report latency metric for handled requests that return a response.
      const latency = performance.now() - startTime;
      this.reportLatency(type, interfaceDef, latency);
    }
    // Not all request types require a return value.
    return response;
  }

  onRequestReceived(type: string, interfaceDef: InterfaceDef|undefined): void {
    this.reportRequestCountEvent(
        type, interfaceDef, GlicRequestEvent.REQUEST_RECEIVED);
    if (!this.panelIsActive) {
      this.reportRequestCountEvent(
          type, interfaceDef, GlicRequestEvent.REQUEST_RECEIVED_WHILE_INACTIVE);
    }
  }

  onRequestHandlerException(type: string, interfaceDef: InterfaceDef|undefined):
      void {
    this.reportRequestCountEvent(
        type, interfaceDef, GlicRequestEvent.REQUEST_HANDLER_EXCEPTION);
  }

  onRequestCompleted(type: string, interfaceDef: InterfaceDef|undefined): void {
    this.reportRequestCountEvent(
        type, interfaceDef, GlicRequestEvent.RESPONSE_SENT);
  }

  reportRequestCountEvent(
      requestType: string, interfaceDef: InterfaceDef|undefined,
      event: GlicRequestEvent) {
    const histogramInfo =
        getHostRequestHistogramInfo(requestType, interfaceDef);
    if (histogramInfo === undefined) {
      return;
    }
    let mojoEvent = MojomGlicRequestEvent.kRequestReceived;
    switch (event) {
      case GlicRequestEvent.REQUEST_RECEIVED:
        mojoEvent = MojomGlicRequestEvent.kRequestReceived;
        break;
      case GlicRequestEvent.RESPONSE_SENT:
        mojoEvent = MojomGlicRequestEvent.kResponseSent;
        break;
      case GlicRequestEvent.REQUEST_HANDLER_EXCEPTION:
        mojoEvent = MojomGlicRequestEvent.kRequestHandlerException;
        break;
      case GlicRequestEvent.REQUEST_RECEIVED_WHILE_INACTIVE:
        mojoEvent = MojomGlicRequestEvent.kRequestReceivedWhileInactive;
        break;
      default:
        return;
    }
    try {
      this.handler.reportApiRequestCount(histogramInfo.id, mojoEvent);
    } catch (e) {
      console.error('[reportApiRequestCount ERROR]', e);
    }
  }

  reportLatency(
      requestType: string, interfaceDef: InterfaceDef|undefined,
      latencyMs: number) {
    const histogramInfo =
        getHostRequestHistogramInfo(requestType, interfaceDef);
    if (histogramInfo === undefined) {
      return;
    }
    try {
      this.handler.reportApiRequestLatency(
          histogramInfo.name,
          {microseconds: BigInt(Math.round(latencyMs * 1000))});
    } catch (e) {
      console.error('[reportApiRequestLatency ERROR]', e);
    }
  }
}

// LINT.IfChange(GlicRequestEvent)
enum GlicRequestEvent {
  REQUEST_RECEIVED = 0,
  RESPONSE_SENT = 1,
  REQUEST_HANDLER_EXCEPTION = 2,
  // Deprecated: REQUEST_RECEIVED_WHILE_HIDDEN = 3,
  REQUEST_RECEIVED_WHILE_INACTIVE = 4,
  MAX_VALUE = REQUEST_RECEIVED_WHILE_INACTIVE,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicRequestEvent)
