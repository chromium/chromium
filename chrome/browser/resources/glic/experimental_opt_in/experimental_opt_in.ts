// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from '//resources/js/util.js';

import {ExperimentalOptInPageHandler} from './glic_experimental_opt_in.mojom-webui.js';

const handler = ExperimentalOptInPageHandler.getRemote();

// LINT.IfChange(GlicExperimentalTriggeringErrorType)
enum FailureType {
  GENERIC_ERROR = 0,
  OFFLINE = 1,
  COOKIE_SYNC_FAILED = 2,
  MAX_VALUE = COOKIE_SYNC_FAILED,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicExperimentalTriggeringErrorType)

const TRANSITION_DURATION_MS = 250;
// Setup target height and width custom properties immediately at load to
// prevent layout shifts.
const defaultHeight =
    loadTimeData.getInteger('glicExperimentalOptInDefaultHeight');
const defaultWidth =
    loadTimeData.getInteger('glicExperimentalOptInDefaultWidth');
document.documentElement.style.setProperty(
    '--glic-experimental-opt-in-height', `${defaultHeight}px`);
document.documentElement.style.setProperty(
    '--glic-experimental-opt-in-width', `${defaultWidth}px`);
document.documentElement.style.setProperty(
    '--glic-transition-duration', `${TRANSITION_DURATION_MS}ms`);

// Set min-height on body to prevent collapse during loading when webview is
// hidden and skeleton is absolute.
document.body.style.minHeight = `${defaultHeight}px`;

export class ExperimentalOptInApp {
  private webview_: chrome.webviewTag.WebView;
  private errorPanel_: HTMLElement;
  private errorIcon_: HTMLElement;
  private errorHeadline_: HTMLElement;
  private errorMessage_: HTMLElement;
  private closeButtonError_: HTMLElement;
  private tryAgainButton_: HTMLElement;
  private optInUrl_: string;
  private optInOrigin_: string;

  private hasError_: boolean = false;
  private transitioned_: boolean = false;
  private loadingTimeoutId_: number|null = null;

  constructor() {
    this.webview_ = getRequiredElement<chrome.webviewTag.WebView>('webview');
    // Allow a small margin of error (±2px) around the target width to prevent
    // subpixel rounding or zoom differences from failing the webview's internal
    // size-changed checks and collapsing the layout.
    this.webview_.setAttribute('minwidth', String(defaultWidth - 2));
    this.webview_.setAttribute('maxwidth', String(defaultWidth + 2));

    this.errorPanel_ = getRequiredElement('errorPanel');
    this.errorIcon_ = getRequiredElement('errorIcon');
    this.errorHeadline_ = getRequiredElement('errorHeadline');
    this.errorMessage_ = getRequiredElement('errorMessage');
    this.closeButtonError_ = getRequiredElement('closeButtonError');
    this.tryAgainButton_ = getRequiredElement('tryAgainButton');

    this.optInUrl_ =
        loadTimeData.getString('glicExperimentalTriggeringOptInURL');
    this.optInOrigin_ = new URL(this.optInUrl_).origin;

    const skeleton = document.getElementById('skeleton-container');
    if (skeleton) {
      try {
        skeleton.setAttribute(
            'state',
            loadTimeData.getString('glicRequiredExperimentalOptInState'));
      } catch (e) {
        console.error('Failed to get opt-in state', e);
        this.hasError_ = true;
        this.showFailureState_(FailureType.GENERIC_ERROR);
      }
    }

    this.setupEventListeners_();

    if (!this.hasError_) {
      this.tryLoad_();
    }
  }

  private setupEventListeners_() {
    this.webview_.addEventListener(
        'contentload', () => this.transitionToWebview_());
    this.webview_.addEventListener(
        'loadstop', () => this.transitionToWebview_());

    this.webview_.addEventListener('loadstart', () => {
      this.hasError_ = false;
      this.errorPanel_.hidden = true;
      this.webview_.hidden = false;
      this.webview_.classList.remove('autosized');
      this.startWatchdog_();
    });

    this.webview_.request.onBeforeRequest.addListener(
        (details: {url: string, frameId: number}) => {
          if (details.frameId !== 0) {
            return {};
          }
          const url = URL.parse(details.url);
          if (!url) {
            console.error(
                'Failed to parse URL in onBeforeRequest:', details.url);
            return {cancel: true};
          }
          if (loadTimeData.getBoolean('glicDevEnabled')) {
            return {};
          }
          if (url.protocol === 'http:' || url.protocol === 'https:') {
            if (url.origin !== this.optInOrigin_) {
              return {cancel: true};
            }
          }
          return {};
        },
        {
          urls: ['<all_urls>'],
          types: ['main_frame'],
        },
        ['blocking']);

    this.webview_.addEventListener('contentload', () => {
      this.clearWatchdog_();
      if (this.hasError_) {
        return;
      }
      this.errorPanel_.hidden = true;
      this.webview_.classList.add('autosized');
      this.webview_.hidden = false;
      handler.onWebviewLoaded();
    });

    this.webview_.addEventListener(
        'loadabort', ((e: Event) => {
                       if (loadTimeData.getBoolean('glicDevEnabled')) {
                         return;
                       }
                       const loadAbortEvent =
                           e as unknown as chrome.webviewTag.LoadAbortEvent;
                       // Log failures when the top-level
                       // frame fails to load.
                       if (loadAbortEvent.isTopLevel) {
                         this.hasError_ = true;
                         this.clearWatchdog_();
                         this.showFailureState_(FailureType.OFFLINE);
                       }
                     }) as EventListener);

    this.closeButtonError_.addEventListener('click', () => {
      handler.reject();
    });

    this.tryAgainButton_.addEventListener('click', () => {
      this.tryLoad_();
    });

    this.webview_.addEventListener(
        'loadcommit', ((e: Event) => {
                        const loadCommitEvent =
                            e as unknown as chrome.webviewTag.LoadCommitEvent;
                        if (!loadCommitEvent.isTopLevel) {
                          return;
                        }
                        const urlObj = new URL(loadCommitEvent.url);
                        const urlHash = urlObj.hash;

                        if (urlHash === '#continue') {
                          handler.accept();
                        } else if (urlHash.startsWith('#noThanks')) {
                          handler.reject();
                        }
                      }) as EventListener);

    this.webview_.addEventListener(
        'newwindow', (e: Event) => this.onNewWindow_(e));
  }

