// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';

function init() {
  const webview = document.querySelector('webview');
  if (webview) {
    webview.addEventListener('sizechanged', (e: Event) => {
      const sizeEvent = e as unknown as chrome.webviewTag.SizeChangedEvent;
      window.resizeTo(sizeEvent.newWidth, sizeEvent.newHeight);
    });

    const url = loadTimeData.getString('glicExperimentalTriggeringOptInURL');
    webview.setAttribute('src', url);
  }
}

if (document.readyState === 'loading') {
  window.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
