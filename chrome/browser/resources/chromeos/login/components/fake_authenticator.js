// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake out of the authenticator.js
 * closure_compiler coverage.
 */

cr.define('cr.login', function() {
  class Authenticator {
    /**
     * @param {!WebView|string} webview
     */
    constructor(webview) {
    }

    /**
     * @param {string} message
     */
    sendMessageToWebview(message) {
    }

    /**
     * @param {string|symbol} eventType
     * @param {function(Object):void} listener
     * @param {!Object=} thisObject
     */
    addEventListener(eventType, listener, thisObject) {
    }
  }

  return {Authenticator: Authenticator};
});
