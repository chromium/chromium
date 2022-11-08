// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface ResetBrowserProxy {
  /**
   * @param sendSettings Whether the user gave consent to upload broken settings
   *     to Google for analysis.
   * @param requestOrigin The origin of the reset request.
   * @return A promise firing once resetting has completed.
   */
  performResetProfileSettings(sendSettings: boolean, requestOrigin: string):
      Promise<void>;

  /**
   * A method to be called when the reset profile dialog is hidden.
   */
  onHideResetProfileDialog(): void;

  /**
   * A method to be called when the reset profile banner is hidden.
   */
  onHideResetProfileBanner(): void;

  /**
   * A method to be called when the reset profile dialog is shown.
   */
  onShowResetProfileDialog(): void;

  /**
   * Shows the settings that are about to be reset and which will be reported
   * to Google for analysis, in a new tab.
   */
  showReportedSettings(): void;

  /**
   * Retrieves the triggered reset tool name.
   * @return A promise firing with the tool name, once it has been retrieved.
   */
  getTriggeredResetToolName(): Promise<string>;
}

export class ResetBrowserProxyImpl implements ResetBrowserProxy {
  performResetProfileSettings(sendSettings: boolean, requestOrigin: string) {
    return sendWithPromise(
        'performResetProfileSettings', sendSettings, requestOrigin);
  }

  onHideResetProfileDialog() {
    chrome.send('onHideResetProfileDialog');
  }

  onHideResetProfileBanner() {
    chrome.send('onHideResetProfileBanner');
  }

  onShowResetProfileDialog() {
    chrome.send('onShowResetProfileDialog');
  }

  showReportedSettings() {
    sendWithPromise('getReportedSettings')
        .then(function(settings: Array<{key: string, value: string}>) {
          const output = settings.map(function(entry) {
            return entry.key + ': ' + entry.value.replace(/\n/g, ', ');
          });
          const win = window.open('about:blank')!;
          const div = win.document.createElement('div');
          div.textContent = output.join('\n');
          div.style.whiteSpace = 'pre';
          win.document.body.appendChild(div);
        });
  }

  getTriggeredResetToolName(): Promise<string> {
    return sendWithPromise('getTriggeredResetToolName');
  }

  static getInstance(): ResetBrowserProxy {
    return instance || (instance = new ResetBrowserProxyImpl());
  }

  static setInstance(obj: ResetBrowserProxy) {
    instance = obj;
  }
}

let instance: ResetBrowserProxy|null = null;
