// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {SidePanelBrowserProxy} from './side_panel_browser_proxy.js';

// Type of message to handle from the remote UI.
export enum MessageType {
  SCROLL_TO = 0,
}

// The parameters for values that are returned in the payload.
export enum ParamType {
  MESSAGE_TYPE = 'type',
  TEXT_FRAGMENTS = 'textFragments',
  PDF_PAGE_NUMBER = 'pdfPageNumber',
}

const HANDSHAKE_INTERVAL_MS = 500;

// Proxy class to control post messages received from the remote UI.
export class PostMessageReceiver {
  private eventTracker: EventTracker = new EventTracker();
  private sidePanelProxy: SidePanelBrowserProxy;
  private resultsFrame: HTMLIFrameElement;
  private listenerIds: number[] = [];
  private resultsSearchUrl: URL =
      new URL(loadTimeData.getString('resultsSearchURL'));
  private handshakeMessage: Uint8Array =
      new TextEncoder().encode(loadTimeData.getString('handshakeMessage'));
  private handshakeIntervalId: number|null = null;
  private isAimSearchboxEnabled: boolean =
      loadTimeData.getBoolean('enableAimSearchbox');

  constructor(
      sidePanelProxy: SidePanelBrowserProxy, resultsFrame: HTMLIFrameElement) {
    this.sidePanelProxy = sidePanelProxy;
    this.resultsFrame = resultsFrame;

    // Listen to message events on the window.
    this.eventTracker.add(window, 'message', this.onMessageReceived.bind(this));
    this.listenerIds = [
      this.sidePanelProxy.callbackRouter.sendClientMessageToAim.addListener(
          this.onSendClientMessageToAim.bind(this)),
      this.sidePanelProxy.callbackRouter.aimHandshakeReceived.addListener(
          this.onAimHandshakeReceived.bind(this)),
    ];

    // Begin sending the handshake message until the remote UI acknowledges.
    this.queueHandshake();
  }

  // Stop listening to message events.
  detach() {
    this.eventTracker.removeAll();
    this.listenerIds.forEach(
        id => assert(this.sidePanelProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];

    // Stop sending the handshake message if it's still running.
    this.stopAimHandshake();
  }

  private queueHandshake() {
    // Only send the handshake message if the AIM searchbox is enabled.
    if (!this.isAimSearchboxEnabled) {
      return;
    }
    // Wait for the iframe to load before sending the handshake message.
    // Also will send a new handshake message if the iframe is reloaded.
    this.eventTracker.add(this.resultsFrame, 'load', () => {
      this.handshakeIntervalId = setInterval(() => {
        this.onSendClientMessageToAim(this.handshakeMessage);
      }, HANDSHAKE_INTERVAL_MS);
    });
  }

  // Sends the message to the remote UI. Message should be a serialized proto,
  // or some other format that the remote UI can handle.
  private onSendClientMessageToAim(serializedMessage: Uint8Array) {
    const contentWindow = this.resultsFrame.contentWindow;
    assert(contentWindow);
    contentWindow.postMessage(serializedMessage, this.resultsSearchUrl.origin);
  }

  private onMessageReceived(event: MessageEvent) {
    // Only handle messages from the results frame.
    if (event.origin !== this.resultsSearchUrl.origin) {
      return;
    }

    // The scrollTo message is a JSON object with the message type and
    // parameters. If JSON parsing fails, assume it's a AIM message and pass it
    // to the remote UI to serialize.
    try {
      const data = JSON.parse(event.data);
      const messageType = data[ParamType.MESSAGE_TYPE];
      if (messageType === MessageType.SCROLL_TO) {
        this.sidePanelProxy.handler.onScrollToMessage(
            data[ParamType.TEXT_FRAGMENTS], data[ParamType.PDF_PAGE_NUMBER]);
        return;
      }
    } catch (e) {
      this.onAimMessage(event.data);
    }
  }

  private onAimMessage(message: Uint8Array) {
    // Only handle messages if the AIM searchbox is enabled.
    if (!this.isAimSearchboxEnabled) {
      return;
    }
    // Pass the message to the browser to handle. Array.from is used to convert
    // the Uint8Array to a normal number JS array which mojo expects.
    this.sidePanelProxy.handler.onAimMessage(Array.from(message));
  }

  private onAimHandshakeReceived() {
    this.stopAimHandshake();
  }

  private stopAimHandshake() {
    // Stop sending the handshake message.
    if (this.handshakeIntervalId) {
      clearInterval(this.handshakeIntervalId);
      this.handshakeIntervalId = null;
    }
  }
}
