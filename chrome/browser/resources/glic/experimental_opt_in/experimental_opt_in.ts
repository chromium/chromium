// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

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

async function init() {
  const webview = getRequiredElement<chrome.webviewTag.WebView>('webview');
  const offlinePanel = getRequiredElement('offlinePanel');
  const closeButtonOffline = getRequiredElement('closeButtonOffline');

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
        showOfflineState();
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

  function showOfflineState() {
    if (!offlinePanel.hidden) {
      return;
    }
    offlinePanel.hidden = false;
    webview.hidden = true;
    window.resizeTo(512, 502);
  }

  webview.addEventListener('loadstart', () => {
    hasError = false;
    offlinePanel.hidden = true;
    webview.hidden = false;
    startWatchdog();
  });

  webview.addEventListener('contentload', () => {
    clearWatchdog();
    if (hasError) {
      return;
    }
    offlinePanel.hidden = true;
    webview.hidden = false;
    handler.onWebviewLoaded();
  });

  webview.addEventListener('loadabort', ((e: Event) => {
    const loadAbortEvent = e as unknown as
      chrome.webviewTag.LoadAbortEvent;
    // Log failures when the top-level
    // frame fails to load.
    if (loadAbortEvent.isTopLevel) {
      hasError = true;
      clearWatchdog();
      showOfflineState();
    }
  }) as EventListener);

  closeButtonOffline.addEventListener('click', () => {
    handler.reject();
  });

  // Immediate pre-flight check. If the browser is already offline, show the
  // connection issue UI immediately and stop.
  if (!navigator.onLine) {
    showOfflineState();
    return;
  }

  // Wait for cookie sync to complete before setting src
  const { success } = await handler.syncCookies();
  if (!success) {
    console.error('Failed to sync cookies for glic webview');
    // If sync fails, check if it's because the user went offline during the
    // process.
    if (!navigator.onLine) {
      showOfflineState();
      return;
    }
    // TODO(b/513344047): Show "Something went wrong" failure UI here in a
    // follow-up.
    return;
  }

  const url = loadTimeData.getString('glicExperimentalTriggeringOptInURL');
  webview.setAttribute('src', url);

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
