// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the google assistant section
 * to interact with the browser.
 */

cr.define('assistant', function() {
  var requestPrefix = 'login.AssistantOptInFlowScreen.';

  /** @interface */
  class BrowserProxy {
    /**
     * Send user action to the handler.
     * @param {string} screenId ID of the screen.
     * @param {data} action The user action.
     */
    userActed(screenId, action) {}

    /**
     * Notify the screen is shown.
     * @param {string} screenId ID of the screen.
     */
    screenShown(screenId) {}

    /** Send timeout signal. */
    timeout() {}

    /** Send flow finished signal. */
    flowFinished() {}

    /**
     * Send initialized signal.
     * @param {FlowType} flowType The flow type.
     */
    initialized(flowType) {}

    /** Send dialog close signal. */
    dialogClose() {}
  }

  /** @implements {assistant.BrowserProxy} */
  class BrowserProxyImpl {
    /** @override */
    userActed(screenId, action) {
      chrome.send(requestPrefix + screenId + '.userActed', action);
    }

    /** @override */
    screenShown(screenId) {
      chrome.send(requestPrefix + screenId + '.screenShown');
    }

    /** @override */
    timeout() {
      chrome.send(requestPrefix + 'timeout');
    }

    /** @override */
    flowFinished() {
      chrome.send(requestPrefix + 'flowFinished');
    }

    /** @override */
    initialized(flowType) {
      chrome.send(requestPrefix + 'initialized', flowType);
    }

    /** @override */
    dialogClose() {
      chrome.send('dialogClose');
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(BrowserProxyImpl);

  // #cr_define_end
  return {
    BrowserProxy: BrowserProxy,
    BrowserProxyImpl: BrowserProxyImpl,
  };
});
