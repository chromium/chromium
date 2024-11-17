// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '/strings.m.js';

import {isRTL} from '//resources/js/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthResult} from '../mojom/graduation_ui.mojom-webui.js';

import {ScreenSwitchedEvent, ScreenSwitchEvents} from './graduation_app.js';
import {getTemplate} from './graduation_takeout_ui.html.js';
import {getGraduationUiHandler} from './graduation_ui_handler.js';

declare global {
  interface HTMLElementEventMap {
    'newwindow': chrome.webviewTag.NewWindowEvent;
  }
}

enum AuthStatus {
  IN_PROGRESS = 0,
  SUCCESS = 1,
  ERROR = 2,
}

/**
 * The base URL of the banner shown in Takeout indicating that the user has
 * completed the final step of the flow.
 * May be suffixed by a year, for example: "-2024.png".
 */
const TAKEOUT_COMPLETED_BANNER_BASE_URL: string =
    'https://www.gstatic.com/ac/takeout/migration/migration-banner';

/**
 * There are some cases wherein the Takeout tool fires a loadabort event
 * that is benign and does not indicate a fatal error (i.e. when double-clicking
 * the `Start Transfer` button).
 *
 * Therefore, on loadabort, the webview is reloaded until the limit on
 * consecutive failed reload attempts is reached. If that limit is reached, the
 * terminal error screen is triggered.
 *
 * Reloads are attempted with an exponential backoff that starts at 500ms and
 * plateaus at 2000ms to limit perceived latency. The first reload occurs after
 * the first backoff.
 * Backoff pattern: 500ms, 1000ms, 2000ms, 2000ms, ...
 */
export class WebviewReloadHelper {
  static readonly MAX_RELOAD_ATTEMPTS: number = 3;
  private static readonly INITIAL_RELOAD_DELAY_IN_MS: number = 500;
  private static readonly MAXIMUM_RELOAD_DELAY_IN_MS: number = 2000;
  private static readonly BACKOFF_FACTOR: number = 2;

  private reloadCount: number = 0;
  private reloadDelay: number = WebviewReloadHelper.INITIAL_RELOAD_DELAY_IN_MS;
  private reloadTimer: number = 0;

  reset(): void {
    this.reloadCount = 0;
    this.reloadDelay = WebviewReloadHelper.INITIAL_RELOAD_DELAY_IN_MS;
    // Cancels any future reload.
    window.clearTimeout(this.reloadTimer);
  }

  isReloadCountLimitReached(): boolean {
    return this.reloadCount === WebviewReloadHelper.MAX_RELOAD_ATTEMPTS;
  }

  private updateReloadDelay(): void {
    const multipliedReloadDelay =
        this.reloadDelay * WebviewReloadHelper.BACKOFF_FACTOR;
    this.reloadDelay = Math.min(
        multipliedReloadDelay, WebviewReloadHelper.MAXIMUM_RELOAD_DELAY_IN_MS);
  }

  scheduleReload(webview: chrome.webviewTag.WebView): void {
    if (this.isReloadCountLimitReached()) {
      return;
    }
    this.reloadCount++;
    window.clearTimeout(this.reloadTimer);
    this.reloadTimer =
        window.setTimeout(webview.reload.bind(this), this.reloadDelay);
    this.updateReloadDelay();
  }
}

