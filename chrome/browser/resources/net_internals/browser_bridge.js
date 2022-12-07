// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/** @type {?BrowserBridge} */
let instance = null;

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser.
 */
export class BrowserBridge {
  constructor() {}

  //--------------------------------------------------------------------------
  // Messages sent to the browser
  //--------------------------------------------------------------------------
  sendReloadProxySettings() {
    chrome.send('reloadProxySettings');
  }

  sendClearBadProxies() {
    chrome.send('clearBadProxies');
  }

  sendResolveHost(hostname) {
    return sendWithPromise('resolveHost', hostname);
  }

  sendClearHostResolverCache() {
    chrome.send('clearHostResolverCache');
  }

  sendHSTSQuery(domain) {
    return sendWithPromise('hstsQuery', domain);
  }

  sendHSTSAdd(domain, sts_include_subdomains) {
    chrome.send('hstsAdd', [domain, sts_include_subdomains]);
  }

  sendDomainSecurityPolicyDelete(domain) {
    chrome.send('domainSecurityPolicyDelete', [domain]);
  }

  sendCloseIdleSockets() {
    chrome.send('closeIdleSockets');
  }

  sendFlushSocketPools() {
    chrome.send('flushSocketPools');
  }

  setNetworkDebugMode(subsystem) {
    chrome.send('setNetworkDebugMode', [subsystem]);
  }

  static getInstance() {
    return instance || (instance = new BrowserBridge());
  }
}
