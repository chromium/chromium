// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {BrowserProxyImpl} from './browser_proxy.js';
import {GlicApiHost} from './glic_api_impl/glic_api_host.js';

const transitionDuration = {
  microseconds: BigInt(100000),
};

// Time to wait before showing loading panel.
const kPreHoldLoadingTimeMs = loadTimeData.getInteger('preLoadingTimeMs');

// Minimum time to hold "loading" panel visible.
const kMinHoldLoadingTimeMs = loadTimeData.getInteger('minLoadingTimeMs');

// Maximum time to wait for load before showing error panel.
const kMaxWaitTimeMs = loadTimeData.getInteger('maxLoadingTimeMs');

interface PageElementTypes {
  panelContainer: HTMLElement;
  loadingPanel: HTMLElement;
  offlinePanel: HTMLElement;
  errorPanel: HTMLElement;
  guestPanel: chrome.webviewTag.WebView;
}

const $: PageElementTypes = new Proxy({}, {
  get(_target: any, prop: string) {
    return getRequiredElement(prop);
  },
});

type PanelId = 'loadingPanel'|'guestPanel'|'offlinePanel'|'errorPanel';

export class GlicAppController {
  preLoadingTimer: number|undefined;
  minHoldTimer: number|undefined;
  maxWaitTimer: number|undefined;
  webViewLoaded: boolean = false;

  // Last seen width and height of guest panel.
  lastWidth: number = 400;
  lastHeight: number = 80;

  // Set to true when the panel has been opened by the browser at least once.
  guestPanelOpened: boolean = false;

  host: GlicApiHost|undefined;

  // Created from constructor and never null since the destructor replaces it
  // with an empty <webview>.
  webview: chrome.webviewTag.WebView;

  constructor(private browserProxy: BrowserProxyImpl) {
    // Bind event listener functions so that they can be used and removed when
    // needed.
    this.onLoadCommit = this.onLoadCommit.bind(this);
    this.contentLoaded = this.contentLoaded.bind(this);
    this.onNewWindow = this.onNewWindow.bind(this);
    this.onPermissionRequest = this.onPermissionRequest.bind(this);

    this.webview = this.createWebView();

    this.updateOnlineState(navigator.onLine);
    window.addEventListener('online', () => {
      this.updateOnlineState(true);
    });
    window.addEventListener('offline', () => {
      this.updateOnlineState(false);
    });

    this.preLoadingTimer = setTimeout(() => {
      this.showLoading();
    }, kPreHoldLoadingTimeMs);
  }

  onLoadCommit(e: any): void {
    this.loadCommit(e.url, e.isTopLevel);
  }

  onNewWindow(e: Event): void {
    this.onNewWindowEvent(e as chrome.webviewTag.NewWindowEvent);
  }

  onPermissionRequest(e: any): void {
    if (e.permission === 'media' || e.permission === 'geolocation') {
      e.request.allow();
    }
  }

  createWebView(): chrome.webviewTag.WebView {
    this.webViewLoaded = false;
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;
    webview.id = 'guestPanel';
    webview.setAttribute('partition', 'persist:glicpart');
    webview.setAttribute('class', 'panel');
    webview.hidden = true;
    $.panelContainer.appendChild(webview);

    webview.addEventListener('loadcommit', this.onLoadCommit);
    webview.addEventListener('contentload', this.contentLoaded);
    webview.addEventListener('newwindow', this.onNewWindow);
    webview.addEventListener('permissionrequest', this.onPermissionRequest);

    return webview;
  }

  onNewWindowEvent(event: chrome.webviewTag.NewWindowEvent) {
    if (!this.host) {
      return;
    }
    event.preventDefault();
    this.host.openLinkInNewTab(event.targetUrl);
    event.stopPropagation();
  }

  loadCommit(url: string, isTopLevel: boolean) {
    if (!isTopLevel) {
      return;
    }
    if (this.host) {
      this.host.destroy();
      this.host = undefined;
    }
    if (this.webview.contentWindow) {
      this.host = new GlicApiHost(
          this.browserProxy, this.webview.contentWindow, new URL(url).origin,
          this);
    }
    this.browserProxy.handler.webviewCommitted({url});

    // TODO(https://crbug.com/388328847): Remove when login issues are resolved.
    if (url.startsWith('https://login.corp.google.com/') ||
        url.startsWith('https://accounts.google.com/')) {
      this.showLogin();
    }
  }

