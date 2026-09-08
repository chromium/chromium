// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {BrowserProxyImpl} from './browser_proxy.js';
import {PanelStateKind} from './glic_enums.mojom-webui.js';
import {GuestPageType, HelpCenterTopic, PrepareForClientResult, ProfileReadyState, WebClientState, WebUiState} from './glic_webui.mojom-webui.js';
import type {ZoomAction} from './glic_webui.mojom-webui.js';
import type {WebviewDelegate} from './webview.js';
import {WebviewController, WebviewPersistentState} from './webview.js';
// Time to wait before showing loading panel.
const kPreHoldLoadingTimeMs = loadTimeData.getInteger('preLoadingTimeMs');

// Minimum time to hold "loading" panel visible.
const kMinHoldLoadingTimeMs = loadTimeData.getInteger('minLoadingTimeMs');

// Maximum time to wait for load before showing error panel.
const kMaxWaitTimeMs = loadTimeData.getInteger('maxLoadingTimeMs');

// Whether to enable the debug button on the error panel. Can be enabled with
// the --enable-features=GlicDebugWebview command-line flag.
const kEnableDebug = loadTimeData.getBoolean('enableDebug');

const kShowErrorAllowed = loadTimeData.getBoolean('showErrorAllowed');

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
  showError: HTMLButtonElement;
  locationMismatchPanel: HTMLElement;
  locationMismatchHelpButton: HTMLButtonElement;
  ineligibleAccountHelpButton: HTMLButtonElement;
  ineligibleAccountPanel: HTMLElement;
}

const $: PageElementTypes = new Proxy({}, {
                              get(_target: object, prop: string) {
                                return getRequiredElement(prop);
                              },
                            }) as unknown as PageElementTypes;

type PanelId = 'loadingPanel'|'guestPanel'|'offlinePanel'|'errorPanel'|
    'unavailablePanel'|'ineligibleAccountPanel'|'disabledByAdminPanel'|
    'signInPanel'|'locationMismatchPanel';

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
// Enum for specific stages of loading the web client, reported if loading times
// out.
// LINT.IfChange(LoadingStage)
export enum LoadingStage {
  NOT_LOADING = 0,
  AWAITING_PROFILE_READY = 1,
  AWAITING_COOKIE_SYNC = 2,
  LOADING_WEB_CLIENT = 3,
  AWAITING_NOTIFY_PANEL_WILL_OPEN = 4,
  MAX_VALUE = AWAITING_NOTIFY_PANEL_WILL_OPEN,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:LoadingStage,//tools/metrics/histograms/metadata/glic/histograms.xml:LoadingStage)

// Reasons for entering WebUiState.kError.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PanelWebUiStateErrorReason)
export enum WebUiErrorReason {
  WEBVIEW_ERROR = 0,
  LOAD_ERROR = 1,
  COOKIE_SYNC_ERROR = 2,
  TIMEOUT_NOTIFY_PANEL_WILL_OPEN = 3,
  TIMEOUT_LOADING_CLIENT = 4,
  TIMEOUT_WARMED = 5,
  CLIENT_ERROR = 6,
  CLOSE_DEBUG_VIEW = 7,
  MAX_VALUE = CLOSE_DEBUG_VIEW,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:PanelWebUiStateErrorReason)

export class GlicAppController implements WebviewDelegate {
  loadingTimer: number|undefined;
  private isFreCompleted: boolean = loadTimeData.getBoolean('completedFre');

  // This is used to simulate no connection for tests.
  private simulateNoConnection: boolean =
      loadTimeData.getBoolean('simulateNoConnection');

  private guestResizeEnabled: boolean = false;

  // Present only when loading or after loading is finished. Removed on error.
  private webview?: WebviewController;
  get webviewForTesting(): WebviewController|undefined {
    return this.webview;
  }
  private webviewPersistentState = new WebviewPersistentState();

  private profileReadyState: ProfileReadyState|undefined = undefined;
  private profileReadyInitialState = Promise.withResolvers<void>();

  // Loading stage, affects metrics only.
  private loadingStage: LoadingStage = LoadingStage.NOT_LOADING;
  private loadingStageStartTimestampMs?: DOMHighResTimeStamp;

  private panelStateKind: PanelStateKind = PanelStateKind.kHidden;

  state: WebUiState|undefined;

  // When entering loading state, this represents the earliest timestamp at
  // which the UI can transition to the ready state. This ensures that the
  // loading UI isn't just a brief flash on screen.
  private earliestLoadingDismissTime: number|undefined;

