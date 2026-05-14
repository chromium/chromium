// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from '//resources/js/util.js';

import {ExperimentalOptInPageHandler} from './glic_experimental_opt_in.mojom-webui.js';

const handler = ExperimentalOptInPageHandler.getRemote();

function init() {
  const webview = getRequiredElement<chrome.webviewTag.WebView>('webview');

  webview.addEventListener('sizechanged', (e: Event) => {
    const sizeEvent = e as unknown as chrome.webviewTag.SizeChangedEvent;
    window.resizeTo(sizeEvent.newWidth, sizeEvent.newHeight);
  });

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
}

if (document.readyState === 'loading') {
  window.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
