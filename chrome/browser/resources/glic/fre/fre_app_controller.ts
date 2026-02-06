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

// If unified FRE is enabled to change formatting for sidepanel ui.
const IS_UNIFIED_FRE = loadTimeData.getBoolean('isUnifiedFre');

// Minimum height for FRE.
const MIN_HEIGHT = 200;

export enum FreResultType {
  ACCEPT,
  DISMISS,
  REJECT,
}

export interface FreResult {
  type: FreResultType;
}


interface FreControllerOptions {
  partitionString?: string;
  shouldSizeForDialog?: boolean;
  onClose?: () => void;
}

type PanelId = 'freGuestPanel'|'freOfflinePanel'|'freErrorPanel'|
    'freLoadingPanel'|'freDisabledByAdminPanel';

interface StateDescriptor {
  onEnter?: () => void;
  onExit?: () => void;
}

export class FreAppController {
  state = FreWebUiState.kUninitialized;
  loadingTimer: number|undefined;

  // Created from constructor and never null since the destructor replaces it
  // with an empty <webview>.
  private webview: chrome.webviewTag.WebView;
  private webviewEventTracker = new EventTracker();
  private glicRequestHeaderInjector: GlicRequestHeaderInjector|undefined;
  private freHandler: FrePageHandlerRemote;

  // When entering loading state, this represents the earliest timestamp at
  // which the UI can transition to the ready state. This ensures that the
  // loading UI isn't just a brief flash on screen.
  private earliestLoadingDismissTime: number|undefined;

  // This is set when the "Try again" button in the error state is pressed,
  // indicating that a different timeout value should be used for the
  // subsequent content load. This value is reset when we the associated
  // content load ends.
  private useReloadTimeout = false;

  // Unified FRE variables.
  private freContainer: HTMLElement;
  private webviewContainer: HTMLElement;
  private partitionString: string;
  private shouldSizeForDialog: boolean;
  private onCloseCallback?: () => void;


