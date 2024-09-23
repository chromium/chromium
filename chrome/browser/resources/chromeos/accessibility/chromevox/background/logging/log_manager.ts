// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages logging state, enable/disable, etc.
 */
import {AsyncUtil} from '/common/async_util.js';

import {TreeDumper} from '../../common/tree_dumper.js';
import {ChromeVoxPrefs, LoggingPrefs} from '../prefs.js';

import {LogStore} from './log_store.js';

export class LogManager {
  /** Takes a dump of the current accessibility tree and adds it to the logs. */
  static async logTreeDump(): Promise<void> {
    const root = await AsyncUtil.getDesktop();
    LogStore.instance.writeTreeLog(new TreeDumper(root));
  }

  static setLoggingEnabled(value: boolean): void {
    for (const type of Object.values(LoggingPrefs)) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      ChromeVoxPrefs.instance!.setLoggingPrefs(type, value);
    }
  }

  static showLogPage(): void {
    // Use chrome.windows API to ensure page is opened in Ash-chrome.
    chrome.windows.create({
      url: 'chromevox/log_page/log.html',
      type: chrome.windows.CreateType.PANEL,
    });
  }
}
