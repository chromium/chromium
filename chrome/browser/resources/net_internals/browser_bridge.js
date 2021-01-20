// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser.
 */
class BrowserBridge {
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
    return cr.sendWithPromise('hstsQuery', domain);
  }

  sendHSTSAdd(domain, sts_include_subdomains) {
    chrome.send('hstsAdd', [domain, sts_include_subdomains]);
  }

  sendDomainSecurityPolicyDelete(domain) {
    chrome.send('domainSecurityPolicyDelete', [domain]);
  }

  sendExpectCTQuery(domain) {
    return cr.sendWithPromise('expectCTQuery', domain);
  }

  sendExpectCTAdd(domain, report_uri, enforce) {
    chrome.send('expectCTAdd', [domain, report_uri, enforce]);
  }

  sendExpectCTTestReport(report_uri) {
    return cr.sendWithPromise('expectCTTestReport', report_uri);
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

cr.addSingletonGetter(BrowserBridge);
