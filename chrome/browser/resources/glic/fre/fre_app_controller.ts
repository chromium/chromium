// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {FrePageHandlerFactory, FrePageHandlerRemote, FreWebUiState} from './glic_fre.mojom-webui.js';

interface StateDescriptor {
  onEnter?: () => void;
  onExit?: () => void;
}

interface PageElementTypes {
  webviewContainer: HTMLDivElement;
}

const $: PageElementTypes = new Proxy({}, {
  get(_target: any, prop: string) {
    return getRequiredElement(prop);
  },
});

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
  private webview: chrome.webviewTag.WebView;
  private webviewEventTracker = new EventTracker();

  constructor() {
    this.onLoadCommit = this.onLoadCommit.bind(this);
    this.onContentLoad = this.onContentLoad.bind(this);
    this.onNewWindow = this.onNewWindow.bind(this);

    this.webview = this.createWebview();

    window.addEventListener('online', () => {
      this.online();
    });
    window.addEventListener('offline', () => {
      this.offline();
    });
    window.addEventListener('load', () => {
      // Allow WebUI close button to close the window. This close button is
      // present on all UI states except for `FreWebUiState.kReady`.
      document.querySelector('.close-button')!.addEventListener('click', () => {
        freHandler.dismissFre();
      });
    });

    if (navigator.onLine) {
      this.setState(FreWebUiState.kBeginLoading);
    } else {
      this.setState(FreWebUiState.kOffline);
    }
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

  onContentLoad() {
    this.setState(FreWebUiState.kReady);
  }

  onNewWindow(e: any) {
    e.preventDefault();
    freHandler.validateAndOpenLinkInNewTab({
      url: e.targetUrl,
    });
    e.stopPropagation();
  }

  online(): void {
    if (this.state !== FreWebUiState.kOffline) {
      return;
    }
    this.setState(FreWebUiState.kBeginLoading);
  }

  offline(): void {
    const allowedStates = [
      FreWebUiState.kBeginLoading,
      FreWebUiState.kShowLoading,
      FreWebUiState.kFinishLoading,
      FreWebUiState.kReady,
    ];
    if (allowedStates.includes(this.state)) {
      this.setState(FreWebUiState.kOffline);
    }
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

  async beginLoading(): Promise<void> {
    // Attempt to re-sync cookies before continuing.
    const {success} = await freHandler.prepareForClient();
    if (!success) {
      this.setState(FreWebUiState.kError);
      return;
    }

    // Load the web client now that cookie sync is complete.
    this.destroyWebview();

    // TODO(crbug.com/393417356): Set up a timer to transition to showLoading
    // state after a determined prehold loading time.
    this.webview.src = loadTimeData.getString('glicFreURL');
    this.setState(FreWebUiState.kShowLoading);
  }

  private createWebview(): chrome.webviewTag.WebView {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;
    webview.id = 'freGuestFrame';
    webview.setAttribute('partition', 'glicfrepart');
    $.webviewContainer.appendChild(webview);

    this.webviewEventTracker.add(
        webview, 'loadcommit', this.onLoadCommit.bind(this));
    this.webviewEventTracker.add(
        webview, 'contentload', this.onContentLoad.bind(this));
    this.webviewEventTracker.add(
        webview, 'newwindow', this.onNewWindow.bind(this));

    return webview;
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
    // by `onContentLoad`.
    this.loadingTimer = setTimeout(() => {
      this.setState(FreWebUiState.kError);
    }, kMaxWaitTimeMs);
  }

  // Destroy the current webview and create a new one. This is necessary because
  // webview does not support unloading content by setting src=""
  destroyWebview(): void {
    this.webviewEventTracker.removeAll();

    $.webviewContainer.removeChild(this.webview);

    this.webview = this.createWebview();
  }
}
