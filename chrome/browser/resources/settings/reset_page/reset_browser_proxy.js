// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
export class ResetBrowserProxy {
  /**
   * @param {boolean} sendSettings Whether the user gave consent to upload
   *     broken settings to Google for analysis.
   * @param {string} requestOrigin The origin of the reset request.
   * @return {!Promise} A promise firing once resetting has completed.
   */
  performResetProfileSettings(sendSettings, requestOrigin) {}

  /**
   * A method to be called when the reset profile dialog is hidden.
   */
  onHideResetProfileDialog() {}

  /**
   * A method to be called when the reset profile banner is hidden.
   */
  onHideResetProfileBanner() {}

  /**
   * A method to be called when the reset profile dialog is shown.
   */
  onShowResetProfileDialog() {}

  /**
   * Shows the settings that are about to be reset and which will be reported
   * to Google for analysis, in a new tab.
   */
  showReportedSettings() {}

  /**
   * Retrieves the triggered reset tool name.
   * @return {!Promise<string>} A promise firing with the tool name, once it
   *     has been retrieved.
   */
  getTriggeredResetToolName() {}
}

/**
 * @implements {ResetBrowserProxy}
 */
export class ResetBrowserProxyImpl {
  /** @override */
  performResetProfileSettings(sendSettings, requestOrigin) {
    return sendWithPromise(
        'performResetProfileSettings', sendSettings, requestOrigin);
  }

  /** @override */
  onHideResetProfileDialog() {
    chrome.send('onHideResetProfileDialog');
  }

  /** @override */
  onHideResetProfileBanner() {
    chrome.send('onHideResetProfileBanner');
  }

  /** @override */
  onShowResetProfileDialog() {
    chrome.send('onShowResetProfileDialog');
  }

  /** @override */
  showReportedSettings() {
    sendWithPromise('getReportedSettings').then(function(settings) {
      const output = settings.map(function(entry) {
        return entry.key + ': ' + entry.value.replace(/\n/g, ', ');
      });
      const win = window.open('about:blank');
      const div = win.document.createElement('div');
      div.textContent = output.join('\n');
      div.style.whiteSpace = 'pre';
      win.document.body.appendChild(div);
    });
  }

  /** @override */
  getTriggeredResetToolName() {
    return sendWithPromise('getTriggeredResetToolName');
  }
}

addSingletonGetter(ResetBrowserProxyImpl);
