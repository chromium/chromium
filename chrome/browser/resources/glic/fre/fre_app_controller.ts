// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {GlicRequestHeaderInjector} from '/shared/glic_request_headers.js';
import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {FrePageHandlerFactory, FrePageHandlerRemote, FreWebUiState} from './glic_fre.mojom-webui.js';
import {GlicFreWebviewLoadAbortReason} from './metrics_enums.js';

// Time to wait before showing loading panel.
const PRE_HOLD_LOADING_TIME_MS = loadTimeData.getInteger('preLoadingTimeMs');

// Minimum time to hold "loading" panel visible.
const MIN_HOLD_LOADING_TIME_MS = loadTimeData.getInteger('minLoadingTimeMs');

// Maximum time to wait for load before showing error panel.
const MAX_WAIT_TIME_MS = loadTimeData.getInteger('maxLoadingTimeMs');

// Maximum time to wait for load before showing error panel following a
// user-initiated reload.
const RELOAD_MAX_WAIT_TIME_RELOAD_MS =
    loadTimeData.getInteger('reloadMaxLoadingTimeMs');

// Initial FRE width. Also used as the minimum and maximum width for FRE.
const INITIAL_WIDTH = loadTimeData.getInteger('freInitialWidth');

// Minimum height for FRE.
const MIN_HEIGHT = 200;

interface PageElementTypes {
  webviewContainer: HTMLDivElement;
}

const $: PageElementTypes = new Proxy({}, {
  get(_target: any, prop: string) {
    return getRequiredElement(prop);
  },
});

type PanelId = 'guestPanel'|'offlinePanel'|'errorPanel'|'loadingPanel'|
    'disabledByAdminPanel';

