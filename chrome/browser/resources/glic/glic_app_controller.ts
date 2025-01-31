// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {BrowserProxyImpl} from './browser_proxy.js';
import {WebUiState} from './glic.mojom-webui.js';
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
  unavailablePanel: HTMLElement;
  guestPanel: chrome.webviewTag.WebView;
}

const $: PageElementTypes = new Proxy({}, {
  get(_target: any, prop: string) {
    return getRequiredElement(prop);
  },
});

type PanelId =
    'loadingPanel'|'guestPanel'|'offlinePanel'|'errorPanel'|'unavailablePanel';

interface StateDescriptor {
  onEnter?: () => void;
  onExit?: () => void;
}

export class GlicAppController {
  loadingTimer: number|undefined;

  // This is used to simulate no connection for tests.
  simulateNoConnection: boolean =
      loadTimeData.getBoolean('simulateNoConnection');

  // Last seen width and height of guest panel.
  lastWidth: number = 400;
  lastHeight: number = 80;

  host: GlicApiHost|undefined;

  // Created from constructor and never null since the destructor replaces it
  // with an empty <webview>.
  webview: chrome.webviewTag.WebView;

  state: WebUiState|undefined;

  // When entering loading state, this represents the earliest timestamp at
  // which the UI can transition to the ready state. This ensures that the
  // loading UI isn't just a brief flash on screen.
  earliestLoadingDismissTime: number|undefined;