  contentLoaded() {
    if (this.host) {
      this.host.contentLoaded();
    }
  }

  // Show one webUI panel and hide the others. The widget is resized to fit the
  // newly-visible content. If the guest panel is now visible, then its size
  // will be determined by the most recent resize request.
  showPanel(id: PanelId): void {
    for (const panel of document.querySelectorAll<HTMLElement>('.panel')) {
      panel.hidden = panel.id !== id;
    }
    // Resize widget to size of new panel.
    if (id === 'guestPanel') {
      // For the guest webview, use the most recently requested size.
      this.browserProxy.handler.resizeWidget(
          {width: this.lastWidth, height: this.lastHeight}, transitionDuration);
    } else {
      const newRect = $[id].getBoundingClientRect();
      this.browserProxy.handler.resizeWidget(
          {width: newRect.width, height: newRect.height}, transitionDuration);
    }
  }

  beginLoadingSequence(maxWaitTimeMs?: number): void {
    // Blocking on cookie syncing here introduces latency, we should consider
    // ways to avoid it.
    this.browserProxy.handler.syncWebviewCookies().then(() => {
      // Load the web client only after cookie sync is complete.
      this.webview!.src = loadTimeData.getString('glicGuestURL');
      this.showLoading(maxWaitTimeMs);
    });
  }

  // Destroy the current webview and create a new one. This is necessary because
  // webview does not support unloading content by setting src=""
  destroyWebview(): void {
    if (this.host) {
      this.host.destroy();
      this.host = undefined;
    }

    this.webview.removeEventListener('loadcommit', this.onLoadCommit);
    this.webview.removeEventListener('contentload', this.contentLoaded);
    this.webview.removeEventListener('newwindow', this.onNewWindow);
    this.webview.removeEventListener(
        'permissionrequest', this.onPermissionRequest);

    $.panelContainer.removeChild(this.webview);

    this.webview = this.createWebView();
  }

  updateOnlineState(online: boolean): void {
    if (this.webViewLoaded) {
      return;
    }
    if (online) {
      this.beginLoadingSequence();
    } else {
      clearTimeout(this.maxWaitTimer);
      this.maxWaitTimer = undefined;
      this.destroyWebview();
      this.showPanel('offlinePanel');
    }
  }

  onGuestResizeRequest(request: {width: number, height: number}) {
    // Save most recently requested guest window size.
    this.lastWidth = request.width;
    this.lastHeight = request.height;
  }

  showLoading(maxWaitTimeMs?: number): void {
    this.minHoldTimer = setTimeout(() => {
      this.minHoldTimer = undefined;
      if (this.webViewLoaded) {
        this.showGuest();
      }
    }, kMinHoldLoadingTimeMs);
    this.maxWaitTimer = setTimeout(() => {
      this.maxWaitTimer = undefined;
      if (!this.webViewLoaded) {
        this.destroyWebview();
        this.showPanel('errorPanel');
      }
    }, maxWaitTimeMs ?? kMaxWaitTimeMs);

    this.showPanel('loadingPanel');
  }

  // Called when the notifyPanelWillOpen promise resolves to open the panel
  // when triggered from the browser.
  openGuestPanel(): void {
    this.guestPanelOpened = true;
    this.showGuest();
  }

  // This may also be called when the panel is re-opened by webui after being
  // hidden, such as when an error panel is shown.
  // This will do nothing if openGuestPanel has not been called at least once.
  showGuest(): void {
    if (this.guestPanelOpened) {
      clearTimeout(this.maxWaitTimer);
      this.maxWaitTimer = undefined;
      this.webViewLoaded = true;
      // Wait for at least one loading animation cycle
      if (!this.minHoldTimer) {
        this.showPanel('guestPanel');
      }
    }
  }

  // TODO(https://crbug.com/388328847): Remove when login issues are resolved.
  showLogin(): void {
    this.lastWidth = 400;
    this.lastHeight = 800;
    this.openGuestPanel();
  }
}