  browserProxy: BrowserProxyImpl;

  constructor(browserProxy: BrowserProxyImpl) {
    this.browserProxy = browserProxy;

    window.addEventListener('online', () => {
      this.online();
    });
    window.addEventListener('offline', () => {
      if (!this.isOnline()) {
        this.offline();
      }
    });

    // Programmatically redirect focus to the embedded guest
    // webview if focus gets trapped on the orchestrator container
    // (document.body) while the guest panel is visible.
    window.addEventListener('focus', () => {
      const isGuestVisible = !$.guestPanel.hidden;
      const isFocusTrapped =
          document.activeElement === document.body || !document.activeElement;

      if (isGuestVisible && isFocusTrapped) {
        this.webview?.focus();
      }
    });

    if (this.isOnline()) {
      this.setState(WebUiState.kBeginLoad);
    } else {
      this.setState(WebUiState.kOffline);
    }
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
    $.locationMismatchHelpButton.addEventListener('click', () => {
      this.browserProxy.pageHandler.openHelpCenterTopicAndClosePanel(
          HelpCenterTopic.kLocationMismatch);
    });
    $.ineligibleAccountHelpButton.addEventListener('click', () => {
      this.browserProxy.pageHandler.openHelpCenterTopicAndClosePanel(
          HelpCenterTopic.kIneligibleAccount);
    });
    $.signInButton.addEventListener('click', () => {
      this.signIn();
    });
    $.showError.addEventListener('click', () => {
      this.showPanel('guestPanel');
    });

    this.browserProxy.preloadPageCallbackRouter.onGuestNavigationStarted
        .addListener(() => {
          this.onGuestNavigationStarted();
        });

    this.browserProxy.preloadPageCallbackRouter.onGuestNavigated.addListener(
        (url: string, isApiAllowed: boolean, pageType: GuestPageType,
         isInitialCommit: boolean) => {
          this.onGuestNavigated(url, isApiAllowed, pageType, isInitialCommit);
        });

    this.browserProxy.preloadPageCallbackRouter.onGuestProcessGone.addListener(
        () => {
          this.webviewError('guest process gone');
        });

    if (kShowErrorAllowed) {
      $.showError.hidden = false;
    }

    document.addEventListener('keydown', ev => {
      if (this.state !== WebUiState.kReady) {
        if (ev.code === 'Escape') {
          ev.stopPropagation();
          ev.preventDefault();
          this.browserProxy.pageHandler.closePanel();
        }
      }
    });

    if (kEnableDebug) {
      window.addEventListener('load', () => {
        this.installDebugButton();
      });
    }
    this.initializeIcons_();
  }

  webviewError(reason: string): void {
    console.warn(`webview exit. reason: ${reason}`);
    this.setErrorState(WebUiErrorReason.WEBVIEW_ERROR);
  }

  onGuestNavigationStarted(): void {
    this.webview?.onGuestNavigationStarted();
  }

  onGuestNavigated(
      url: string, isApiAllowed: boolean, pageType: GuestPageType,
      isInitialCommit: boolean): void {
    this.webview?.onGuestNavigated(
        url, isApiAllowed, pageType, isInitialCommit);
  }

  webviewPageCommit(type: GuestPageType, _isApiAllowed: boolean) {
    switch (type) {
      case GuestPageType.kLogin:
        this.cancelTimeout();
        $.guestPanel.classList.toggle('show-header', true);
        this.showPanel('guestPanel');
        break;
      case GuestPageType.kGuestError:
        this.setState(WebUiState.kGuestError);
        break;
      case GuestPageType.kRegular:
        $.guestPanel.classList.toggle('show-header', false);
        if (loadTimeData.getBoolean('reloadAfterNavigation')) {
          if (this.state === WebUiState.kReady ||
              this.state === WebUiState.kWarmed ||
              this.state === WebUiState.kGuestError) {
            this.setState(WebUiState.kBeginLoad);
          }
        }
        break;
      case GuestPageType.kLoadError:
        this.setErrorState(WebUiErrorReason.LOAD_ERROR);
        break;
      case GuestPageType.kDisabledByAdmin:
        this.webviewDeniedByAdmin();
        break;
      default:
        assertNotReachedCase(type);
    }
  }

  webviewDeniedByAdmin() {
    $.disabledByAdminPanel.classList.toggle(
        'show-disabled-by-admin-link', true);
    this.setState(WebUiState.kDisabledByAdmin);
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
    this.browserProxy.pageHandler.onWebUiStateChanged(this.state);
    this.browserProxy.pageHandler.enableDragResize(
        this.state === WebUiState.kReady && this.guestResizeEnabled);
  }

