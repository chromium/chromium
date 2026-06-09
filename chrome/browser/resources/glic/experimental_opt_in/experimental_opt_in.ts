// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from '//resources/js/util.js';

import {ExperimentalOptInPageHandler} from './glic_experimental_opt_in.mojom-webui.js';

const handler = ExperimentalOptInPageHandler.getRemote();

function onNewWindow(e: Event) {
  const newWindowEvent = e as unknown as chrome.webviewTag.NewWindowEvent;
  newWindowEvent.preventDefault();
  handler.validateAndOpenLinkInNewTab(newWindowEvent.targetUrl);
  newWindowEvent.stopPropagation();
}

enum FailureType {
  OFFLINE,
  GENERIC_ERROR,
}

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

function init() {
  const webview = getRequiredElement<chrome.webviewTag.WebView>('webview');
  webview.setAttribute('minwidth', String(defaultWidth));
  webview.setAttribute('maxwidth', String(defaultWidth));
  const errorPanel = getRequiredElement('errorPanel');
  const errorIcon = getRequiredElement('errorIcon');
  const errorHeadline = getRequiredElement('errorHeadline');
  const errorMessage = getRequiredElement('errorMessage');
  const closeButtonError = getRequiredElement('closeButtonError');
  const tryAgainButton = getRequiredElement('tryAgainButton');
  const optInUrl = loadTimeData.getString('glicExperimentalTriggeringOptInURL');
  const optInOrigin = new URL(optInUrl).origin;

  let hasError = false;

  const skeleton = document.getElementById('skeleton-container');
  if (skeleton) {
    try {
      skeleton.setAttribute(
          'state',
          loadTimeData.getString('glicRequiredExperimentalOptInState'));
    } catch (e) {
      console.error('Failed to get opt-in state', e);
      hasError = true;
      showFailureState(FailureType.GENERIC_ERROR);
    }
  }

  let transitioned = false;
  const transitionToWebview = () => {
    if (transitioned) {
      return;
    }
    transitioned = true;

    // Clear min-height restriction once we have real content
    document.body.style.minHeight = '';

    const skeleton = document.getElementById('skeleton-container');
    if (skeleton) {
      skeleton.classList.add('fade-out');
    }
    // Force visual layout reflow so transitioning class opacity takes effect
    // correctly.
    webview.offsetHeight;
    webview.classList.add('visible');

    setTimeout(() => {
      if (skeleton) {
        skeleton.classList.add('hidden');
        skeleton.classList.remove('fade-out');
      }
    }, TRANSITION_DURATION_MS);
  };

  webview.addEventListener('contentload', transitionToWebview);
  webview.addEventListener('loadstop', transitionToWebview);

  let loadingTimeoutId: number | null = null;

  function clearWatchdog() {
    if (loadingTimeoutId !== null) {
      clearTimeout(loadingTimeoutId);
      loadingTimeoutId = null;
    }
  }

  function startWatchdog() {
    clearWatchdog();
    loadingTimeoutId = setTimeout(() => {
      if (!hasError && webview.hidden === false) {
        hasError = true;
        webview.stop();
        // A timeout may be caused by general slowness or server issues, not
        // just the device being offline, but we show the same generic offline
        // error UI here.
        showFailureState(FailureType.OFFLINE);
      }
    }, 10000);
  }

  function showFailureState(type: FailureType) {
    document.body.style.minHeight = '';
    if (type === FailureType.OFFLINE) {
      errorIcon.setAttribute('icon', 'glic:offline');
      errorHeadline.textContent = loadTimeData.getString('offlineNoticeHeader');
      errorMessage.textContent =
        loadTimeData.getString('experimentalOptInOfflineNoticeMessage');
    } else {
      errorIcon.setAttribute('icon', 'glic:error');
      errorHeadline.textContent = loadTimeData.getString('errorNoticeHeader');
      errorMessage.textContent =
        loadTimeData.getString('experimentalOptInErrorNoticeMessage');
    }

    errorPanel.hidden = false;
    webview.hidden = true;
    const skeleton = document.getElementById('skeleton-container');
    if (skeleton) {
      skeleton.classList.add('hidden');
    }
  }

  webview.addEventListener('loadstart', () => {
    hasError = false;
    errorPanel.hidden = true;
    webview.hidden = false;
    webview.classList.remove('autosized');
    startWatchdog();
  });

  webview.request.onBeforeRequest.addListener(
      (details: {url: string, frameId: number}) => {
        if (details.frameId !== 0) {
          return {};
        }
        const url = URL.parse(details.url);
        if (!url) {
          console.error('Failed to parse URL in onBeforeRequest:', details.url);
          return {cancel: true};
        }
        if (url.protocol === 'http:' || url.protocol === 'https:') {
          if (url.origin !== optInOrigin) {
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
  webview.addEventListener('contentload', () => {
    clearWatchdog();
    if (hasError) {
      return;
    }
    errorPanel.hidden = true;
    webview.classList.add('autosized');
    webview.hidden = false;
    handler.onWebviewLoaded();
  });

  webview.addEventListener(
    'loadabort', ((e: Event) => {
      const loadAbortEvent =
        e as unknown as chrome.webviewTag.LoadAbortEvent;
      // Log failures when the top-level
      // frame fails to load.
      if (loadAbortEvent.isTopLevel) {
        hasError = true;
        clearWatchdog();
        showFailureState(FailureType.OFFLINE);
      }
    }) as EventListener);

  closeButtonError.addEventListener('click', () => {
    handler.reject();
  });

  tryAgainButton.addEventListener('click', () => {
    tryLoad();
  });

  async function tryLoad() {
    errorPanel.hidden = true;
    webview.hidden = true;
    hasError = false;
    const skeleton = document.getElementById('skeleton-container');
    if (skeleton) {
      skeleton.classList.remove('hidden', 'fade-out');
    }

    // Immediate pre-flight check. If the browser is already offline, show the
    // connection issue UI immediately and stop.
    if (!navigator.onLine) {
      showFailureState(FailureType.OFFLINE);
      return;
    }

    // Wait for cookie sync to complete before setting src
    const { success } = await handler.syncCookies();
    if (!success) {
      console.error('Failed to sync cookies for glic webview');
      // If sync fails, check if it's because the user went offline during the
      // process.
      if (!navigator.onLine) {
        showFailureState(FailureType.OFFLINE);
        return;
      }
      showFailureState(FailureType.GENERIC_ERROR);
      return;
    }

    const url = loadTimeData.getString('glicExperimentalTriggeringOptInURL');
    if (webview.getAttribute('src') === url) {
      // If the URL is already set, setting it again does nothing. Force a reload.
      webview.reload();
    } else {
      webview.setAttribute('src', url);
    }
  }
  if (!hasError) {
    tryLoad();
  }

  webview.addEventListener(
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

  webview.addEventListener('newwindow', onNewWindow as EventListener);
}

if (document.readyState === 'loading') {
  window.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
