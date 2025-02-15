// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';

import {FrePageHandlerRemote, PageHandlerFactory} from '../glic.mojom-webui.js';

const freHandler = new FrePageHandlerRemote();
PageHandlerFactory.getRemote().createFrePageHandler(
    (freHandler).$.bindNewPipeAndPassReceiver());

const webview =
    document.getElementById('fre-guest-frame') as chrome.webviewTag.WebView;

webview.addEventListener('loadcommit', onLoadCommit);
webview.addEventListener('newwindow', onNewWindow);

function onLoadCommit(e: any) {
  if (!e.isTopLevel) {
    return;
  }
  const url = new URL(e.url);
  const urlHash = url.hash;

  // Fragment navigations are used to represent actions taken in the web client
  // following this mapping: “Continue” button navigates to
  // glic/intro...#continue, “No thanks” button navigates to
  // glic/intro...#noThanks
  if (urlHash === '#continue') {
    freHandler.acceptFre();
  } else if (urlHash === '#noThanks') {
    freHandler.dismissFre();
  }
}

function onNewWindow(e: any) {
  e.preventDefault();
  freHandler.validateAndOpenLinkInNewTab({
    url: e.targetUrl,
  });
  e.stopPropagation();
}

webview.src = loadTimeData.getString('glicFreURL');
