// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import { loadTimeData } from '//resources/js/load_time_data.js';
import { getRequiredElement } from '//resources/js/util.js';

import { ExperimentalOptInPageHandler } from './glic_experimental_opt_in.mojom-webui.js';

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

function init() {
  const webview = getRequiredElement<chrome.webviewTag.WebView>('webview');
  const errorPanel = getRequiredElement('errorPanel');
  const errorIcon = getRequiredElement('errorIcon');
  const errorHeadline = getRequiredElement('errorHeadline');
  const errorMessage = getRequiredElement('errorMessage');
  const closeButtonError = getRequiredElement('closeButtonError');
  const tryAgainButton = getRequiredElement('tryAgainButton');

  let hasError = false;
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

  webview.addEventListener('sizechanged', (e: Event) => {
    if (hasError || webview.hidden) {
      return;
    }
    const sizeEvent = e as unknown as chrome.webviewTag.SizeChangedEvent;
    window.resizeTo(sizeEvent.newWidth, sizeEvent.newHeight);
  });

  function showFailureState(type: FailureType) {
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
    window.resizeTo(512, 502);
  }

  webview.addEventListener('loadstart', () => {
    hasError = false;
    errorPanel.hidden = true;
    webview.hidden = false;
    startWatchdog();
  });

  webview.addEventListener('contentload', () => {
    clearWatchdog();
    if (hasError) {
      return;
    }
    errorPanel.hidden = true;
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
    webview.hidden = false;
    hasError = false;

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

  tryLoad();

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
