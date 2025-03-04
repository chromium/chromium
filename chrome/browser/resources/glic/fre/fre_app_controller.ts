// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import {FrePageHandlerFactory, FrePageHandlerRemote, FreWebUiState} from './glic_fre.mojom-webui.js';

interface StateDescriptor {
  onEnter?: () => void;
  onExit?: () => void;
}

type PanelId = 'guestPanel'|'offlinePanel'|'errorPanel';

// Maximum time to wait for load before showing error panel.
const kMaxWaitTimeMs = 15000;

const freHandler = new FrePageHandlerRemote();
FrePageHandlerFactory.getRemote().createPageHandler(
    (freHandler).$.bindNewPipeAndPassReceiver());

export class FreAppController {
  state = FreWebUiState.kUninitialized;
  loadingTimer: number|undefined;

  // Created from constructor and never null since the destructor replaces it
  // with an empty <webview>.
  webview: chrome.webviewTag.WebView;

  constructor() {
    this.onLoadCommit = this.onLoadCommit.bind(this);
    this.contentLoaded = this.contentLoaded.bind(this);
    this.onNewWindow = this.onNewWindow.bind(this);

    this.webview = this.createWebView();
    this.setState(FreWebUiState.kBeginLoading);
  }

  createWebView(): chrome.webviewTag.WebView {
    const webview =
        document.getElementById('fre-guest-frame') as chrome.webviewTag.WebView;
    webview.addEventListener('loadcommit', this.onLoadCommit);
    webview.addEventListener('contentload', this.contentLoaded);
    webview.addEventListener('newwindow', this.onNewWindow);

    return webview;
  }

  onLoadCommit(e: any) {
    if (!e.isTopLevel) {
      return;
    }
    const url = new URL(e.url);
    const urlHash = url.hash;

    // Fragment navigations are used to represent actions taken in the web
    // client following this mapping: “Continue” button navigates to
    // glic/intro...#continue, “No thanks” button navigates to
    // glic/intro...#noThanks
    if (urlHash === '#continue') {
      freHandler.acceptFre();
    } else if (urlHash === '#noThanks') {
      freHandler.dismissFre();
    }
  }

  contentLoaded() {
    this.setState(FreWebUiState.kReady);
  }

  onNewWindow(e: any) {
    e.preventDefault();
    freHandler.validateAndOpenLinkInNewTab({
      url: e.targetUrl,
    });
    e.stopPropagation();
  }

  private showPanel(id: PanelId): void {
    for (const panel of document.querySelectorAll<HTMLElement>('.panel')) {
      panel.hidden = panel.id !== id;
    }
  }

  setState(newState: FreWebUiState): void {
    if (this.state === newState) {
      return;
    }
    if (this.state) {
      this.states.get(this.state)!.onExit?.call(this);
      this.cancelTimeout();
    }
    this.state = newState;
    this.states.get(this.state)!.onEnter?.call(this);
    freHandler.webUiStateChanged(newState);
  }

  readonly states: Map<FreWebUiState, StateDescriptor> = new Map([
    [
      FreWebUiState.kBeginLoading,
      {onEnter: this.beginLoading},
    ],
    [
      FreWebUiState.kShowLoading,
      {onEnter: this.showLoading},
    ],
    [
      FreWebUiState.kHoldLoading,
      {onEnter: this.holdLoading},
    ],
    [
      FreWebUiState.kFinishLoading,
      {onEnter: this.finishLoading},
    ],
    [
      FreWebUiState.kError,
      {
        onEnter: () => {
          this.destroyWebview();
          this.showPanel('errorPanel');
        },
      },
    ],
    [
      FreWebUiState.kOffline,
      {
        onEnter: () => {
          this.destroyWebview();
          this.showPanel('offlinePanel');
        },
      },
    ],
    [
      FreWebUiState.kReady,
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

  beginLoading(): void {
    // TODO crbug.com/393417356. Check cookies and load the web client after
    // cookie sync. Set up the timer to transition to show loading after a
    // determined prehold loading time.
    this.webview.src = loadTimeData.getString('glicFreURL');
    this.setState(FreWebUiState.kShowLoading);
  }

  showLoading(): void {
    // TODO crbug.com/393417356. Show a loading panel and transition to finish
    // loading after a minimum hold loading time.
    this.setState(FreWebUiState.kFinishLoading);
  }

  holdLoading(): void {
    // TODO crbug.com/393417356. Setup a timer to transition to ready
    // only after a minimum hold loading time.
    // The web client is ready, but we still wait for the remainder of
    // `kMinHoldLoadingTimeMs` before showing it, to allow the loading
    // animation
    // to complete once.
    this.setState(FreWebUiState.kReady);
  }

  finishLoading(): void {
    // The web client is not yet ready, so wait for the remainder of
    // `kMaxWaitTimeMs`. Switch to error state at that time unless interrupted
    // by `contentLoaded`.
    this.loadingTimer = setTimeout(() => {
      this.setState(FreWebUiState.kError);
    }, kMaxWaitTimeMs);
  }

  // Destroy the current webview and create a new one. This is necessary because
  // webview does not support unloading content by setting src=""
  destroyWebview(): void {
    this.webview.removeEventListener('loadcommit', this.onLoadCommit);
    this.webview.removeEventListener('contentload', this.contentLoaded);
    this.webview.removeEventListener('newwindow', this.onNewWindow);

    this.webview = this.createWebView();
  }
}
