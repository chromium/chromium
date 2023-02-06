// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages logging state, enable/disable, etc.
 */
import {AsyncUtil} from '../../../common/async_util.js';
import {LogType} from '../../common/log_types.js';
import {TreeDumper} from '../../common/tree_dumper.js';
import {ChromeVoxPrefs} from '../prefs.js';

import {LogStore} from './log_store.js';

export class LogManager {
  /** Takes a dump of the current accessibility tree and adds it to the logs. */
  static async logTreeDump() {
    const root = await AsyncUtil.getDesktop();
    LogStore.instance.writeTreeLog(new TreeDumper(root));
  }

  /** @param {boolean} value */
  static setLoggingEnabled(value) {
    for (const type of Object.values(ChromeVoxPrefs.loggingPrefs)) {
      ChromeVoxPrefs.instance.setLoggingPrefs(type, value);
    }
  }

  static showLogPage() {
    const logPage = {url: 'chromevox/log_page/log.html'};
    chrome.tabs.create(logPage);
  }
}
