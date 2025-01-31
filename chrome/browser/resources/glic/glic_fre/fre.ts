// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';

import {BrowserProxyImpl} from '../browser_proxy.js';

const browserProxy = BrowserProxyImpl.getInstance();

const webview =
    document.getElementById('fre-guest-frame') as chrome.webviewTag.WebView;
// TODO(cuianthony): For now, borrow the configuration of the glic guest URL, to
// be replaced with the correct configuration set up for the FRE.
webview.src = loadTimeData.getString('glicGuestURL');

webview.addEventListener('loadcommit', onLoadCommit);

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
    browserProxy.freHandler.acceptFre();
  } else if (urlHash === '#noThanks') {
    browserProxy.freHandler.dismissFre();
  }
}
