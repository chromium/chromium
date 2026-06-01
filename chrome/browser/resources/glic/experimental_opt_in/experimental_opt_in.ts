// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

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

async function init() {
  const webview = getRequiredElement<chrome.webviewTag.WebView>('webview');

  webview.addEventListener('sizechanged', (e: Event) => {
    const sizeEvent = e as unknown as chrome.webviewTag.SizeChangedEvent;
    window.resizeTo(sizeEvent.newWidth, sizeEvent.newHeight);
  });

  webview.addEventListener('contentload', () => {
    handler.onWebviewLoaded();
  });
  // Wait for cookie sync to complete before setting src
  const {success} = await handler.syncCookies();
  if (!success) {
    // TODO(b/513344047): Show failure UI.
    console.error('Failed to sync cookies for glic webview');
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
