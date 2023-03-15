// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyCode} from './key_code.js';

/**
 * @fileoverview Wraps automation and accessibility_private callbacks
 * in Promises.
 */
const AutomationNode = chrome.automation.AutomationNode;

export class AsyncUtil {
  /** @return {!Promise<!AutomationNode>} */
  static async getDesktop() {
    return new Promise(resolve => chrome.automation.getDesktop(resolve));
  }

  /** @return {!Promise<AutomationNode>} */
  static async getFocus() {
    return new Promise(resolve => chrome.automation.getFocus(resolve));
  }

  /**
   * @param {!KeyCode} keyCode
   * @return {!Promise<string>}
   */
  static async getLocalizedDomKeyStringForKeyCode(keyCode) {
    return new Promise(
        resolve =>
            chrome.accessibilityPrivate.getLocalizedDomKeyStringForKeyCode(
                keyCode, resolve));
  }
}
