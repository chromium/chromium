// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the google assistant section
 * to interact with the browser.
 */

const requestPrefix = 'login.AssistantOptInFlowScreen.';

/**
 * Indicates the type of the opt-in flow.
 * @enum {number}
 * */
export const AssistantOptinFlowType = {
  // The whole consent flow.
  CONSENT_FLOW: 0,
  // The voice match enrollment flow.
  SPEAKER_ID_ENROLLMENT: 1,
  // The voice match retrain flow.
  SPEAKER_ID_RETRAIN: 2,
};

/** @interface */
export class BrowserProxy {
  /**
   * Send user action to the handler.
   * @param {string} screenId ID of the screen.
   * @param {!Array<string>} action The user action.
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
   * @param {!Array<AssistantOptinFlowType>} flowType
   */
  initialized(flowType) {}

  /** Send dialog close signal. */
  dialogClose() {}
}

/** @implements {BrowserProxy} */
export class BrowserProxyImpl {
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

  /** @return {!BrowserProxy} */
  static getInstance() {
    return instance || (instance = new BrowserProxyImpl());
  }
}

/** @type {?BrowserProxy} */
let instance = null;
