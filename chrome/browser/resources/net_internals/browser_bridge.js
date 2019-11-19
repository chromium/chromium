// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser.
 */
const BrowserBridge = (function() {
  'use strict';

  /**
   * @constructor
   */
  function BrowserBridge() {
    assertFirstConstructorCall(BrowserBridge);

    // List of observers for various bits of browser state.
    this.hstsObservers_ = [];
    this.expectCTObservers_ = [];
    this.crosONCFileParseObservers_ = [];
    this.storeDebugLogsObservers_ = [];
    this.setNetworkDebugModeObservers_ = [];
  }

  cr.addSingletonGetter(BrowserBridge);

  BrowserBridge.prototype = {

    //--------------------------------------------------------------------------
    // Messages sent to the browser
    //--------------------------------------------------------------------------

    /**
     * Wraps |chrome.send|.
     * TODO(mattm): remove this and switch things to use chrome.send directly.
     */
    send: function(value1, value2) {
      if (arguments.length == 1) {
        chrome.send(value1);
      } else if (arguments.length == 2) {
        chrome.send(value1, value2);
      } else {
        throw 'Unsupported number of arguments.';
      }
    },

    sendReloadProxySettings: function() {
      this.send('reloadProxySettings');
    },

    sendClearBadProxies: function() {
      this.send('clearBadProxies');
    },

    sendClearHostResolverCache: function() {
      this.send('clearHostResolverCache');
    },

    sendHSTSQuery: function(domain) {
      this.send('hstsQuery', [domain]);
    },

    sendHSTSAdd: function(domain, sts_include_subdomains) {
      this.send('hstsAdd', [domain, sts_include_subdomains]);
    },

    sendDomainSecurityPolicyDelete: function(domain) {
      this.send('domainSecurityPolicyDelete', [domain]);
    },

    sendExpectCTQuery: function(domain) {
      this.send('expectCTQuery', [domain]);
    },

    sendExpectCTAdd: function(domain, report_uri, enforce) {
      this.send('expectCTAdd', [domain, report_uri, enforce]);
    },

    sendExpectCTTestReport: function(report_uri) {
      this.send('expectCTTestReport', [report_uri]);
    },

    sendCloseIdleSockets: function() {
      this.send('closeIdleSockets');
    },

    sendFlushSocketPools: function() {
      this.send('flushSocketPools');
    },

    importONCFile: function(fileContent, passcode) {
      this.send('importONCFile', [fileContent, passcode]);
    },

    storeDebugLogs: function() {
      this.send('storeDebugLogs');
    },

    storeCombinedDebugLogs: function() {
      this.send('storeCombinedDebugLogs');
    },

    setNetworkDebugMode: function(subsystem) {
      this.send('setNetworkDebugMode', [subsystem]);
    },

    //--------------------------------------------------------------------------
    // Messages received from the browser.
    //--------------------------------------------------------------------------

    receive: function(command, params) {
      this[command](params);
    },

    receivedHSTSResult: function(info) {
      for (let i = 0; i < this.hstsObservers_.length; i++) {
        this.hstsObservers_[i].onHSTSQueryResult(info);
      }
    },

    receivedExpectCTResult: function(info) {
      for (let i = 0; i < this.expectCTObservers_.length; i++) {
        this.expectCTObservers_[i].onExpectCTQueryResult(info);
      }
    },

    receivedExpectCTTestReportResult: function(result) {
      for (let i = 0; i < this.expectCTObservers_.length; i++) {
        this.expectCTObservers_[i].onExpectCTTestReportResult(result);
      }
    },

    receivedONCFileParse: function(error) {
      for (let i = 0; i < this.crosONCFileParseObservers_.length; i++) {
        this.crosONCFileParseObservers_[i].onONCFileParse(error);
      }
    },

    receivedStoreDebugLogs: function(status) {
      for (let i = 0; i < this.storeDebugLogsObservers_.length; i++) {
        this.storeDebugLogsObservers_[i].onStoreDebugLogs(status);
      }
    },

    receivedStoreCombinedDebugLogs: function(status) {
      for (let i = 0; i < this.storeDebugLogsObservers_.length; i++) {
        this.storeDebugLogsObservers_[i].onStoreCombinedDebugLogs(status);
      }
    },

    receivedSetNetworkDebugMode: function(status) {
      for (let i = 0; i < this.setNetworkDebugModeObservers_.length; i++) {
        this.setNetworkDebugModeObservers_[i].onSetNetworkDebugMode(status);
      }
    },

    //--------------------------------------------------------------------------

    /**
     * Adds a listener for the results of HSTS (HTTPS Strict Transport Security)
     * queries. The observer will be called back with:
     *
     *   observer.onHSTSQueryResult(result);
     */
    addHSTSObserver: function(observer) {
      this.hstsObservers_.push(observer);
    },

    /**
     * Adds a listener for the results of Expect-CT queries. The observer will
     * be called back with:
     *
     *   observer.onExpectCTQueryResult(result);
     */
    addExpectCTObserver: function(observer) {
      this.expectCTObservers_.push(observer);
    },

    /**
     * Adds a listener for ONC file parse status. The observer will be called
     * back with:
     *
     *   observer.onONCFileParse(error);
     */
    addCrosONCFileParseObserver: function(observer) {
      this.crosONCFileParseObservers_.push(observer);
    },

    /**
     * Adds a listener for storing log file status. The observer will be called
     * back with:
     *
     *   observer.onStoreDebugLogs(status);
     *   observer.onStoreCombinedDebugLogs(status);
     */
    addStoreDebugLogsObserver: function(observer) {
      this.storeDebugLogsObservers_.push(observer);
    },

    /**
     * Adds a listener for network debugging mode status. The observer
     * will be called back with:
     *
     *   observer.onSetNetworkDebugMode(status);
     */
    addSetNetworkDebugModeObserver: function(observer) {
      this.setNetworkDebugModeObservers_.push(observer);
    },
  };

  return BrowserBridge;
})();
