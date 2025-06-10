// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {PrepareForClientResult, ProfileReadyState, WebUiState} from './glic.mojom-webui.js';
import type {PageInterface} from './glic.mojom-webui.js';
import type {ApiHostEmbedder} from './glic_api_impl/glic_api_host.js';
import {WebClientState} from './glic_api_impl/glic_api_host.js';
import type {PageType, WebviewDelegate} from './webview.js';
import {WebviewController, WebviewPersistentState} from './webview.js';

const transitionDuration = {
  microseconds: BigInt(100000),
};

// Time to wait before showing loading panel.
const kPreHoldLoadingTimeMs = loadTimeData.getInteger('preLoadingTimeMs');

// Minimum time to hold "loading" panel visible.
const kMinHoldLoadingTimeMs = loadTimeData.getInteger('minLoadingTimeMs');

// Maximum time to wait for load before showing error panel.
const kMaxWaitTimeMs = loadTimeData.getInteger('maxLoadingTimeMs');

// Whether to enable the debug button on the error panel. Can be enabled with
// the --enable-features=GlicDebugWebview command-line flag.
const kEnableDebug = loadTimeData.getBoolean('enableDebug');

// Whether additional web client unresponsiveness tracking metrics should be
// recorded.
const kEnableUnresponsiveMetrics =
    loadTimeData.getBoolean('enableWebClientUnresponsiveMetrics');

interface PageElementTypes {
  panelContainer: HTMLElement;
  loadingPanel: HTMLElement;
  offlinePanel: HTMLElement;
  errorPanel: HTMLElement;
  unavailablePanel: HTMLElement;
  signInPanel: HTMLElement;
  guestPanel: HTMLElement;
  webviewHeader: HTMLDivElement;
  webviewContainer: HTMLDivElement;
  signInButton: HTMLButtonElement;
  unresponsiveOverlay: HTMLElement;
}

const $: PageElementTypes = new Proxy({}, {
  get(_target: any, prop: string) {
    return getRequiredElement(prop);
  },
});

type PanelId = 'loadingPanel'|'guestPanel'|'offlinePanel'|'errorPanel'|
    'unavailablePanel'|'signInPanel';

interface StateDescriptor {
  onEnter?: () => void;
  onExit?: () => void;
  // Whether to try to reload the webview on open while in this state.
  reloadOnOpen?: boolean;
}

// Web client unresponsiveness state tracking values for metrics reporting.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WebClientUnresponsiveState)
export enum WebClientUnresponsiveState {
  ENTERED_FROM_WEBVIEW_EVENT = 0,
  ENTERED_FROM_CUSTOM_HEARTBEAT = 1,
  ALREADY_ON_FROM_WEBVIEW_EVENT = 2,
  ALREADY_ON_FROM_CUSTOM_HEARTBEAT = 3,
  EXITED = 4,
  MAX_VALUE = EXITED,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:WebClientUnresponsiveState)