interface StateDescriptor {
  onEnter?: () => void;
  onExit?: () => void;
}

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
  private glicRequestHeaderInjector: GlicRequestHeaderInjector|undefined;

  // When entering loading state, this represents the earliest timestamp at
  // which the UI can transition to the ready state. This ensures that the
  // loading UI isn't just a brief flash on screen.
  private earliestLoadingDismissTime: number|undefined;

  // This is set when the "Try again" button in the error state is pressed,
  // indicating that a different timeout value should be used for the
  // subsequent content load. This value is reset when we the associated
  // content load ends.
  private useReloadTimeout = false;

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
      // Allow WebUI close buttons to close the window. Close buttons are
      // present on all UI states except for `FreWebUiState.kReady`.
      const buttons = document.querySelectorAll('.close-button');
      for (const button of buttons) {
        const parentPanel = button.closest('.panel');
        if (parentPanel) {
          button.addEventListener('click', () => {
            chrome.metricsPrivate.recordUserAction('Glic.Fre.CloseWithX');
            freHandler.dismissFre(this.panelIdToEnum(parentPanel.id));
          });
        }
      }

      const disabledByAdminButton =
          document.getElementById('disabledByAdminCloseButton');
      assert(disabledByAdminButton);

      const parentPanel = disabledByAdminButton.closest('.panel');
      assert(parentPanel);

      disabledByAdminButton.addEventListener('click', () => {
        chrome.metricsPrivate.recordUserAction(
            'Glic.Fre.DisabledByAdminPanelCloseButton');
        freHandler.dismissFre(this.panelIdToEnum(parentPanel.id));
      });

      document.querySelector('#disabledByAdminPanel a')
          ?.addEventListener('click', (e) => {
            e.preventDefault();
            chrome.metricsPrivate.recordUserAction(
                'Glic.Fre.DisabledByAdminPanelLinkClicked');
            freHandler.validateAndOpenLinkInNewTab({
              url: (e.target as HTMLAnchorElement).href,
            });
            e.stopPropagation();
          });

      document.getElementById('reload')?.addEventListener('click', () => {
        this.reload();
      });
    });

    document.addEventListener('keydown', ev => {
      if (ev.code === 'Escape') {
        ev.stopPropagation();
        ev.preventDefault();
        const visiblePanel =
            document.querySelector<HTMLElement>('.panel:not([hidden])');
        if (visiblePanel) {
          chrome.metricsPrivate.recordUserAction('Glic.Fre.CloseWithEsc');
          freHandler.dismissFre(this.panelIdToEnum(visiblePanel.id));
        }
      }
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

    if (loadTimeData.getBoolean('caaGuestError') &&
        (url.hostname === 'access.workspace.google.com' ||
         url.hostname === 'admin.google.com')) {
      this.setState(FreWebUiState.kDisabledByAdmin);
      return;
    }

    // Fragment navigations are used to represent actions taken in the web
    // client following this mapping: “Continue” button navigates to
    // glic/intro...#continue, “No thanks” button navigates to
    // glic/intro...#noThanks
    if (urlHash === '#continue') {
      freHandler.acceptFre();
    } else if (urlHash.startsWith('#noThanks')) {
      const source = url.searchParams.get('source');
      if (source === 'x_button') {
        chrome.metricsPrivate.recordUserAction(`Glic.Fre.CloseWithX`);
        freHandler.dismissFre(FreWebUiState.kReady);
      } else {
        freHandler.rejectFre();
      }
    }
  }

  onContentLoad() {
    // Immediately signal the C++ side that the content is loaded.
    freHandler.logWebClientLoaded();

    if (this.state === FreWebUiState.kBeginLoading ||
        this.state === FreWebUiState.kFinishLoading) {
      this.setState(FreWebUiState.kReady);
    } else if (this.state === FreWebUiState.kShowLoading) {
      this.setState(FreWebUiState.kHoldLoading);
    }
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
    ];
    if (allowedStates.includes(this.state)) {
      this.setState(FreWebUiState.kOffline);
    }
  }

  // Called when the "Try again" button is clicked.
  reload(): void {
    this.destroyWebview();
    this.useReloadTimeout = true;
    freHandler.freReloaded();
    this.setState(FreWebUiState.kBeginLoading);
  }

  private showPanel(id: PanelId): void {
    for (const panel of document.querySelectorAll<HTMLElement>('.panel')) {
      panel.hidden = panel.id !== id;
    }

    // After making the guest panel visible, programmatically move focus
    // to the content inside the webview. This ensures that screen readers
    // announce the new content.
    if (id === 'guestPanel') {
      this.webview.focus();
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
          this.useReloadTimeout = false;
          this.destroyWebview();
          this.showPanel('errorPanel');
        },
      },
    ],
    [
      FreWebUiState.kDisabledByAdmin,
      {
        onEnter: () => {
          this.destroyWebview();
          this.showPanel('disabledByAdminPanel');
        },
      },
    ],
    [
      FreWebUiState.kOffline,
      {
        onEnter: () => {
          this.useReloadTimeout = false;
          this.destroyWebview();
          this.showPanel('offlinePanel');
        },
      },
    ],
    [
      FreWebUiState.kReady,
      {
        onEnter: () => {
          this.useReloadTimeout = false;
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
    // Time at which to show the loading panel if the web client is not ready.
    const showLoadingTime = performance.now() + PRE_HOLD_LOADING_TIME_MS;

    // Attempt to re-sync cookies before continuing.
    const {success} = await freHandler.prepareForClient();
    if (!success) {
      this.setState(FreWebUiState.kError);
      return;
    }

    // Load the web client now that cookie sync is complete.
    this.destroyWebview();

    // Signal to the fre controller that the web ui framework has completed
    // loading and the remote web content is about to start loading in the
    // webview. This is used to record timing metrics.
    freHandler.logWebUiLoadComplete();

    this.webview.src = loadTimeData.getString('glicFreURL');
    this.loadingTimer = setTimeout(() => {
      this.setState(FreWebUiState.kShowLoading);
    }, Math.max(0, showLoadingTime - performance.now()));
  }

  showLoading(): void {
    this.showPanel('loadingPanel');
    // After `kMinHoldLoadingTimeMs`, transition to `kFinishLoading` or `kReady`
    // states. Note that we never transition from `kShowLoading` to `kReady`
    // before the timeout.
    this.earliestLoadingDismissTime =
        performance.now() + MIN_HOLD_LOADING_TIME_MS;
    this.loadingTimer = setTimeout(() => {
      this.setState(FreWebUiState.kFinishLoading);
    }, MIN_HOLD_LOADING_TIME_MS);
  }

  holdLoading(): void {
    // The web client is ready but we wait for the remainder of
    // `kMinHoldLoadingTimeMs` before showing it. This is to allow enough time
    // to view the loading animation.
    this.loadingTimer = setTimeout(() => {
      this.setState(FreWebUiState.kReady);
    }, Math.max(0, this.earliestLoadingDismissTime! - performance.now()));
  }

  finishLoading(): void {
    // The web client is not yet ready, so wait for the remainder of
    // `kMaxWaitTimeMs`. If a reload initiated by the user is being processed,
    // this max time is increased. Switch to the error state at that time
    // unless interrupted by `onContentLoad`, triggering the ready state.
    const timeoutValue = this.useReloadTimeout ?
        RELOAD_MAX_WAIT_TIME_RELOAD_MS :
        MAX_WAIT_TIME_MS;
    this.loadingTimer = setTimeout(() => {
      console.warn('Exceeded timeout in finishLoading');
      chrome.metricsPrivate.recordUserAction('Glic.Fre.WebviewLoadTimedOut');
      chrome.metricsPrivate.recordEnumerationValue(
          'Glic.Fre.WebviewLoadAbortReason',
          GlicFreWebviewLoadAbortReason.ERR_TIMED_OUT,
          GlicFreWebviewLoadAbortReason.MAX_VALUE + 1);
      freHandler.exceededTimeoutError();
      this.setState(FreWebUiState.kError);
    }, timeoutValue - MIN_HOLD_LOADING_TIME_MS);
  }

  onSizeChanged(e: any): void {
    window.resizeTo(e.newWidth, e.newHeight);
  }

  private createWebview(): chrome.webviewTag.WebView {
    const webview =
        document.createElement('webview') as chrome.webviewTag.WebView;
    webview.id = 'freGuestFrame';
    // TODO(crbug.com/408475473): Update the webviewTag definition to be able to
    // define properties rather than using setAttribute.
    webview.setAttribute('partition', 'glicfrepart');
    webview.setAttribute('autosize', 'true');
    webview.setAttribute('minwidth', INITIAL_WIDTH.toString());
    webview.setAttribute('maxwidth', INITIAL_WIDTH.toString());
    webview.setAttribute('minheight', MIN_HEIGHT.toString());
    webview.setAttribute('maxheight', window.screen.availHeight.toString());

    this.glicRequestHeaderInjector = new GlicRequestHeaderInjector(
        webview, loadTimeData.getString('chromeVersion'),
        loadTimeData.getString('chromeChannel'),
        loadTimeData.getString('glicHeaderRequestTypes'));

    $.webviewContainer.appendChild(webview);

    this.webviewEventTracker.add(
        webview, 'loadcommit', this.onLoadCommit.bind(this));
    this.webviewEventTracker.add(
        webview, 'contentload', this.onContentLoad.bind(this));
    this.webviewEventTracker.add(
        webview, 'loadabort', this.onLoadAbort.bind(this));
    this.webviewEventTracker.add(
        webview, 'newwindow', this.onNewWindow.bind(this));
    this.webviewEventTracker.add(
        webview, 'sizechanged', this.onSizeChanged.bind(this));

    return webview;
  }

  private reasonStringToEnum(reason: string|undefined):
      GlicFreWebviewLoadAbortReason {
    switch (reason) {
      case 'ERR_ABORTED':
        return GlicFreWebviewLoadAbortReason.ERR_ABORTED;
      case 'ERR_INVALID_URL':
        return GlicFreWebviewLoadAbortReason.ERR_INVALID_URL;
      case 'ERR_DISALLOWED_URL_SCHEME':
        return GlicFreWebviewLoadAbortReason.ERR_DISALLOWED_URL_SCHEME;
      case 'ERR_BLOCKED_BY_CLIENT':
        return GlicFreWebviewLoadAbortReason.ERR_BLOCKED_BY_CLIENT;
      case 'ERR_ADDRESS_UNREACHABLE':
        return GlicFreWebviewLoadAbortReason.ERR_ADDRESS_UNREACHABLE;
      case 'ERR_EMPTY_RESPONSE':
        return GlicFreWebviewLoadAbortReason.ERR_EMPTY_RESPONSE;
      case 'ERR_FILE_NOT_FOUND':
        return GlicFreWebviewLoadAbortReason.ERR_FILE_NOT_FOUND;
      case 'ERR_UNKNOWN_URL_SCHEME':
        return GlicFreWebviewLoadAbortReason.ERR_UNKNOWN_URL_SCHEME;
      case 'ERR_TIMED_OUT':
        return GlicFreWebviewLoadAbortReason.ERR_TIMED_OUT;
      case 'ERR_HTTP_RESPONSE_CODE_FAILURE':
        return GlicFreWebviewLoadAbortReason.ERR_HTTP_RESPONSE_CODE_FAILURE;
      default:
        return GlicFreWebviewLoadAbortReason.UNKNOWN;
    }
  }

  private onLoadAbort(e: any) {
    const reasonEnum = this.reasonStringToEnum(e.reason);
    chrome.metricsPrivate.recordUserAction('Glic.Fre.WebviewLoadAborted');
    chrome.metricsPrivate.recordEnumerationValue(
        'Glic.Fre.WebviewLoadAbortReason', reasonEnum,
        GlicFreWebviewLoadAbortReason.MAX_VALUE + 1);

    this.setState(FreWebUiState.kError);
  }

  private panelIdToEnum(panelId: string): FreWebUiState {
    switch (panelId) {
      case 'guestPanel':
        return FreWebUiState.kReady;
      case 'offlinePanel':
        return FreWebUiState.kOffline;
      case 'errorPanel':
        return FreWebUiState.kError;
      case 'loadingPanel':
        return FreWebUiState.kShowLoading;
      case 'disabledByAdminPanel':
        return FreWebUiState.kDisabledByAdmin;
      default:
        return FreWebUiState.kUninitialized;
    }
  }

  // Destroy the current webview and create a new one. This is necessary because
  // webview does not support unloading content by setting src=""
  destroyWebview(): void {
    this.webviewEventTracker.removeAll();

    if (this.glicRequestHeaderInjector) {
      this.glicRequestHeaderInjector.destroy();
      this.glicRequestHeaderInjector = undefined;
    }

    $.webviewContainer.removeChild(this.webview);

    this.webview = this.createWebview();
  }
}
