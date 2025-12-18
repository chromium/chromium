// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxy} from './browser_proxy.js';
import {PrepareForClientResult, ProfileReadyState} from './glic.mojom-webui.js';
import type {Subscriber} from './glic_api/glic_api.js';
import type {ApiHostEmbedder} from './glic_api_impl/host/glic_api_host.js';
import {WebClientState} from './glic_api_impl/host/glic_api_host.js';
import type {ObservableValueReadOnly} from './observable.js';
import {ObservableValue} from './observable.js';
import {OneShotTimer} from './timer.js';
import {WebviewController, WebviewPersistentState} from './webview.js';
import type {PageType, WebviewControllerInterface, WebviewDelegate} from './webview.js';

// Status of the webview loading process.
export enum GlicWebviewLoadStatus {
  // Webview is destroyed, and not trying to load.
  // Stays in this state if loading preconditions are not met:
  //  * device is online
  //  * loading was requested with setWantLoad(true)
  //  * the profile is ready
  NOT_LOADED = 0,
  // Working on loading.
  LOADING = 1,
  // The client has been initialized and is responsive.
  RESPONSIVE = 2,
  // The client has been initialized, but has become unresponsive.
  UNRESPONSIVE = 3,
  // The load failed, and the webview has been destroyed again.
  // Loading is not attempted again unless the preconditions mentioned above
  // change.
  ERROR = 4,
  // A login page has been reached, and is allowed by `allowLoginPages`.
  AT_LOGIN_PAGE = 5,
  // An error page provided by the guest that should be shown.
  AT_GUEST_ERROR_PAGE = 6,

  MAX_VALUE = AT_LOGIN_PAGE,
}

export enum GlicWebviewLoadErrorReason {
  UNKNOWN = 0,
  BLOCKED_BY_NEED_SIGN_IN = 1,
  BLOCKED_BY_SYNC_ERROR = 2,
  PROFILE_INELIGIBLE = 3,
  LOGIN_PAGE_REACHED = 4,
  PAGE_LOAD_ERROR = 5,
  DISABLED_BY_ADMIN = 6,
  NOT_ONLINE = 7,
}

export interface GlicWebviewLoadState {
  status: GlicWebviewLoadStatus;
  // Provided if status is `ERROR`.
  errorReason?: GlicWebviewLoadErrorReason;
}

export type OnlineMonitor = ObservableValueReadOnly<boolean>;

export class OnlineMonitorImpl extends ObservableValue<boolean> {
  constructor() {
    super(true, navigator.onLine);
    window.addEventListener('online', () => {
      // When using `ignoreOfflineState`, we don't trust the offline state. But
      // if we get the signal that we're back online, we can re-send the online
      // signal to trigger a retry.
      this.assignAndSignal(this.isOnline(), /*force=*/ true);
    });
    window.addEventListener('offline', () => {
      this.assignAndSignal(this.isOnline());
    });
  }
  private isOnline() {
    if (loadTimeData.getBoolean('ignoreOfflineState')) {
      return true;
    }
    return navigator.onLine &&
        // This is used to simulate no connection for tests.
        !loadTimeData.getBoolean('simulateNoConnection');
  }
}

// TODO(b/454120908): More work to do here.
// * Use this outside of GlicAppController
// * Support lazy setting of ApiHostEmbedder, and only create the host
//   after that point.
// * Remove onLoadTimeOut(), somehow, it's ugly
// * waitingOnPanelWillOpen() looks error-prone, can we do better?
export class GlicWebviewLoader implements WebviewDelegate {
  // Maximum time to wait for load before showing error panel.
  private readonly kMaxWaitTimeMs = loadTimeData.getInteger('maxLoadingTimeMs');