export class GlicAppController implements PageInterface, WebviewDelegate,
                                          ApiHostEmbedder {
  loadingTimer: number|undefined;

  // This is used to simulate no connection for tests.
  private simulateNoConnection: boolean =
      loadTimeData.getBoolean('simulateNoConnection');

  private guestResizeEnabled: boolean = false;

  // Width for non-resizable panel.
  private defaultWidth: number = 352;

  // Last seen width and height of guest panel.
  private lastWidth: number = 400;
  private lastHeight: number = 80;

  // Present only when loading or after loading is finished. Removed on error.
  private webview?: WebviewController;
  private webviewPersistentState = new WebviewPersistentState();

  private profileReadyState: ProfileReadyState|undefined = undefined;
  private profileReadyInitialState = Promise.withResolvers<void>();

  private enteredUnresponsiveTimestampMs?: number;

  state: WebUiState|undefined;

  // When entering loading state, this represents the earliest timestamp at
  // which the UI can transition to the ready state. This ensures that the
  // loading UI isn't just a brief flash on screen.
  private earliestLoadingDismissTime: number|undefined;

  browserProxy: BrowserProxyImpl;

  constructor() {
    this.browserProxy = new BrowserProxyImpl(this);

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
    $.signInButton.addEventListener('click', () => {
      this.signIn();
    });

    document.addEventListener('keydown', ev => {
      if (this.state !== WebUiState.kReady) {
        if (ev.code === 'Escape') {
          ev.stopPropagation();
          ev.preventDefault();
          this.browserProxy.handler.closePanel();
        }
      }
    });

    if (kEnableDebug) {
      window.addEventListener('load', () => {
        this.installDebugButton();
      });
    }
  }

  // WebviewDelegate implementation.
  webviewUnresponsive(): void {
    console.warn('webview unresponsive');
    this.trackUnresponsiveState(
        this.state === WebUiState.kUnresponsive ?
            WebClientUnresponsiveState.ALREADY_ON_FROM_WEBVIEW_EVENT :
            WebClientUnresponsiveState.ENTERED_FROM_WEBVIEW_EVENT);
    this.setState(WebUiState.kUnresponsive);
  }

  trackUnresponsiveState(newState: WebClientUnresponsiveState): void {
    if (!kEnableUnresponsiveMetrics) {
      return;
    }

    // Track and record unresponsive state duration.
    if (newState === WebClientUnresponsiveState.ENTERED_FROM_WEBVIEW_EVENT ||
        newState === WebClientUnresponsiveState.ENTERED_FROM_CUSTOM_HEARTBEAT) {
      // Entering an unresponsive state.
      this.enteredUnresponsiveTimestampMs = Date.now();
    } else if (newState === WebClientUnresponsiveState.EXITED) {
      // Existing an unresponsive state.
      if (this.enteredUnresponsiveTimestampMs !== undefined) {
        const unresponsiveDuration =
            Date.now() - this.enteredUnresponsiveTimestampMs;
        chrome.metricsPrivate.recordMediumTime(
            'Glic.Host.WebClientUnresponsiveState.Duration',
            unresponsiveDuration);
        this.enteredUnresponsiveTimestampMs = undefined;
      } else {
        console.error(
            'Unresponsive state exited without an entering timestamp');
      }
    }

    // Record unresponsive state detections and transitions.
    chrome.metricsPrivate.recordEnumerationValue(
        'Glic.Host.WebClientUnresponsiveState', newState,
        WebClientUnresponsiveState.MAX_VALUE + 1);
  }

  webviewError(reason: string): void {
    console.warn(`webview exit. reason: ${reason}`);
    this.setState(WebUiState.kError);
  }

  webviewPageCommit(type: PageType) {
    switch (type) {
      case 'login':
        this.lastWidth = 400;
        this.lastHeight = 800;
        this.cancelTimeout();
        $.guestPanel.classList.toggle('show-header', true);
        this.showPanel('guestPanel');
        break;
      case 'guestError':
        this.setState(WebUiState.kGuestError);
        break;
      case 'regular':
        $.guestPanel.classList.toggle('show-header', false);
        if (this.state === WebUiState.kReady ||
            this.state === WebUiState.kGuestError) {
          this.setState(WebUiState.kBeginLoad);
        }
        break;
    }
  }

  private setState(newState: WebUiState): void {
    if (this.state === newState) {
      return;
    }
    if (this.state) {
      this.states.get(this.state)!.onExit?.call(this);
    }
    this.state = newState;
    this.states.get(this.state)!.onEnter?.call(this);
    this.browserProxy.handler.webUiStateChanged(this.state);
    this.browserProxy.handler.enableDragResize(
        this.state === WebUiState.kReady && this.guestResizeEnabled);
  }

  private stateDescriptor(): StateDescriptor|undefined {
    return this.state !== undefined ? this.states.get(this.state) : undefined;
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
        reloadOnOpen: true,
        onEnter:
            () => {
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
        reloadOnOpen: true,
        onEnter:
            () => {
              this.destroyWebview();
              this.showPanel('unavailablePanel');
            },
      },
    ],
    [
      WebUiState.kReady,
      {
        onEnter: () => {
          $.guestPanel.classList.toggle('show-header', false);
          this.showPanel('guestPanel');
        },
      },
    ],
    [
      WebUiState.kUnresponsive,
      {
        reloadOnOpen: true,
        onEnter:
            () => {
              $.unresponsiveOverlay.classList.toggle('hidden', false);
            },
        onExit:
            () => {
              this.trackUnresponsiveState(WebClientUnresponsiveState.EXITED);
              $.unresponsiveOverlay.classList.toggle('hidden', true);
            },
      },
    ],
    [
      WebUiState.kSignIn,
      {
        reloadOnOpen: true,
        onEnter:
            () => {
              this.destroyWebview();
              this.showPanel('signInPanel');
            },
      },
    ],
    [
      WebUiState.kGuestError,
      {
        reloadOnOpen: true,
        onEnter:
            () => {
              this.lastWidth = 400;
              this.lastHeight = 800;
              $.guestPanel.classList.toggle('show-header', true);
              this.showPanel('guestPanel');
            },
      },
    ],
  ]);

  private cancelTimeout(): void {
    if (this.loadingTimer) {
      clearTimeout(this.loadingTimer);
      this.loadingTimer = undefined;
    }
  }

  private async beginLoad(): Promise<void> {
    // Time to show the loading panel if the web client is not ready.
    const showLoadingTime = performance.now() + kPreHoldLoadingTimeMs;

    // profileReadyState isn't available right away. Wait until it's ready.
    await this.profileReadyInitialState.promise;

    const readyState = this.profileReadyState;
    switch (readyState) {
      case ProfileReadyState.kUnknownError:
        this.setState(WebUiState.kUnavailable);
        return;
      case ProfileReadyState.kSignInRequired:
        this.setState(WebUiState.kSignIn);
        return;
      case ProfileReadyState.kReady:
        break;
    }

    // Blocking on cookie syncing here introduces latency, we should consider
    // ways to avoid it.
    const {result} = await this.browserProxy.handler.prepareForClient();
    switch (result) {
      case PrepareForClientResult.kSuccess:
        break;
      case PrepareForClientResult.kUnknownError:
        console.warn('prepareForClient in beginLoad() failed.');
        this.setState(WebUiState.kError);
        return;
      case PrepareForClientResult.kRequiresSignIn:
        this.setState(WebUiState.kSignIn);
        return;
    }

    // Load the web client only after cookie sync is complete.
    this.destroyWebview();
    this.webview = new WebviewController(
        $.webviewContainer, this.browserProxy, this, this,
        this.webviewPersistentState);
    this.webview.getWebClientState().subscribe(
        this.webClientStateChanged.bind(this));

    this.loadingTimer = setTimeout(() => {
      this.setState(WebUiState.kShowLoading);
    }, Math.max(0, showLoadingTime - performance.now()));
  }

  private showLoading(): void {
    this.showPanel('loadingPanel');
    // After kMinHoldLoadingTimeMs, transition to finish-loading or ready. Note
    // that we do not transition from show-loading to ready before the timeout.
    this.earliestLoadingDismissTime = performance.now() + kMinHoldLoadingTimeMs;
    this.loadingTimer = setTimeout(() => {
      this.setState(WebUiState.kFinishLoading);
    }, kMinHoldLoadingTimeMs);
  }

  private holdLoading(): void {
    // The web client is ready, but we still wait for the remainder of
    // `kMinHoldLoadingTimeMs` before showing it, to allow the loading animation
    // to complete once.
    this.loadingTimer = setTimeout(() => {
      this.setState(WebUiState.kReady);
    }, Math.max(0, this.earliestLoadingDismissTime! - performance.now()));
  }

  private finishLoading(): void {
    // The web client is not yet ready, so wait for the remainder of
    // `kMaxWaitTimeMs`. Switch to error state at that time unless interrupted
    // by `webClientReady`.
    this.loadingTimer = setTimeout(() => {
      if (this.webview?.waitingOnPanelWillOpen()) {
        console.warn('Exceeded timeout waiting for notifyPanelWillOpen');
        this.setState(WebUiState.kError);
      } else if (
          this.webview?.getWebClientState().getCurrentValue() ===
          WebClientState.RESPONSIVE) {
        this.setState(WebUiState.kReady);
      } else {
        console.warn('Exceeded timeout waiting for client to load');
        this.setState(WebUiState.kError);
      }
    }, kMaxWaitTimeMs - kMinHoldLoadingTimeMs);
  }

  // Show one webUI panel and hide the others. The widget is resized to fit the
  // newly-visible content. If the guest panel is now visible, then its size
  // will be determined by the most recent resize request.
  private showPanel(id: PanelId): void {
    for (const panel of document.querySelectorAll<HTMLElement>('.panel')) {
      panel.hidden = panel.id !== id;
    }
    // Resize widget to size of new panel.
    if (id === 'guestPanel') {
      // For the guest webview, use the most recently requested size.
      this.browserProxy.handler.resizeWidget(
          {width: this.lastWidth, height: this.lastHeight}, transitionDuration);
    } else {
      this.browserProxy.handler.resizeWidget(
          {
            width: this.defaultWidth,
            height: $[id].getBoundingClientRect().height,
          },
          transitionDuration);
    }
  }

  // Destroy the current webview if it exists. This is necessary because
  // webview does not support unloading content by setting src=""
  private destroyWebview(): void {
    if (!this.webview) {
      return;
    }
    this.webview.destroy();
    this.webview = undefined;
  }

  private online(): void {
    if (this.simulateNoConnection) {
      return;
    }
    if (this.state !== WebUiState.kOffline) {
      return;
    }
    this.setState(WebUiState.kBeginLoad);
  }

  private offline(): void {
    const allowedStates = [
      WebUiState.kBeginLoad,
      WebUiState.kShowLoading,
      WebUiState.kFinishLoading,
    ];
    if (allowedStates.includes(this.state!)) {
      this.setState(WebUiState.kOffline);
    }
  }

  private installDebugButton(): void {
    const button = document.createElement('cr-icon-button');
    button.id = 'debug';
    button.classList.add('tonal-button');
    button.setAttribute('iron-icon', 'cr:search');
    document.querySelector('#errorPanel .notice')!.appendChild(button);
    button.addEventListener('click', () => {
      this.showDebug();
    });
  }

  // ApiHostEmbedder implementation.

  // Called when the web client requests that the window size be changed.
  onGuestResizeRequest(request: {width: number, height: number}) {
    // Save most recently requested guest window size.
    this.lastWidth = request.width;
    this.lastHeight = request.height;
  }

  // Called when the web client requests to enable manual drag resize.
  enableDragResize(enabled: boolean) {
    this.guestResizeEnabled = enabled;
    if (this.state === WebUiState.kReady) {
      this.browserProxy.handler.enableDragResize(this.guestResizeEnabled);
    }
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

  webClientStateChanged(state: WebClientState): void {
    switch (state) {
      case WebClientState.RESPONSIVE:
        // If we're still in a loading state, let it transition naturally
        // through the loading process.
        switch (this.state) {
          case WebUiState.kBeginLoad:
          case WebUiState.kShowLoading:
          case WebUiState.kHoldLoading:
            return;
        }
        this.setState(WebUiState.kReady);
        break;
      case WebClientState.UNRESPONSIVE:
        this.trackUnresponsiveState(
            this.state === WebUiState.kUnresponsive ?
                WebClientUnresponsiveState.ALREADY_ON_FROM_CUSTOM_HEARTBEAT :
                WebClientUnresponsiveState.ENTERED_FROM_CUSTOM_HEARTBEAT);
        this.setState(WebUiState.kUnresponsive);
        break;
      case WebClientState.ERROR:
        this.guestResizeEnabled = false;
        this.setState(WebUiState.kError);
        break;
    }
  }

  // External entry points.

  // TODO: Make this a proper state.
  showDebug(): void {
    this.lastWidth = 400;
    this.lastHeight = 800;
    this.setState(WebUiState.kReady);
    $.guestPanel.classList.toggle('show-header', true);
    $.guestPanel.classList.toggle('debug', true);
  }

  close(): void {
    // If we're in the debug view, switch back to error. Otherwise close the
    // window.
    if (this.state === WebUiState.kReady &&
        $.guestPanel.classList.contains('debug')) {
      $.guestPanel.classList.toggle('debug', false);
      this.setState(WebUiState.kError);
    } else {
      this.browserProxy.handler.closePanel();
    }
  }

  // Called when the reload button is clicked.
  reload(): void {
    this.destroyWebview();
    // TODO: Allow the timeout on this load to be longer than the initial load.
    this.setState(WebUiState.kBeginLoad);
  }

  private signIn(): void {
    this.browserProxy.handler.signInAndClosePanel();
  }

  // PageInterface implementation.

  // Called before the WebUI is shown. If we're in an error state, automatically
  // try to reload.
  intentToShow() {
    if (this.stateDescriptor()?.reloadOnOpen) {
      this.reload();
    }
  }

  setProfileReadyState(state: ProfileReadyState) {
    if (this.profileReadyState === state) {
      return;
    }
    const initialCall = this.profileReadyState === undefined;
    this.profileReadyState = state;

    if (initialCall) {
      // The initial state is handled in `beginLoad()`.
      this.profileReadyInitialState.resolve();
    } else {
      switch (this.profileReadyState) {
        case ProfileReadyState.kUnknownError:
          this.setState(WebUiState.kUnavailable);
          break;
        case ProfileReadyState.kSignInRequired:
          this.setState(WebUiState.kSignIn);
          break;
        case ProfileReadyState.kReady:
          if (this.stateDescriptor()?.reloadOnOpen) {
            this.setState(WebUiState.kBeginLoad);
          }
          break;
      }
    }
  }
}