  constructor(options: FreControllerOptions = {}) {
    this.onLoadCommit = this.onLoadCommit.bind(this);
    this.onContentLoad = this.onContentLoad.bind(this);
    this.onNewWindow = this.onNewWindow.bind(this);
    const container = getRequiredElement('fre-app-container');
    assert(container, '#fre-app-container not found in constructor');
    this.freContainer = container!;

    this.freHandler = new FrePageHandlerRemote();
    FrePageHandlerFactory.getRemote().createPageHandler(
        this.freHandler.$.bindNewPipeAndPassReceiver());

    this.webviewContainer = getRequiredElement('freWebviewContainer')!;
    assert(
        this.webviewContainer, '#freWebviewContainer not found in constructor');
    this.partitionString = options.partitionString ?? 'glicfrepart';
    this.shouldSizeForDialog = options.shouldSizeForDialog ?? true;
    this.onCloseCallback = options.onClose;


    this.webview = this.createWebview();

    // TODO(b/459795708): Remove when FRE is deduplicated and unified fre is
    // launched.
    const frePanelStateKindSection = getRequiredElement('fre-local-panels');
    frePanelStateKindSection.classList.toggle('side-panel', IS_UNIFIED_FRE);
    frePanelStateKindSection.classList.toggle('floating', !IS_UNIFIED_FRE);

    window.addEventListener('online', () => {
      this.online();
    });
    window.addEventListener('offline', () => {
      this.offline();
    });
    window.addEventListener('load', () => {
      // Allow WebUI close buttons to close the window. Close buttons are
      // present on all UI states except for `FreWebUiState.kReady`.
      const buttons = this.freContainer.querySelectorAll('.close-button');
      for (const button of buttons) {
        const parentPanel = button.closest('.panel');
        if (parentPanel) {
          button.addEventListener('click', () => {
            chrome.metricsPrivate.recordUserAction('Glic.Fre.CloseWithX');
            this.dismissFre(this.panelIdToEnum(parentPanel.id));
          });
        }
      }

      const disabledByAdminButton =
          getRequiredElement('freDisabledByAdminCloseButton');
      assert(disabledByAdminButton);

      const parentPanel = disabledByAdminButton.closest('.panel');
      assert(parentPanel);

      disabledByAdminButton.addEventListener('click', () => {
        chrome.metricsPrivate.recordUserAction(
            'Glic.Fre.DisabledByAdminPanelCloseButton');
        this.dismissFre(this.panelIdToEnum(parentPanel.id));
      });

      const disabledByAdminLink =
          this.freContainer.querySelector<HTMLAnchorElement>(
              '#freDisabledByAdminPanel a');
      assert(disabledByAdminLink);
      disabledByAdminLink.addEventListener(
          'click', (e) => {
            e.preventDefault();
            chrome.metricsPrivate.recordUserAction(
                'Glic.Fre.DisabledByAdminPanelLinkClicked');
            this.freHandler.validateAndOpenLinkInNewTab({
              url: (e.target as HTMLAnchorElement).href,
            });
            e.stopPropagation();
          });

      getRequiredElement('fre-reload')?.addEventListener('click', () => {
        this.reload();
      });
    });

    this.freContainer.addEventListener('keydown', (ev: KeyboardEvent) => {
      if (ev.code === 'Escape') {
        ev.stopPropagation();
        ev.preventDefault();
        const visiblePanel = this.freContainer.querySelector<HTMLElement>(
            '.panel:not([hidden])');
        if (visiblePanel) {
          chrome.metricsPrivate.recordUserAction('Glic.Fre.CloseWithEsc');
          this.dismissFre(this.panelIdToEnum(visiblePanel.id));
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
      this.acceptFre();
    } else if (urlHash.startsWith('#noThanks')) {
      const source = url.searchParams.get('source');
      if (source === 'x_button') {
        chrome.metricsPrivate.recordUserAction(`Glic.Fre.CloseWithX`);
        this.dismissFre(FreWebUiState.kReady);
      } else {
        this.rejectFre();
      }
    }
  }

  onContentLoad() {
    if (this.state === FreWebUiState.kBeginLoading ||
        this.state === FreWebUiState.kFinishLoading) {
      this.setState(FreWebUiState.kReady);
    } else if (this.state === FreWebUiState.kShowLoading) {
      this.setState(FreWebUiState.kHoldLoading);
    }
  }

  onNewWindow(e: any) {
    e.preventDefault();
    this.freHandler.validateAndOpenLinkInNewTab({
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
    this.freHandler.freReloaded();
    this.setState(FreWebUiState.kBeginLoading);
  }

  private showPanel(id: PanelId): void {
    for (const panel of this.freContainer.querySelectorAll<HTMLElement>(
             '.panel')) {
      panel.hidden = panel.id !== id;
    }

    const frePanelStateKindSection = getRequiredElement('fre-local-panels');
    frePanelStateKindSection.classList.toggle('hidden', id === 'freGuestPanel');

    // After making the guest panel visible, programmatically move focus
    // to the content inside the webview. This ensures that screen readers
    // announce the new content.
    if (id === 'freGuestPanel') {
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
    this.freHandler.webUiStateChanged(newState);
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
          this.showPanel('freErrorPanel');
        },
      },
    ],
    [
      FreWebUiState.kDisabledByAdmin,
      {
        onEnter: () => {
          this.destroyWebview();
          this.showPanel('freDisabledByAdminPanel');
        },
      },
    ],
    [
      FreWebUiState.kOffline,
      {
        onEnter: () => {
          this.useReloadTimeout = false;
          this.destroyWebview();
          this.showPanel('freOfflinePanel');
        },
      },
    ],
    [
      FreWebUiState.kReady,
      {
        onEnter: () => {
          this.useReloadTimeout = false;
          this.showPanel('freGuestPanel');
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
    const {success} = await this.freHandler.prepareForClient();
    if (!success) {
      this.setState(FreWebUiState.kError);
      return;
    }

    // Load the web client now that cookie sync is complete.
    this.destroyWebview();

    // Signal to the fre controller that the web ui framework has completed
    // loading and the remote web content is about to start loading in the
    // webview. This is used to record timing metrics.
    this.freHandler.logWebUiLoadComplete();

    const glicFreURL = new URL(loadTimeData.getString('glicFreURL'));
    // If `shouldSizeForDialog` is false, this indicates the side panel context.
    // Append a query parameter to notify the webview of this context.
    if (!this.shouldSizeForDialog) {
      glicFreURL.searchParams.append('sidepanelFre', 'true');
    }
    this.webview.src = glicFreURL.toString();

    this.loadingTimer = setTimeout(() => {
      this.setState(FreWebUiState.kShowLoading);
    }, Math.max(0, showLoadingTime - performance.now()));
  }

  showLoading(): void {
    this.showPanel('freLoadingPanel');
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
      this.freHandler.exceededTimeoutError();
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
    webview.setAttribute('partition', this.partitionString);
    webview.setAttribute('autosize', 'true');
    if (this.shouldSizeForDialog) {
      // Relax the min/max wiidth constraints on the `<webview>`. For
      // certain device scale factors, when converting back and forth between
      // DIPs and pixels, we can have rounding errors which can break the auto
      // resizing FRE dialog. (See b:480783053 for more details)
      webview.setAttribute(
          'minwidth',
          (loadTimeData.getInteger('freInitialWidth') - 1).toString());
      webview.setAttribute(
          'maxwidth',
          (loadTimeData.getInteger('freInitialWidth') + 1).toString());
      webview.setAttribute('minheight', MIN_HEIGHT.toString());
      webview.setAttribute('maxheight', window.screen.availHeight.toString());
    }
    this.glicRequestHeaderInjector = new GlicRequestHeaderInjector(
        webview, loadTimeData.getString('chromeVersion'),
        loadTimeData.getString('chromeChannel'),
        loadTimeData.getString('glicHeaderRequestTypes'));

    this.webviewContainer.appendChild(webview);

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
      case 'freGuestPanel':
        return FreWebUiState.kReady;
      case 'freOfflinePanel':
        return FreWebUiState.kOffline;
      case 'freErrorPanel':
        return FreWebUiState.kError;
      case 'freLoadingPanel':
        return FreWebUiState.kShowLoading;
      case 'freDisabledByAdminPanel':
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

    this.webviewContainer.removeChild(this.webview);

    this.webview = this.createWebview();
  }

  private dismissFre(state: FreWebUiState): void {
    this.freHandler.dismissFre(state);
    this.onCloseCallback?.();
  }

  private acceptFre(): void {
    this.freHandler.acceptFre();
  }

  private rejectFre(): void {
    this.freHandler.rejectFre();
    this.onCloseCallback?.();
  }
}
