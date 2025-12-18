// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from '//resources/js/util.js';

import type {BrowserProxyImpl} from './browser_proxy.js';
import type {ProfileReadyState} from './glic.mojom-webui.js';
import {PanelStateKind, WebUiState} from './glic.mojom-webui.js';
import type {ApiHostEmbedder} from './glic_api_impl/host/glic_api_host.js';
import type {GlicWebviewLoadState, OnlineMonitor} from './glic_webview_loader.js';
import {GlicWebviewLoader, GlicWebviewLoadErrorReason, GlicWebviewLoadStatus, OnlineMonitorImpl} from './glic_webview_loader.js';

const transitionDuration = {
  microseconds: BigInt(100000),
};


interface PageElementTypes {
  panelContainer: HTMLElement;
  loadingPanel: HTMLElement;
  offlinePanel: HTMLElement;
  errorPanel: HTMLElement;
  unavailablePanel: HTMLElement;
  disabledByAdminPanel: HTMLElement;
  signInPanel: HTMLElement;
  guestPanel: HTMLElement;
  webviewHeader: HTMLDivElement;
  webviewContainer: HTMLDivElement;
  profilePickerButton: HTMLButtonElement;
  disabledByAdminCloseButton: HTMLButtonElement;
  signInButton: HTMLButtonElement;
  unresponsiveOverlay: HTMLElement;
  reload: HTMLButtonElement;
}

const $: PageElementTypes = new Proxy({}, {
  get(_target: any, prop: string) {
    return getRequiredElement(prop);
  },
});

type PanelId = 'loadingPanel'|'guestPanel'|'offlinePanel'|'errorPanel'|
    'unavailablePanel'|'disabledByAdminPanel'|'signInPanel';

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

export class GlicAppController implements ApiHostEmbedder {
  loadingTimer: number|undefined;

  // Time to wait before showing loading panel.
  readonly kPreHoldLoadingTimeMs = loadTimeData.getInteger('preLoadingTimeMs');

  // Minimum time to hold "loading" panel visible.
  readonly kMinHoldLoadingTimeMs = loadTimeData.getInteger('minLoadingTimeMs');

  // Maximum time to wait for load before showing error panel.
  readonly kMaxWaitTimeMs = loadTimeData.getInteger('maxLoadingTimeMs');

  // Whether to enable the debug button on the error panel. Can be enabled with
  // the --enable-features=GlicDebugWebview command-line flag.
  readonly kEnableDebug = loadTimeData.getBoolean('enableDebug');

  // Whether additional web client unresponsiveness tracking metrics should be
  // recorded.
  readonly kEnableUnresponsiveMetrics =
      loadTimeData.getBoolean('enableWebClientUnresponsiveMetrics');

  private guestResizeEnabled: boolean = false;

  // Width and height for non-resizable panel.
  private defaultWidth: number = 400;
  private defaultHeight: number = 252;

  // Height for floating loading panel.
  private floatingLoadingHeight: number = 80;

  // Last seen width and height of guest panel.
  private lastWidth: number = 400;
  private lastHeight: number = 80;

  private enteredUnresponsiveTimestampMs?: number;

  private panelStateKind: PanelStateKind = PanelStateKind.kHidden;

  state: WebUiState|undefined;

  // When entering loading state, this represents the earliest timestamp at
  // which the UI can transition to the ready state. This ensures that the
  // loading UI isn't just a brief flash on screen.
  private earliestLoadingDismissTime: number|undefined;

  browserProxy: BrowserProxyImpl;
  private onlineMonitor: OnlineMonitor = new OnlineMonitorImpl();
  private webviewLoader: GlicWebviewLoader;