  private persistentState = new WebviewPersistentState();
  wantLoad = false;
  private loadTimer = new OneShotTimer(this.kMaxWaitTimeMs);
  webview?: WebviewControllerInterface;
  webviewHasClient = false;
  profileReadyState?: ProfileReadyState;
  allowLoginPages = true;
  private webClientStateSubscription?: Subscriber;
  // Should only be used in setState().
  private mutableStateForSetState =
      ObservableValue.withValue<GlicWebviewLoadState>(
          {status: GlicWebviewLoadStatus.NOT_LOADED});
  private state: ObservableValueReadOnly<GlicWebviewLoadState> =
      this.mutableStateForSetState;
  private loadTask?: Task<void>;

  constructor(
      private readonly container: HTMLElement,
      protected browserProxy: BrowserProxy,
      private hostEmbedder: ApiHostEmbedder,
      private isOnline: OnlineMonitor,
  ) {
    this.isOnline.subscribe(() => {
      this.updateLoadState();
    });
  }

  currentStatus(): GlicWebviewLoadStatus {
    return this.state.getCurrentValue()?.status ??
        GlicWebviewLoadStatus.NOT_LOADED;
  }

  private setState(state: GlicWebviewLoadState): void {
    const current = this.state.getCurrentValue();
    if (state.status === current?.status &&
        state.errorReason === current?.errorReason) {
      return;
    }
    this.loadTask?.cancel();
    this.loadTask = undefined;
    switch (state.status) {
      case GlicWebviewLoadStatus.NOT_LOADED:
        this.doUnload();
        break;
      case GlicWebviewLoadStatus.LOADING:
        this.loadTask = this.doLoad();
        break;
      case GlicWebviewLoadStatus.RESPONSIVE:
        this.loadTimer.reset();
        break;
      case GlicWebviewLoadStatus.UNRESPONSIVE:
        break;
      case GlicWebviewLoadStatus.ERROR:
        this.doUnload();
        break;
      case GlicWebviewLoadStatus.AT_LOGIN_PAGE:
        break;
      case GlicWebviewLoadStatus.AT_GUEST_ERROR_PAGE:
        break;
      default:
        assertNotReachedCase(state.status);
    }
    this.mutableStateForSetState.assignAndSignal(state);
  }

  reload(): void {
    if (this.wantLoad) {
      this.setWantLoad(false);
      this.setWantLoad(true);
    }
  }

  setWantLoad(wantLoad: boolean): void {
    if (this.wantLoad === wantLoad) {
      return;
    }
    this.wantLoad = wantLoad;
    this.updateLoadState();
  }

  stopLoadingDueToTimeout(): void {
    this.webview?.onLoadTimeOut();
    this.doUnload();
    this.setState({status: GlicWebviewLoadStatus.NOT_LOADED});
  }

  waitingOnPanelWillOpen(): boolean {
    return this.webview?.waitingOnPanelWillOpen() ?? false;
  }

  setProfileReadyState(state: ProfileReadyState) {
    this.profileReadyState = state;
    this.updateLoadState();
  }

  updateLoadState(): void {
    // Always unload if not wantLoad.
    if (!this.wantLoad) {
      this.setState({status: GlicWebviewLoadStatus.NOT_LOADED});
      return;
    }

    // Want load, and already loaded.
    if (this.webview) {
      const unloadState = this.checkShouldUnload();
      if (unloadState !== undefined) {
        this.setState(unloadState);
        return;
      }
      return;
    }

    // Only try to load when profile is ready (but we do not unload if it
    // becomes unready after load).
    const notLoadState = this.checkShouldNotLoad();
    if (notLoadState !== undefined) {
      this.setState(notLoadState);
      return;
    }

    // Start loading if not already loading/loaded.
    switch (this.currentStatus()) {
      case GlicWebviewLoadStatus.NOT_LOADED:
      case GlicWebviewLoadStatus.ERROR:
      case GlicWebviewLoadStatus.AT_GUEST_ERROR_PAGE:
      case GlicWebviewLoadStatus.AT_LOGIN_PAGE:
        this.setState({status: GlicWebviewLoadStatus.LOADING});
        break;
      default:
        break;
    }
  }