  private setErrorState(reason: WebUiErrorReason): void {
    // Only record the histogram if not already in the error state.
    if (this.state === WebUiState.kError) {
      return;
    }
    chrome.histograms.recordEnumerationValue(
        'Glic.PanelWebUiState.Error',
        reason,
        WebUiErrorReason.MAX_VALUE + 1,
    );
    if (!this.isFreCompleted) {
      chrome.histograms.recordEnumerationValue(
          'Glic.Fre.PanelWebUiState.Error',
          reason,
          WebUiErrorReason.MAX_VALUE + 1,
      );
    }
    this.setState(WebUiState.kError);
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
              // Keep the webview alive for debugging purposes.
              this.setWebviewDormant();
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
      WebUiState.kIneligibleAccount,
      {
        reloadOnOpen: true,
        onEnter:
            () => {
              this.destroyWebview();
              this.showPanel('ineligibleAccountPanel');
            },
      },
    ],
    [
      WebUiState.kDisabledByAdmin,
      {
        reloadOnOpen: true,
        onEnter:
            () => {
              this.destroyWebview();
              this.showPanel('disabledByAdminPanel');
            },
      },
    ],
    [
      WebUiState.kLocationMismatch,
      {
        reloadOnOpen: true,
        onEnter:
            () => {
              this.destroyWebview();
              this.showPanel('locationMismatchPanel');
            },
      },
    ],
    [
      WebUiState.kWarmed,
      {
        onEnter: () => {
          $.guestPanel.classList.toggle('show-header', false);
          this.showPanel('guestPanel');
        },
      },
    ],
    [
      WebUiState.kReady,
      {
        onEnter: () => {
          this.trackLoadingStageEnd();
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
    if (!loadTimeData.getBoolean('noLoader')) {
      this.loadingTimer = setTimeout(() => {
        if (this.state === WebUiState.kWarmed ||
            this.state === WebUiState.kReady) {
          return;
        }
        this.setState(WebUiState.kShowLoading);
      }, kPreHoldLoadingTimeMs);
    }

    this.load();
  }

  private trackLoadingStageStart(newStage: LoadingStage) {
    this.loadingStage = newStage;
    this.loadingStageStartTimestampMs = performance.now();
  }

  private trackLoadingStageEnd() {
    if (this.loadingStage === LoadingStage.NOT_LOADING) {
      return;
    }

    chrome.histograms.recordMediumTime(
        'Glic.Host.LoadingStageDuration.' +
            LoadingStage[this.getLoadingStage()],
        Math.floor(performance.now() - this.loadingStageStartTimestampMs!));
    this.loadingStage = LoadingStage.NOT_LOADING;
  }

  private getLoadingStage(): LoadingStage {
    return this.loadingStage;
  }

  private async load(): Promise<void> {
    this.destroyWebview();

    // profileReadyState isn't available right away. Wait until it's ready.
    this.trackLoadingStageStart(LoadingStage.AWAITING_PROFILE_READY);
    await this.profileReadyInitialState.promise;
    this.trackLoadingStageEnd();

    const readyState = this.profileReadyState;
    assert(readyState !== undefined);
    switch (readyState) {
      case ProfileReadyState.kIneligible:
      case ProfileReadyState.kUnknownError:
        this.setState(WebUiState.kUnavailable);
        return;
      case ProfileReadyState.kIneligibleAccount:
        this.setState(WebUiState.kIneligibleAccount);
        return;
      case ProfileReadyState.kLocationMismatch:
        this.setState(WebUiState.kLocationMismatch);
        return;
      case ProfileReadyState.kDisabledByAdmin:
        $.disabledByAdminPanel.classList.toggle(
            'show-disabled-by-admin-link', false);
        this.setState(WebUiState.kDisabledByAdmin);
        return;
      case ProfileReadyState.kSignInRequired:
        this.setState(WebUiState.kSignIn);
        return;
      case ProfileReadyState.kReady:
        break;
      default:
        assertNotReachedCase(readyState);
    }

    // Blocking on cookie syncing here introduces latency, we should consider
    // ways to avoid it.
    this.trackLoadingStageStart(LoadingStage.AWAITING_COOKIE_SYNC);
    const {result} = this.browserProxy.glicPreloadHandler ?
        await this.browserProxy.glicPreloadHandler.prepareForClient() :
        await this.browserProxy.pageHandler.prepareForClient();
    this.trackLoadingStageEnd();

    switch (result) {
      case PrepareForClientResult.kSuccess:
        break;
      case PrepareForClientResult.kErrorResyncingCookies:
        console.warn('prepareForClient in beginLoad() failed.');
        this.setErrorState(WebUiErrorReason.COOKIE_SYNC_ERROR);

        return;
      case PrepareForClientResult.kRequiresSignIn:
        this.setState(WebUiState.kSignIn);
        return;
      default:
        assertNotReachedCase(result);
    }

    // Load the web client only after cookie sync is complete.
    this.trackLoadingStageStart(LoadingStage.LOADING_WEB_CLIENT);
    this.webview = new WebviewController(
        $.webviewContainer, this.browserProxy, this,
        this.webviewPersistentState);
    this.webview.getWebClientState().subscribe(
        this.webClientStateChanged.bind(this));

    if (loadTimeData.getBoolean('noLoader')) {
      this.showPanel('guestPanel');
    }

    // Browser is expected to call client's notifyPanelWillOpen(), and then we
    // expect a call to webClientReady() when that finishes.
  }

  private showLoading(): void {
    if (this.state === WebUiState.kWarmed) {
      return;
    }
    this.showPanel('loadingPanel');
    this.earliestLoadingDismissTime = performance.now() + kMinHoldLoadingTimeMs;
    if (this.webview?.getWebClientState().getCurrentValue() ===
        WebClientState.kResponsive) {
      if (this.panelStateKind === PanelStateKind.kHidden) {
        this.setState(WebUiState.kWarmed);
        return;
      }
      this.setState(WebUiState.kHoldLoading);
      return;
    }
    // After kMinHoldLoadingTimeMs, transition to finish-loading or ready. Note
    // that we do not transition from show-loading to ready before the timeout.
    this.earliestLoadingDismissTime = performance.now() + kMinHoldLoadingTimeMs;
    this.loadingTimer = setTimeout(() => {
      if (this.state === WebUiState.kWarmed ||
          this.state === WebUiState.kReady) {
        return;
      }
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
    if (this.state === WebUiState.kWarmed) {
      return;
    }
    if (this.panelStateKind === PanelStateKind.kHidden) {
      this.cancelTimeout();
      this.trackLoadingStageEnd();
      this.setState(WebUiState.kWarmed);
      return;
    }
    // The web client is not yet ready, so wait for the remainder of
    // `kMaxWaitTimeMs`. Switch to error state at that time unless interrupted
    // by `webClientReady`.
    this.loadingTimer = setTimeout(() => {
      console.warn('Exceeded timeout waiting for client to load');
      this.setErrorState(WebUiErrorReason.TIMEOUT_LOADING_CLIENT);

      if (this.state !== WebUiState.kReady) {
        chrome.histograms.recordEnumerationValue(
            'Glic.Host.LoadingTimedOut', this.getLoadingStage(),
            LoadingStage.MAX_VALUE + 1);
      }
      this.trackLoadingStageEnd();
    }, kMaxWaitTimeMs - kMinHoldLoadingTimeMs);
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

    // Focus the webview when the guest panel is shown.
    // b/475260887: webview.focus() won't focus the client page if the
    // <webview> element is invisible (due to an ancestor element having
    // display: none or HTML hidden attribute).
    if (id === 'guestPanel') {
      this.webview?.focus();
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

  private setWebviewDormant(): void {
    // Never allow dormant state when the panel is hidden.
    if (this.panelStateKind === PanelStateKind.kHidden) {
      this.destroyWebview();
      return;
    }
    this.webview?.setDormant();
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

  private startWarmedTimeout(): void {
    this.cancelTimeout();
    if (this.panelStateKind === PanelStateKind.kHidden) {
      return;
    }
    this.loadingTimer = setTimeout(() => {
      if (this.state === WebUiState.kWarmed) {
        this.setErrorState(WebUiErrorReason.TIMEOUT_WARMED);
      }
    }, kMaxWaitTimeMs);
  }

  webClientStateChanged(state: WebClientState): void {
    switch (state) {
      case WebClientState.kUninitialized:
        break;
      case WebClientState.kWarmed:
        if (this.state === WebUiState.kBeginLoad ||
            this.state === WebUiState.kFinishLoading ||
            this.state === WebUiState.kShowLoading) {
          this.cancelTimeout();
          this.setState(WebUiState.kWarmed);
          if (this.panelStateKind !== PanelStateKind.kHidden) {
            this.startWarmedTimeout();
          }
        }
        break;
      case WebClientState.kResponsive:
        if (this.state === WebUiState.kUnresponsive) {
          this.setState(WebUiState.kReady);
        }
        break;
      case WebClientState.kUnresponsive:
        break;
      case WebClientState.kError:
        this.guestResizeEnabled = false;
        this.setErrorState(WebUiErrorReason.CLIENT_ERROR);
        break;
      default:
        assertNotReachedCase(state);
    }
  }

  // External entry points.

  showDebug(): void {
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
      this.setErrorState(WebUiErrorReason.CLOSE_DEBUG_VIEW);

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
    this.destroyWebview();
    // TODO: Allow the timeout on this load to be longer than the initial load.
    this.setState(WebUiState.kBeginLoad);
  }

  private openProfilePicker(): void {
    this.browserProxy.pageHandler.openProfilePickerAndClosePanel();
  }

  private signIn(): void {
    this.browserProxy.pageHandler.signInAndClosePanel();
  }

  clientReadyStateChanged(ready: boolean): void {
    if (!ready) {
      return;
    }
    if (this.state === WebUiState.kBeginLoad ||
        this.state === WebUiState.kFinishLoading ||
        this.state === WebUiState.kWarmed) {
      this.cancelTimeout();
      this.trackLoadingStageEnd();
      this.setState(WebUiState.kReady);
    } else if (this.state === WebUiState.kShowLoading) {
      this.cancelTimeout();
      this.setState(WebUiState.kHoldLoading);
    } else if (this.state !== WebUiState.kHoldLoading) {
      this.setState(WebUiState.kReady);
    }
  }

  updatePageState(panelStateKind: PanelStateKind) {
    if (this.panelStateKind === panelStateKind) {
      return;
    }
    this.panelStateKind = panelStateKind;

    if (this.panelStateKind !== PanelStateKind.kHidden &&
        this.state === WebUiState.kWarmed) {
      this.startWarmedTimeout();
    }

    const panelStateKindSection = getRequiredElement('localPanels');
    panelStateKindSection.classList.toggle(
        'sidePanel', this.panelStateKind === PanelStateKind.kAttached);
    panelStateKindSection.classList.toggle(
        'floating', this.panelStateKind === PanelStateKind.kDetached);
  }

  zoom(zoomAction: ZoomAction) {
    this.webview?.zoom(zoomAction);
  }

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
        case ProfileReadyState.kIneligible:
          this.setState(WebUiState.kUnavailable);
          break;
        case ProfileReadyState.kIneligibleAccount:
          this.setState(WebUiState.kIneligibleAccount);
          break;
        case ProfileReadyState.kLocationMismatch:
          this.setState(WebUiState.kLocationMismatch);
          break;
        case ProfileReadyState.kDisabledByAdmin:
          $.disabledByAdminPanel.classList.toggle(
              'show-disabled-by-admin-link', false);
          this.setState(WebUiState.kDisabledByAdmin);
          break;
        case ProfileReadyState.kSignInRequired:
          this.setState(WebUiState.kSignIn);
          break;
        case ProfileReadyState.kReady:
          if (this.stateDescriptor()?.reloadOnOpen) {
            this.setState(WebUiState.kBeginLoad);
          }
          break;
        default:
          assertNotReachedCase(this.profileReadyState);
      }
    }
  }

  openDisabledByAdminLink(): void {
    this.browserProxy.pageHandler.openDisabledByAdminLinkAndClosePanel();
  }

  isOnline() {
    return loadTimeData.getBoolean('ignoreOfflineState') ?
        true :
        navigator.onLine && !this.simulateNoConnection;
  }

  private initializeIcons_() {
    const isRounded = loadTimeData.getBoolean('webuiRoundedIconsEnabled');
    const updateIcon =
        (panel: HTMLElement, roundedIcon: string, oldIcon: string) => {
          const el = panel.querySelector('cr-icon');
          if (el) {
            el.setAttribute('icon', isRounded ? roundedIcon : oldIcon);
          }
        };
    updateIcon($.offlinePanel, 'glic:wifi-off', 'glic:offline-old');
    updateIcon($.errorPanel, 'glic:error', 'glic:error-old');
    updateIcon(
        $.unavailablePanel, 'glic:person-alert', 'glic:person-alert-old');
    updateIcon(
        $.ineligibleAccountPanel, 'glic:do-not-touch',
        'glic:ineligible-account-old');
    updateIcon($.signInPanel, 'glic:person-alert', 'glic:person-alert-old');
    updateIcon(
        $.locationMismatchPanel, 'glic:location-on',
        'glic:location-mismatch-old');
  }
}
