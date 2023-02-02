// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Watches the currently focused URL to verify if logging should
 * occur.
 */
import {CursorRange} from '../../../common/cursors/range.js';
import {ChromeVoxRange, ChromeVoxRangeObserver} from '../chromevox_range.js';
import {ChromeVoxPrefs} from '../prefs.js';

import {LogStore} from './log_store.js';

/** @implements {ChromeVoxRangeObserver} */
export class LogUrlWatcher {
  static init() {
    ChromeVoxPrefs.instance.enableOrDisableLogUrlWatcher_();
  }

  static create() {
    if (LogUrlWatcher.instance) {
      return;
    }
    LogUrlWatcher.instance = new LogUrlWatcher();
    ChromeVoxRange.addObserver(LogUrlWatcher.instance);
    // Initialize using the current range.
    LogUrlWatcher.instance.onCurrentRangeChanged(ChromeVoxRange.current);
  }

  static destroy() {
    if (!LogUrlWatcher.instance) {
      return;
    }
    ChromeVoxRange.removeObserver(LogUrlWatcher.instance);
    LogUrlWatcher.instance = null;
  }

  /**
   * @param {?CursorRange} range The new range.
   * @param {boolean=} opt_fromEditing
   * @override
   */
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