  getState(): ObservableValueReadOnly<GlicWebviewLoadState> {
    return this.state;
  }

  private checkShouldUnload(): GlicWebviewLoadState|undefined {
    if (!this.wantLoad) {
      return {status: GlicWebviewLoadStatus.NOT_LOADED};
    }
    switch (this.profileReadyState) {
      case undefined:
        // Ready state is never set to undefined except at the start, so don't
        // transition to an error state.
        return {status: GlicWebviewLoadStatus.NOT_LOADED};
      case ProfileReadyState.kUnknownError:
      case ProfileReadyState.kIneligible:
        return {
          status: GlicWebviewLoadStatus.ERROR,
          errorReason: GlicWebviewLoadErrorReason.PROFILE_INELIGIBLE,
        };
      case ProfileReadyState.kDisabledByAdmin:
        return {
          status: GlicWebviewLoadStatus.ERROR,
          errorReason: GlicWebviewLoadErrorReason.DISABLED_BY_ADMIN,
        };
      case ProfileReadyState.kSignInRequired:
        return {
          status: GlicWebviewLoadStatus.ERROR,
          errorReason: GlicWebviewLoadErrorReason.BLOCKED_BY_NEED_SIGN_IN,
        };
      case ProfileReadyState.kReady:
        break;
      default:
        assertNotReachedCase(this.profileReadyState);
    }
    return undefined;
  }

  // Returns an error status if loading should not be tried.
  // Returns undefined if loading can be tried.
  private checkShouldNotLoad(): GlicWebviewLoadState|undefined {
    const shouldUnloadState = this.checkShouldUnload();
    if (shouldUnloadState) {
      return shouldUnloadState;
    }
    if (!this.isOnline.getCurrentValue()) {
      return {
        status: GlicWebviewLoadStatus.ERROR,
        errorReason: GlicWebviewLoadErrorReason.NOT_ONLINE,
      };
    }
    return undefined;
  }

  private doLoad(): Task<void> {
    const taskState: {canceled: boolean} = {canceled: false};
    const promise = this.doLoadAsync(taskState);
    return {
      done: () => promise,
      cancel: () => {
        taskState.canceled = true;
      },
    };
  }

  private async doLoadAsync(taskState: {canceled: boolean}): Promise<void> {
    this.doUnload();

    const {result} = this.browserProxy.glicPreloadHandler ?
        await this.browserProxy.glicPreloadHandler.prepareForClient() :
        await this.browserProxy.pageHandler.prepareForClient();
    if (taskState.canceled) {
      return;
    }

    // If we don't actually want to load anymore, don't do it.
    const notLoadState = this.checkShouldNotLoad();
    if (notLoadState !== undefined) {
      this.setState(notLoadState);
      return;
    }

    switch (result) {
      case PrepareForClientResult.kSuccess:
        break;
      case PrepareForClientResult.kErrorResyncingCookies:
        this.setState({
          status: GlicWebviewLoadStatus.ERROR,
          errorReason: GlicWebviewLoadErrorReason.BLOCKED_BY_SYNC_ERROR,
        });
        return;
      case PrepareForClientResult.kRequiresSignIn:
        this.setState({
          status: GlicWebviewLoadStatus.ERROR,
          errorReason: GlicWebviewLoadErrorReason.BLOCKED_BY_NEED_SIGN_IN,
        });
        return;
      default:
        assertNotReachedCase(result);
    }
    this.loadTimer.start(() => {
      this.stopLoadingDueToTimeout();
    });
    this.webview = this.createWebviewController();
    this.webviewHasClient = false;
    this.webClientStateSubscription =
        this.webview.getWebClientState().subscribe(s => {
          this.webClientStateChanged(s);
        });
  }

  createWebviewController(): WebviewControllerInterface {
    return new WebviewController(
        this.container, this.browserProxy, this, this.hostEmbedder,
        this.persistentState);
  }

