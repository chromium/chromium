// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxyImpl} from './browser_proxy.js';

const transitionDuration = {
  microseconds: BigInt(100000),
};

export class GlicAppController {
  // Last seen width and height of guest panel.
  lastWidth: number = 400;
  lastHeight: number = 80;

  constructor(private browserProxy: BrowserProxyImpl) {
    this.updateOnlineState(navigator.onLine);
    window.addEventListener('online', () => {
      this.updateOnlineState(true);
    });
    window.addEventListener('offline', () => {
      this.updateOnlineState(false);
    });
  }

  updateOnlineState(state: boolean): void {
    const webview =
        document.getElementById('guest-frame') as chrome.webviewTag.WebView;
    if (state) {
      // Blocking on cookie syncing here introduces latency, we should consider
      // ways to avoid it.
      this.browserProxy.handler.syncWebviewCookies().then(() => {
        // Load the web client only after cookie sync is complete.
        webview!.src = loadTimeData.getString('glicGuestURL');
        this.showGuest();
        this.browserProxy.handler.resizeWidget(
            {width: this.lastWidth, height: this.lastHeight},
            transitionDuration);
      });
    } else {
      const offlinePanel = document.getElementById('offline-panel');
      offlinePanel!.classList.remove('hidden');
      const newRect = offlinePanel!.getBoundingClientRect();
      webview!.classList.add('hidden');
      webview!.removeAttribute('src');
      this.browserProxy.handler.resizeWidget(
          {width: newRect.width, height: newRect.height}, transitionDuration);
    }
  }

  onGuestResizeRequest(request: {width: number, height: number}) {
    // Save most recently requested guest window size.
    this.lastWidth = request.width;
    this.lastHeight = request.height;
  }

  showGuest(): void {
    document.getElementById('offline-panel')?.classList.add('hidden');
    document.getElementById('guest-frame')?.classList.remove('hidden');
  }

  // TODO(https://crbug.com/388328847): Remove when login issues are resolved.
  showLogin(): void {
    this.browserProxy.handler.resizeWidget(
        {width: 400, height: 800}, transitionDuration);
    this.showGuest();
  }
}
