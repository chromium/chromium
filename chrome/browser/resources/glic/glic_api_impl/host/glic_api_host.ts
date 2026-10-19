// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in ../client/.

import {enumToClient} from '../../enum_conversions.js';
import {WebClientHandlerRemote} from '../../glic.mojom-webui.js';
import type {WebClientInitialState} from '../../glic.mojom-webui.js';
import {ClientCapabilities} from '../../glic_api/glic_api.js';
import {TaskQueue} from '../../task_queue.js';
import {maybeWrapWithLogging} from '../mojo_logging.js';

import {conversionSettings, urlFromClient} from './conversions.js';
import {PanelOpenState} from './types.js';

/**
 * The host side of the Glic API.
 */
export class GlicApiHost {
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

  private isDestroyed = false;

  constructor() {
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
}
