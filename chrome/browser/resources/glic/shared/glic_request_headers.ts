// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="not enable_extensions_core">
import {OnBeforeSendHeadersParams} from '/shared/guest_view/request_throttlers.js';
// </if>
import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

import {isFullWebView} from '../shared/web_view_type.js';
import type {WebViewType} from '../shared/web_view_type.js';

// Attaches the X-Glic headers to all main-frame requests.
// X-Glic: 1
// X-Glic-Chrome-Channel: stable
// X-Glic-Chrome-Version: 137.0.1234.0
export class GlicRequestHeaderInjector {
  private onDestroy: () => void = () => {};
  constructor(
      webview: WebViewType, private chromeVersion: string,
      private chromeChannel: string, requestTypes: string) {
    if (requestTypes === '') {
      return;
    }
    if (isFullWebView(webview)) {
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
    } else {
      // <if expr="not enable_extensions_core">
      webview.onBeforeSendHeadersParams = new OnBeforeSendHeadersParams(
          requestTypes.split(','),
          /* includeSubFrameRequests= */ false, this.headers());
      // </if>
    }
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
            requestHeaders.push(...this.headers());
            return {requestHeaders};
          };

  private headers() {
    return [
      {
        name: 'X-Glic',
        value: '1',
      },
      {
        name: 'X-Glic-Chrome-Version',
        value: this.chromeVersion,
      },
      {
        name: 'X-Glic-Chrome-Channel',
        value: this.chromeChannel,
      },
    ];
  }
}

type ChromeEventFunctionType<T> =
    T extends ChromeEvent<infer ListenerType>? ListenerType : never;
