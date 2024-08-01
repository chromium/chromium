// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyCode} from './key_code.js';
import {TestImportManager} from './testing/test_import_manager.js';

/**
 * @fileoverview Wraps automation and accessibility_private callbacks
 * in Promises.
 */

type AutomationNode = chrome.automation.AutomationNode;

export class AsyncUtil {
  static async getDesktop(): Promise<AutomationNode> {
    return new Promise(resolve => chrome.automation.getDesktop(resolve));
  }

  static async getFocus(): Promise<AutomationNode> {
    return new Promise(resolve => chrome.automation.getFocus(resolve));
  }

  static async getLocalizedDomKeyStringForKeyCode(
      keyCode: KeyCode): Promise<string> {
    return new Promise(
        resolve =>
            chrome.accessibilityPrivate.getLocalizedDomKeyStringForKeyCode(
                keyCode, resolve));
  }
}

TestImportManager.exportForTesting(AsyncUtil);
