// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

// Attaches the X-Glic headers to all main-frame requests.
// X-Glic: 1
// X-Glic-Chrome-Channel: stable
// X-Glic-Chrome-Version: 137.0.1234.0
export class GlicRequestHeaderInjector {
  private onDestroy: () => void = () => {};
  constructor(
      webview: chrome.webviewTag.WebView, private chromeVersion: string,
      private chromeChannel: string, requestTypes: string) {
    if (requestTypes === '') {
      return;
    }
    const onBeforeSendHeaders = this.onBeforeSendHeaders.bind(this);
    webview.request.onBeforeSendHeaders.addListener(
        onBeforeSendHeaders, {
          // These should be valid values from web_request.d.ts.
          types: requestTypes.split(',') as chrome.webRequest.ResourceType[],
          urls: ['<all_urls>'],
        },
        ['blocking', 'requestHeaders', 'extraHeaders']);

    this.onDestroy = () => {
      webview.request.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
    };
  }

  destroy() {
    this.onDestroy();
  }

  private onBeforeSendHeaders:
      ChromeEventFunctionType<typeof chrome.webRequest.onBeforeSendHeaders> =
          (details) => {
            // Ignore subframe requests.
            if (details.frameId !== 0) {
              return {};
            }
            const requestHeaders = details.requestHeaders || [];
            requestHeaders.push({
              name: 'X-Glic',
              value: '1',
            });
            requestHeaders.push({
              name: 'X-Glic-Chrome-Version',
              value: this.chromeVersion,
            });
            requestHeaders.push({
              name: 'X-Glic-Chrome-Channel',
              value: this.chromeChannel,
            });
            return {requestHeaders};
          };
}

type ChromeEventFunctionType<T> =
    T extends ChromeEvent<infer ListenerType>? ListenerType : never;