  constructor(browserProxy: BrowserProxyImpl) {
    this.browserProxy = browserProxy;
    this.webviewLoader = new GlicWebviewLoader(
        $.webviewContainer, browserProxy, this, this.onlineMonitor);
    this.webviewLoader.getState().subscribe(
        this.webviewLoadStateChanged.bind(this));

    $.profilePickerButton.addEventListener('click', () => {
      this.openProfilePicker();
    });
    $.reload.addEventListener('click', () => {
      this.reload();
    });
    $.disabledByAdminCloseButton.addEventListener('click', () => {
      this.browserProxy.pageHandler.closePanel();
    });
    $.disabledByAdminPanel.querySelector('a')?.addEventListener('click', () => {
      this.openDisabledByAdminLink();
    });
    $.signInButton.addEventListener('click', () => {
      this.signIn();
    });

    document.addEventListener('keydown', ev => {
      if (this.state !== WebUiState.kReady) {
        if (ev.code === 'Escape') {
          ev.stopPropagation();
          ev.preventDefault();
          this.browserProxy.pageHandler.closePanel();
        }
      }
    });

    if (this.kEnableDebug) {
      window.addEventListener('load', () => {
        this.installDebugButton();
      });
    }

    this.webviewLoader.setWantLoad(true);
  }

