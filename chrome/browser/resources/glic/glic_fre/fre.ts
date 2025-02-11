// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';

import {FrePageHandlerRemote, PageHandlerFactory} from '../glic.mojom-webui.js';

const freHandler = new FrePageHandlerRemote();
PageHandlerFactory.getRemote().createFrePageHandler(
    (freHandler as FrePageHandlerRemote).$.bindNewPipeAndPassReceiver());

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

function getWebviewSrc() {
  // If a valid hotkey configuration is used, append the string as a query
  // parameter to the given FRE URL.
  const glicHotkeyString = loadTimeData.getString('glicHotkeyString');
  const hotkeyQueryParamString =
      glicHotkeyString ? '&hotkey=' + glicHotkeyString : '';
  // TODO(cuianthony): For now, borrow the configuration of the glic guest URL,
  // to be replaced with the correct configuration set up for the FRE.
  return loadTimeData.getString('glicFreURL') + hotkeyQueryParamString;
}

webview.src = getWebviewSrc();