  constructor(private browserProxy: BrowserProxyImpl) {
    // Bind event listener functions so that they can be used and removed when
    // needed.
    this.onLoadCommit = this.onLoadCommit.bind(this);
    this.contentLoaded = this.contentLoaded.bind(this);
    this.onNewWindow = this.onNewWindow.bind(this);
    this.onPermissionRequest = this.onPermissionRequest.bind(this);

    this.webview = this.createWebView();

    window.addEventListener('online', () => {
      this.online();
    });
    window.addEventListener('offline', () => {
      this.offline();
    });

    if (navigator.onLine && !this.simulateNoConnection) {
      this.setState(WebUiState.kBeginLoad);
    } else {
      this.setState(WebUiState.kOffline);
    }
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

  setState(newState: WebUiState): void {
    if (this.state === newState) {
      return;
    }
    if (this.state) {
      this.states.get(this.state)!.onExit?.call(this);
    }
    this.state = newState;
    this.states.get(this.state)!.onEnter?.call(this);
    this.browserProxy.handler.webUiStateChanged(this.state);
  }

  readonly states: Map<WebUiState, StateDescriptor> = new Map([
    [
      WebUiState.kBeginLoad,
      {onEnter: this.beginLoad, onExit: this.cancelTimeout},
    ],
    [
      WebUiState.kShowLoading,
      {onEnter: this.showLoading, onExit: this.cancelTimeout},
    ],
    [
      WebUiState.kHoldLoading,
      {onEnter: this.holdLoading, onExit: this.cancelTimeout},
    ],
    [
      WebUiState.kFinishLoading,
      {onEnter: this.finishLoading, onExit: this.cancelTimeout},
    ],
    [
      WebUiState.kError,
      {
        onEnter: () => {
          this.destroyWebview();
          this.showPanel('errorPanel');
        },
      },
    ],
    [
      WebUiState.kOffline,
      {
        onEnter: () => {
          this.destroyWebview();
          this.showPanel('offlinePanel');
        },
      },
    ],
    [
      WebUiState.kUnavailable,
      {
        onEnter: () => {
          this.destroyWebview();
          this.showPanel('unavailablePanel');
        },
      },
    ],
    [
      WebUiState.kReady,
      {
        onEnter: () => {
          this.showPanel('guestPanel');
        },
      },
    ],
  ]);

  cancelTimeout(): void {
    if (this.loadingTimer) {
      clearTimeout(this.loadingTimer);
      this.loadingTimer = undefined;
    }
  }

  beginLoad(): void {
    // Send this message but block on it only after webview cookies are synced
    // to minimize latency. Enabling state is checked only when going online.
    // This only applies when showing Glic in a tab (since the entry point
    // button is removed when disabled) so the mild inconsistency doesn't
    // matter.
    const enabledCheck = this.browserProxy.handler.isProfileEnabled();

    // Time to show the loading panel if the web client is not ready.
    const showLoadingTime = performance.now() + kPreHoldLoadingTimeMs;

    // Blocking on cookie syncing here introduces latency, we should consider
    // ways to avoid it.
    this.browserProxy.handler.syncWebviewCookies().then(async () => {
      const isEnabled = (await enabledCheck).enabled;
      if (!isEnabled) {
        this.setState(WebUiState.kUnavailable);
        return;
      }

      // Load the web client only after cookie sync is complete.
      this.webview!.src = loadTimeData.getString('glicGuestURL');
      this.loadingTimer = setTimeout(() => {
        this.setState(WebUiState.kShowLoading);
      }, Math.max(0, showLoadingTime - performance.now()));
    });
  }

  showLoading(): void {
    this.showPanel('loadingPanel');
    // After kMinHoldLoadingTimeMs, transition to finish-loading or ready. Note
    // that we do not transition from show-loading to ready before the timeout.
    this.earliestLoadingDismissTime = performance.now() + kMinHoldLoadingTimeMs;
    this.loadingTimer = setTimeout(() => {
      this.setState(WebUiState.kFinishLoading);
    }, kMinHoldLoadingTimeMs);
  }

  holdLoading(): void {
    // The web client is ready, but we still wait for the remainder of
    // `kMinHoldLoadingTimeMs` before showing it, to allow the loading animation
    // to complete once.
    this.loadingTimer = setTimeout(() => {
      this.setState(WebUiState.kReady);
    }, Math.max(0, this.earliestLoadingDismissTime! - performance.now()));
  }

  finishLoading(): void {
    // The web client is not yet ready, so wait for the remainder of
    // `kMaxWaitTimeMs`. Switch to error state at that time unless interrupted
    // by `webClientReady`.
    this.loadingTimer = setTimeout(() => {
      this.setState(WebUiState.kError);
    }, kMaxWaitTimeMs - kMinHoldLoadingTimeMs);
  }

  reload(): void {
    this.destroyWebview();
    // TODO: Allow the timeout on this load to be longer than the initial load.
    this.setState(WebUiState.kBeginLoad);
  }

  createWebView(): chrome.webviewTag.WebView {
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
      this.lastWidth = 400;
      this.lastHeight = 800;
      this.setState(WebUiState.kReady);
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

  online(): void {
    if (this.simulateNoConnection) {
      return;
    }
    if (this.state !== WebUiState.kOffline) {
      return;
    }
    this.setState(WebUiState.kBeginLoad);
  }

  offline(): void {
    const allowedStates = [
      WebUiState.kBeginLoad,
      WebUiState.kShowLoading,
      WebUiState.kFinishLoading,
    ];
    if (allowedStates.includes(this.state!)) {
      this.setState(WebUiState.kOffline);
    }
  }

  // External entry points - these methods are called from WebClientImpl and
  // HostMessageHandler in glic_api_host.ts

  // Called when the web client requests that the window size be changed.
  onGuestResizeRequest(request: {width: number, height: number}) {
    // Save most recently requested guest window size.
    this.lastWidth = request.width;
    this.lastHeight = request.height;
  }

  // Called when the notifyPanelWillOpen promise resolves to open the panel
  // when triggered from the browser.
  webClientReady(): void {
    if (this.state === WebUiState.kBeginLoad ||
        this.state === WebUiState.kFinishLoading) {
      this.setState(WebUiState.kReady);
    } else if (this.state === WebUiState.kShowLoading) {
      this.setState(WebUiState.kHoldLoading);
    }
  }

  // This may also be called when the panel is re-opened by webui after being
  // hidden, such as when an error panel is shown.
  // This will do nothing if the app is not in kReady state.
  showGuest(): void {
    if (this.state === WebUiState.kReady) {
      this.showPanel('guestPanel');
    }
  }
}