  trackUnresponsiveState(newState: WebClientUnresponsiveState): void {
    if (!this.kEnableUnresponsiveMetrics) {
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

  private setWebUiState(newState: WebUiState): void {
    if (this.state === newState) {
      return;
    }
    if (this.state) {
      this.states.get(this.state)!.onExit?.call(this);
    }
    this.state = newState;
    this.states.get(this.state)!.onEnter?.call(this);
    this.browserProxy.pageHandler.webUiStateChanged(this.state);
    this.browserProxy.pageHandler.enableDragResize(
        this.state === WebUiState.kReady && this.guestResizeEnabled);
  }

  private stateDescriptor(): StateDescriptor|undefined {
    return this.state !== undefined ? this.states.get(this.state) : undefined;
  }

  readonly states: Map<WebUiState, StateDescriptor> = new Map([
    [
      WebUiState.kBeginLoad,
      {
        onEnter: this.beginLoad,
        onExit: this.cancelTimeout,
      },
    ],
    [
      WebUiState.kShowLoading,
      {
        onEnter: this.showLoading,
        onExit: this.cancelTimeout,
      },
    ],
    [
      WebUiState.kHoldLoading,
      {
        onEnter: this.holdLoading,
        onExit: this.cancelTimeout,
      },
    ],
    [
      WebUiState.kFinishLoading,
      {
        onEnter: this.finishLoading,
        onExit: this.cancelTimeout,
      },
    ],
    [
      WebUiState.kError,
      {
        reloadOnOpen: true,
        onEnter:
            () => {
              this.showPanel('errorPanel');
            },
      },
    ],
    [
      WebUiState.kOffline,
      {
        onEnter: () => {
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
              this.showPanel('unavailablePanel');
            },
      },
    ],
    [
      WebUiState.kDisabledByAdmin,
      {
        reloadOnOpen: true,
        onEnter:
            () => {
              this.showPanel('disabledByAdminPanel');
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

  private beginLoad(): void {
    // Wait a moment before showing the loading panel.
    this.loadingTimer = setTimeout(() => {
      this.setWebUiState(WebUiState.kShowLoading);
    }, this.kPreHoldLoadingTimeMs);
  }


  private webviewLoadStateChanged(state: GlicWebviewLoadState) {
    switch (state.status) {
      case GlicWebviewLoadStatus.NOT_LOADED:
        this.setWebUiState(WebUiState.kError);
        break;
      case GlicWebviewLoadStatus.LOADING:
        this.setWebUiState(WebUiState.kBeginLoad);
        break;
      case GlicWebviewLoadStatus.RESPONSIVE:
        $.guestPanel.classList.toggle('show-header', false);
        switch (this.state) {
          case WebUiState.kBeginLoad:
          case WebUiState.kShowLoading:
          case WebUiState.kHoldLoading:
            // We wait until webClientReady() to transition to kReady.
            break;
          default:
            this.setWebUiState(WebUiState.kReady);
        }
        break;
      case GlicWebviewLoadStatus.UNRESPONSIVE:
        console.warn('webview unresponsive');
        this.trackUnresponsiveState(
            this.state === WebUiState.kUnresponsive ?
                WebClientUnresponsiveState.ALREADY_ON_FROM_WEBVIEW_EVENT :
                WebClientUnresponsiveState.ENTERED_FROM_WEBVIEW_EVENT);
        this.setWebUiState(WebUiState.kUnresponsive);
        break;
      case GlicWebviewLoadStatus.AT_LOGIN_PAGE:
        this.lastWidth = 400;
        this.lastHeight = 800;
        this.cancelTimeout();
        $.guestPanel.classList.toggle('show-header', true);
        this.showPanel('guestPanel');
        break;
      case GlicWebviewLoadStatus.AT_GUEST_ERROR_PAGE:
        this.setWebUiState(WebUiState.kGuestError);
        break;
      case GlicWebviewLoadStatus.ERROR:
        const reason = state.errorReason!;
        this.guestResizeEnabled = false;
        switch (reason) {
          case GlicWebviewLoadErrorReason.UNKNOWN:
          case GlicWebviewLoadErrorReason.BLOCKED_BY_SYNC_ERROR:
          case GlicWebviewLoadErrorReason.LOGIN_PAGE_REACHED:
          case GlicWebviewLoadErrorReason.PAGE_LOAD_ERROR:
            this.setWebUiState(WebUiState.kError);
            break;
          case GlicWebviewLoadErrorReason.BLOCKED_BY_NEED_SIGN_IN:
            this.setWebUiState(WebUiState.kSignIn);
            break;
          case GlicWebviewLoadErrorReason.PROFILE_INELIGIBLE:
            this.setWebUiState(WebUiState.kUnavailable);
            break;
          case GlicWebviewLoadErrorReason.DISABLED_BY_ADMIN:
            $.disabledByAdminPanel.classList.toggle(
                'show-disabled-by-admin-link', true);
            this.setWebUiState(WebUiState.kDisabledByAdmin);
            break;
          case GlicWebviewLoadErrorReason.NOT_ONLINE:
            this.setWebUiState(WebUiState.kOffline);
            break;
          default:
            assertNotReachedCase(reason);
        }
        break;
      default:
        assertNotReachedCase(state.status);
    }
  }

  private showLoading(): void {
    this.showPanel('loadingPanel');
    // After kMinHoldLoadingTimeMs, transition to finish-loading or ready. Note
    // that we do not transition from show-loading to ready before the timeout.
    this.earliestLoadingDismissTime =
        performance.now() + this.kMinHoldLoadingTimeMs;
    this.loadingTimer = setTimeout(() => {
      this.setWebUiState(WebUiState.kFinishLoading);
    }, this.kMinHoldLoadingTimeMs);
  }

  private holdLoading(): void {
    // The web client is ready, but we still wait for the remainder of
    // `kMinHoldLoadingTimeMs` before showing it, to allow the loading animation
    // to complete once.
    this.loadingTimer = setTimeout(() => {
      this.setWebUiState(WebUiState.kReady);
    }, Math.max(0, this.earliestLoadingDismissTime! - performance.now()));
  }

  private finishLoading(): void {
    // The web client is not yet ready, so wait for the remainder of
    // `kMaxWaitTimeMs`. Switch to error state at that time unless interrupted
    // by `webClientReady`.
    this.loadingTimer = setTimeout(() => {
      if (!loadTimeData.getBoolean('glicWebContentsWarming') &&
          this.webviewLoader.waitingOnPanelWillOpen()) {
        console.warn('Exceeded timeout waiting for notifyPanelWillOpen');
        this.setWebUiState(WebUiState.kError);
      } else if (
          this.webviewLoader.currentStatus() ===
          GlicWebviewLoadStatus.RESPONSIVE) {
        // Normally, webClientReady() would transition to kReady. This is
        // a fallback which is used when prewarming, as the webUI isn't shown
        // yet.
        this.setWebUiState(WebUiState.kReady);
      }
    }, this.kMaxWaitTimeMs - this.kMinHoldLoadingTimeMs);
  }

  // Show one webUI panel and hide the others. The widget is resized to fit the
  // newly-visible content. If the guest panel is now visible, then its size
  // will be determined by the most recent resize request.
  private showPanel(id: PanelId): void {
    for (const panel of document.querySelectorAll<HTMLElement>('.panel')) {
      panel.hidden = panel.id !== id;
    }

    const panelStateKindSection = getRequiredElement('localPanels');
    panelStateKindSection.classList.toggle('hidden', id === 'guestPanel');
    // Resize widget to size of new panel.
    if (id === 'guestPanel') {
      // For the guest webview, use the most recently requested size.
      this.browserProxy.pageHandler.resizeWidget(
          {width: this.lastWidth, height: this.lastHeight}, transitionDuration);
    }
    if (id === 'loadingPanel') {
      this.browserProxy.pageHandler.resizeWidget(
          {
            width: this.defaultWidth,
            height: this.floatingLoadingHeight,
          },
          transitionDuration);
    } else {
      this.browserProxy.pageHandler.resizeWidget(
          {
            width: this.defaultWidth,
            height: this.defaultHeight,
          },
          transitionDuration);
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
      this.browserProxy.pageHandler.enableDragResize(this.guestResizeEnabled);
    }
  }

  // Called when the notifyPanelWillOpen promise resolves to open the panel
  // when triggered from the browser.
  webClientReady(): void {
    if (this.state === WebUiState.kBeginLoad ||
        this.state === WebUiState.kFinishLoading) {
      this.setWebUiState(WebUiState.kReady);
    } else if (this.state === WebUiState.kShowLoading) {
      this.setWebUiState(WebUiState.kHoldLoading);
    }
  }

  // External entry points.

  // TODO: Make this a proper state.
  showDebug(): void {
    this.lastWidth = 400;
    this.lastHeight = 800;
    this.setWebUiState(WebUiState.kReady);
    $.guestPanel.classList.toggle('show-header', true);
    $.guestPanel.classList.toggle('debug', true);
  }

  close(): void {
    // If we're in the debug view, switch back to error. Otherwise close the
    // window.
    if (this.state === WebUiState.kReady &&
        $.guestPanel.classList.contains('debug')) {
      $.guestPanel.classList.toggle('debug', false);
      this.setWebUiState(WebUiState.kError);
    } else if (this.state === WebUiState.kReady) {
      this.browserProxy.pageHandler.closePanel();
    } else {
      // Reload in the background if user closes window while web client is not
      // ready. This is an escape hatch for situation where we're stuck in a
      // loading state caused by an error.
      this.browserProxy.pageHandler.closePanel().then(() => {
        this.reload();
      });
    }
  }

  // Called when the reload button is clicked.
  reload(): void {
    this.webviewLoader.reload();
    // TODO: Allow the timeout on this load to be longer than the initial load.
  }

  private openProfilePicker(): void {
    this.browserProxy.pageHandler.openProfilePickerAndClosePanel();
  }

  private signIn(): void {
    this.browserProxy.pageHandler.signInAndClosePanel();
  }

  // PageInterface implementation.
  updatePageState(panelStateKind: PanelStateKind) {
    if (this.panelStateKind === panelStateKind) {
      return;
    }
    this.panelStateKind = panelStateKind;

    const panelStateKindSection = getRequiredElement('localPanels');
    panelStateKindSection.classList.toggle(
        'sidePanel', this.panelStateKind === PanelStateKind.kAttached);
    panelStateKindSection.classList.toggle(
        'floating', this.panelStateKind === PanelStateKind.kDetached);
  }

  // Called before the WebUI is shown. If we're in an error state, automatically
  // try to reload.
  intentToShow() {
    if (this.stateDescriptor()?.reloadOnOpen) {
      this.reload();
    }
  }

  setProfileReadyState(state: ProfileReadyState) {
    this.webviewLoader.setProfileReadyState(state);
  }

  openDisabledByAdminLink(): void {
    this.browserProxy.pageHandler.openDisabledByAdminLinkAndClosePanel();
  }
}
