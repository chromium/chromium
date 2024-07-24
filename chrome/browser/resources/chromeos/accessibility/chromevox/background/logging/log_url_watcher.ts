// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Watches the currently focused URL to verify if logging should
 * occur.
 */
import {CursorRange} from '/common/cursors/range.js';

import {ChromeVoxRange, ChromeVoxRangeObserver} from '../chromevox_range.js';
import {ChromeVoxPrefs} from '../prefs.js';

import {LogStore} from './log_store.js';

export class LogUrlWatcher implements ChromeVoxRangeObserver {
  static instance: LogUrlWatcher | null;

  static init(): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    ChromeVoxPrefs.instance!.enableOrDisableLogUrlWatcher();
  }

  static create(): void {
    if (LogUrlWatcher.instance) {
      return;
    }
    LogUrlWatcher.instance = new LogUrlWatcher();
    ChromeVoxRange.addObserver(LogUrlWatcher.instance);
    // Initialize using the current range.
    LogUrlWatcher.instance.onCurrentRangeChanged(ChromeVoxRange.current);
  }

  static destroy(): void {
    if (!LogUrlWatcher.instance) {
      return;
    }
    ChromeVoxRange.removeObserver(LogUrlWatcher.instance);
    LogUrlWatcher.instance = null;
  }

  /** ChromeVoxRangeObserver implementation. */
  onCurrentRangeChanged(range: CursorRange | null, _fromEditing?: boolean)
    : void {
    if (range && range.start && range.start.node && range.start.node.root) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      LogStore.instance.shouldSkipOutput =
          range.start.node.root.docUrl!.indexOf(
              chrome.extension.getURL('chromevox/log_page/log.html')) === 0;
    } else {
      LogStore.instance.shouldSkipOutput = false;
    }
  }
}

