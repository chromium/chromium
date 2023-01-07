// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Watches the currently focused URL to verify if logging should
 * occur.
 */
import {ChromeVoxState, ChromeVoxStateObserver} from '../chromevox_state.js';

import {LogStore} from './log_store.js';

/** @implements {ChromeVoxStateObserver} */
export class LogUrlWatcher {
  static create() {
    if (LogUrlWatcher.instance) {
      return;
    }
    LogUrlWatcher.instance = new LogUrlWatcher();
    ChromeVoxState.addObserver(LogUrlWatcher.instance);
    // Initialize using the current range.
    if (ChromeVoxState.instance) {
      LogUrlWatcher.instance.onCurrentRangeChanged(
          ChromeVoxState.instance.currentRange);
    }
  }

  static destroy() {
    if (!LogUrlWatcher.instance) {
      return;
    }
    ChromeVoxState.removeObserver(LogUrlWatcher.instance);
    LogUrlWatcher.instance = null;
  }

  /** @override */
  onCurrentRangeChanged(range, opt_fromEditing) {
    if (range && range.start && range.start.node && range.start.node.root) {
      LogStore.shouldSkipOutput =
          range.start.node.root.docUrl.indexOf(
              chrome.extension.getURL('chromevox/log_page/log.html')) === 0;
    } else {
      LogStore.shouldSkipOutput = false;
    }
  }
}

/** @type {LogUrlWatcher} */
LogUrlWatcher.instance;
