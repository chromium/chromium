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

// Proxy class to control post messages received from the remote UI.
export class PostMessageReceiver {
  private eventTracker: EventTracker = new EventTracker();
  private sidePanelProxy: SidePanelBrowserProxy;
  private resultsFrame: HTMLIFrameElement;
  private listenerIds: number[] = [];
  private resultsSearchUrl: URL =
      new URL(loadTimeData.getString('resultsSearchURL'));

  constructor(
      sidePanelProxy: SidePanelBrowserProxy, resultsFrame: HTMLIFrameElement) {
    this.sidePanelProxy = sidePanelProxy;
    this.resultsFrame = resultsFrame;

    // Listen to message events on the window.
    this.eventTracker.add(window, 'message', this.onMessageReceived.bind(this));
    this.listenerIds.push(
        this.sidePanelProxy.callbackRouter.sendClientMessageToAim.addListener(
            this.onSendClientMessageToAim.bind(this)));
  }

  // Stop listening to message events.
  detach() {
    this.eventTracker.removeAll();
    this.listenerIds.forEach(
        id => assert(this.sidePanelProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
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

    const data = JSON.parse(event.data);
    const messageType = data[ParamType.MESSAGE_TYPE];
    if (messageType === MessageType.SCROLL_TO) {
      this.sidePanelProxy.handler.onScrollToMessage(
          data[ParamType.TEXT_FRAGMENTS], data[ParamType.PDF_PAGE_NUMBER]);
      return;
    }
  }
}
