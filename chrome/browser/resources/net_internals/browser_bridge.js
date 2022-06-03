// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

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

  sendExpectCTQuery(domain) {
    return sendWithPromise('expectCTQuery', domain);
  }

  sendExpectCTAdd(domain, report_uri, enforce) {
    chrome.send('expectCTAdd', [domain, report_uri, enforce]);
  }

  sendExpectCTTestReport(report_uri) {
    return sendWithPromise('expectCTTestReport', report_uri);
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
}

addSingletonGetter(BrowserBridge);