  private onNewWindow_(e: Event) {
    const newWindowEvent = e as unknown as chrome.webviewTag.NewWindowEvent;
    newWindowEvent.preventDefault();
    handler.validateAndOpenLinkInNewTab(newWindowEvent.targetUrl);
    newWindowEvent.stopPropagation();
  }

  private transitionToWebview_() {
    if (this.transitioned_) {
      return;
    }
    this.transitioned_ = true;

    // Clear min-height restriction once we have real content
    document.body.style.minHeight = '';

    const skeleton = document.getElementById('skeleton-container');
    if (skeleton) {
      skeleton.classList.add('fade-out');
    }
    // Force visual layout reflow so transitioning class opacity takes effect
    // correctly.
    this.webview_.offsetHeight;
    this.webview_.classList.add('visible');

    setTimeout(() => {
      if (skeleton) {
        skeleton.classList.add('hidden');
        skeleton.classList.remove('fade-out');
      }
    }, TRANSITION_DURATION_MS);
  }

  private clearWatchdog_() {
    if (this.loadingTimeoutId_ !== null) {
      clearTimeout(this.loadingTimeoutId_);
      this.loadingTimeoutId_ = null;
    }
  }

  private startWatchdog_() {
    if (loadTimeData.getBoolean('glicDevEnabled')) {
      return;
    }
    this.clearWatchdog_();
    this.loadingTimeoutId_ = setTimeout(() => {
      if (!this.hasError_ && this.webview_.hidden === false) {
        this.hasError_ = true;
        this.webview_.stop();
        // A timeout may be caused by general slowness or server issues, not
        // just the device being offline, but we show the same generic offline
        // error UI here.
        this.showFailureState_(FailureType.OFFLINE);
      }
    }, 10000);
  }

  private showFailureState_(type: FailureType) {
    document.body.style.minHeight = '';
    chrome.histograms.recordEnumerationValue(
        'Glic.ExperimentalTriggering.OptIn.ErrorShown', type,
        FailureType.MAX_VALUE + 1);
    if (type === FailureType.OFFLINE) {
      this.errorIcon_.setAttribute('icon', 'glic:offline');
      this.errorHeadline_.textContent =
          loadTimeData.getString('offlineNoticeHeader');
      this.errorMessage_.textContent =
          loadTimeData.getString('experimentalOptInOfflineNoticeMessage');
    } else {
      this.errorIcon_.setAttribute('icon', 'glic:error');
      this.errorHeadline_.textContent =
          loadTimeData.getString('errorNoticeHeader');
      this.errorMessage_.textContent =
          loadTimeData.getString('experimentalOptInErrorNoticeMessage');
    }

    this.errorPanel_.hidden = false;
    this.webview_.hidden = true;
    const skeleton = document.getElementById('skeleton-container');
    if (skeleton) {
      skeleton.classList.add('hidden');
    }
  }

  private async tryLoad_() {
    this.errorPanel_.hidden = true;
    this.webview_.hidden = true;
    this.hasError_ = false;
    const skeleton = document.getElementById('skeleton-container');
    if (skeleton) {
      skeleton.classList.remove('hidden', 'fade-out');
    }

    // Immediate pre-flight check. If the browser is already offline, show the
    // connection issue UI immediately and stop.
    if (!navigator.onLine) {
      this.showFailureState_(FailureType.OFFLINE);
      return;
    }

    // Wait for cookie sync to complete before setting src
    const { success } = await handler.syncCookies();
    if (!success) {
      console.error('Failed to sync cookies for glic webview');
      // If sync fails, check if it's because the user went offline during the
      // process.
      if (!navigator.onLine) {
        this.showFailureState_(FailureType.OFFLINE);
        return;
      }
      this.showFailureState_(FailureType.COOKIE_SYNC_FAILED);
      return;
    }

    if (this.webview_.getAttribute('src') === this.optInUrl_) {
      // If the URL is already set, setting it again does nothing. Force a reload.
      this.webview_.reload();
    } else {
      this.webview_.setAttribute('src', this.optInUrl_);
    }
  }
}

function init() {
  ColorChangeUpdater.forDocument().start();
  new ExperimentalOptInApp();
}

if (document.readyState === 'loading') {
  window.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