  private doUnload(): void {
    this.webClientStateSubscription?.unsubscribe();
    this.webview?.destroy();
    this.webview = undefined;
    this.webviewHasClient = false;
  }

  webClientStateChanged(state: WebClientState) {
    if (state === WebClientState.RESPONSIVE) {
      this.webviewHasClient = true;
    }
    if (this.currentStatus() === GlicWebviewLoadStatus.LOADING) {
      if (state === WebClientState.RESPONSIVE) {
        this.setState({status: GlicWebviewLoadStatus.RESPONSIVE});
        return;
      }
    }
    if (state === WebClientState.ERROR) {
      switch (this.currentStatus()) {
        case GlicWebviewLoadStatus.LOADING:
        case GlicWebviewLoadStatus.RESPONSIVE:
        case GlicWebviewLoadStatus.UNRESPONSIVE:
          this.setState({
            status: GlicWebviewLoadStatus.ERROR,
            errorReason: GlicWebviewLoadErrorReason.UNKNOWN,
          });
          return;
        default:
          break;
      }
    }
    if (this.currentStatus() === GlicWebviewLoadStatus.RESPONSIVE ||
        this.currentStatus() === GlicWebviewLoadStatus.UNRESPONSIVE) {
      switch (state) {
        case WebClientState.ERROR:
          break;
        case WebClientState.RESPONSIVE:
          this.setState({status: GlicWebviewLoadStatus.RESPONSIVE});
          break;
        case WebClientState.UNRESPONSIVE:
          this.setState({status: GlicWebviewLoadStatus.UNRESPONSIVE});
          break;
        case WebClientState.UNINITIALIZED:
          break;
        default:
          assertNotReachedCase(state);
      }
    }
  }

  // WebviewDelegate impl.

  webviewError(reason: string): void {
    console.warn(`webview exit. reason: ${reason}`);
    this.setState({
      status: GlicWebviewLoadStatus.ERROR,
      errorReason: GlicWebviewLoadErrorReason.UNKNOWN,
    });
  }

  webviewUnresponsive(): void {
    this.setState({status: GlicWebviewLoadStatus.UNRESPONSIVE});
  }

  webviewPageCommit(pageType: PageType): void {
    switch (pageType) {
      case 'login':
        if (!this.allowLoginPages) {
          this.setState({
            status: GlicWebviewLoadStatus.ERROR,
            errorReason: GlicWebviewLoadErrorReason.LOGIN_PAGE_REACHED,
          });
        } else {
          this.setState({status: GlicWebviewLoadStatus.AT_LOGIN_PAGE});
        }
        break;
      case 'guestError':
        this.setState({
          status: GlicWebviewLoadStatus.AT_GUEST_ERROR_PAGE,
        });
        break;
      case 'regular':
        if (this.webviewHasClient ||
            this.currentStatus() ===
                GlicWebviewLoadStatus.AT_GUEST_ERROR_PAGE ||
            this.currentStatus() === GlicWebviewLoadStatus.AT_LOGIN_PAGE) {
          // If commit happens after load, scrap the webview and try again.
          this.doUnload();
          this.setState({status: GlicWebviewLoadStatus.NOT_LOADED});
          this.updateLoadState();
        }
        break;
      case 'loadError':
        this.setState({
          status: GlicWebviewLoadStatus.ERROR,
          errorReason: GlicWebviewLoadErrorReason.PAGE_LOAD_ERROR,
        });
        break;
      default:
        assertNotReachedCase(pageType);
    }
  }

  webviewDeniedByAdmin(): void {
    this.setState({
      status: GlicWebviewLoadStatus.ERROR,
      errorReason: GlicWebviewLoadErrorReason.DISABLED_BY_ADMIN,
    });
  }
}

interface Task<R> {
  done(): Promise<R|undefined>;
  cancel(): void;
}