export class GraduationTakeoutUi extends PolymerElement {
  static get is() {
    return 'graduation-takeout-ui' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showLoadingScreen: {
        type: Boolean,
        computed: 'computeShowLoadingScreen(authStatus, isWebviewLoading)',
      },

      /**
       * Whether the webview content has indicated that the user has completed
       * the Takeout flow.
       */
      takeoutFlowCompleted: {
        type: Boolean,
        value: false,
      },
    };
  }

  authStatus: AuthStatus = AuthStatus.IN_PROGRESS;
  isWebviewLoading: boolean = false;
  takeoutFlowCompleted: boolean;
  private webview: chrome.webviewTag.WebView;
  private webviewReloadHelper: WebviewReloadHelper;
  private startTransferUrl: string;

  override ready() {
    super.ready();

    this.webview =
        this.shadowRoot!.querySelector<chrome.webviewTag.WebView>('webview')!;

    this.configureWebviewListeners();

    this.addEventListener(ScreenSwitchedEvent, () => {
      this.shadowRoot!.querySelector<HTMLElement>('#backButton')!.focus();
    });

    this.webviewReloadHelper = new WebviewReloadHelper();

    this.startTransferUrl =
        loadTimeData.getString('startTransferUrl').toString();
  }

  onAuthComplete(result: AuthResult): void {
    switch (result) {
      case (AuthResult.kSuccess):
        this.setIsWebviewLoading(true);
        this.authStatus = AuthStatus.SUCCESS;
        this.loadStartTransferPage();
        break;
      case (AuthResult.kError):
        this.authStatus = AuthStatus.ERROR;
        this.triggerErrorScreen();
    }
  }

  private computeShowLoadingScreen(
      authStatus: AuthStatus, isWebviewLoading: boolean) {
    return authStatus === AuthStatus.IN_PROGRESS || isWebviewLoading === true;
  }

  private configureWebviewListeners(): void {
    this.webview.addEventListener('contentload', () => {
      this.webviewReloadHelper.reset();
      this.setIsWebviewLoading(false);
    });

    this.webview.addEventListener('loadabort', () => {
      this.onLoadAbort();
    });

    this.webview.addEventListener(
        'newwindow', (e: chrome.webviewTag.NewWindowEvent) => {
          // Allow the webview to open links in a new tab.
          window.open(e.targetUrl);
        });

    // The done button is made visible when the image shown at the end of the
    // Takeout flow is displayed to the user.
    this.webview.request.onCompleted.addListener((details: any) => {
      if (details.statusCode === 200 &&
          details.url.startsWith(TAKEOUT_COMPLETED_BANNER_BASE_URL)) {
        getGraduationUiHandler().onTransferComplete();
        this.takeoutFlowCompleted = true;
      }
    }, {urls: ['<all_urls>']});
  }

  private onLoadAbort(): void {
    if (this.authStatus !== AuthStatus.SUCCESS) {
      return;
    }

    // Don't attempt reload if the app is offline. This can cause a reload
    // failure that causes the loading spinner to never go away, even when the
    // app comes back online.
    if (!navigator.onLine) {
      return;
    }

    if (this.webviewReloadHelper.isReloadCountLimitReached()) {
      this.webviewReloadHelper.reset();
      this.setIsWebviewLoading(false);
      this.triggerErrorScreen();
      return;
    }
    this.setIsWebviewLoading(true);
    this.webviewReloadHelper.scheduleReload(this.webview);
  }

  private setIsWebviewLoading(isWebviewLoading: boolean): void {
    this.isWebviewLoading = isWebviewLoading;
  }

  private triggerErrorScreen(): void {
    this.dispatchEvent(new CustomEvent(ScreenSwitchEvents.SHOW_ERROR, {
      bubbles: true,
      composed: true,
    }));
  }

  private triggerWelcomeScreen(): void {
    this.dispatchEvent(new CustomEvent(ScreenSwitchEvents.SHOW_WELCOME, {
      bubbles: true,
      composed: true,
    }));
  }

  // Expects the loading screen to be shown by the caller.
  private loadStartTransferPage(): void {
    // If the user has navigated away from the Transfer page in the webview
    // before, navigate the webview back to the Transfer page.
    if (this.webview.src !== this.startTransferUrl) {
      // Re-setting the source reloads the webview.
      this.webview.src = this.startTransferUrl;
      return;
    }
    this.webview.reload();
  }

  private getBackButtonIcon(): string {
    return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
  }

  private onBackClicked(): void {
    this.triggerWelcomeScreen();

    if (this.authStatus === AuthStatus.SUCCESS) {
      this.webview.stop();
      this.webviewReloadHelper.reset();
      this.setIsWebviewLoading(true);
      this.loadStartTransferPage();
    }
  }

  private onDoneClicked(): void {
    window.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GraduationTakeoutUi.is]: GraduationTakeoutUi;
  }
}

customElements.define(GraduationTakeoutUi.is, GraduationTakeoutUi);
