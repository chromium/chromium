// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for extension runtime.
 */
import {TestImportManager} from './testing/test_import_manager.js';

type MessageSender = chrome.runtime.MessageSender;

export class ExtensionUtil {
  /**
   * Returns true if the message sender is valid and originates from within
   * the extension.
   */
  static isValidSender(sender?: MessageSender): boolean {
    return !!sender?.url?.startsWith(`chrome-extension://${chrome.runtime.id}/`);
  }
}

TestImportManager.exportForTesting(['ExtensionUtil', ExtensionUtil]);
