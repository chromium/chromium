// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

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

  constructor(sidePanelProxy: SidePanelBrowserProxy) {
    this.sidePanelProxy = sidePanelProxy;
  }

  // Being listening to message events on the window.
  listen() {
    if (loadTimeData.getBoolean('scrollToEnabled')) {
      this.eventTracker.add(window, 'message', this.onMessage.bind(this));
    }
  }

  // Stop listening to message events.
  detach() {
    this.eventTracker.removeAll();
  }

  private onMessage(event: MessageEvent) {
    const originUrl = new URL(loadTimeData.getString('resultsSearchURL'));
    if (event.origin !== originUrl.origin) {
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
