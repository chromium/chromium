// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Store ChromeVox log.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeConstants} from '../../common/bridge_constants.js';
import {BaseLog, LogType, TextLog, TreeLog} from '../../common/log_types.js';
import {SettingsManager} from '../../common/settings_manager.js';
import {TreeDumper} from '../../common/tree_dumper.js';
import {LoggingPrefs} from '../prefs.js';

const Action = BridgeConstants.LogStore.Action;
const TARGET = BridgeConstants.LogStore.TARGET;

/** Exported for testing. */
export const LOG_LIMIT = 3000;

export class LogStore {
  static instance: LogStore;

  /** Ring buffer of size LOG_LIMIT. */
  private logs_: BaseLog[];
  private shouldSkipOutput_ = false;
  /**
   * |this.logs_| is implemented as a ring buffer which starts
   * from |this.startIndex_| and ends at |this.startIndex_ - 1|.
   * In the initial state, this array is filled by undefined.
   */
  private startIndex_ = 0;

  constructor() {
    this.logs_ = Array(LOG_LIMIT);
    this.startIndex_ = 0;
  }

  /**
   * Creates logs of type |type| in order.
   * This is not the best way to create logs fast but
   * getLogsOfType() is not called often.
   */
  getLogsOfType(logType: LogType): BaseLog[] {
    const returnLogs: BaseLog[] = [];
    for (let i = 0; i < LOG_LIMIT; i++) {
      const index = (this.startIndex_ + i) % LOG_LIMIT;
      if (!this.logs_[index]) {
        continue;
      }
      if (this.logs_[index].logType === logType) {
        returnLogs.push(this.logs_[index]);
      }
    }
    return returnLogs;
  }

  /**
   * Create logs in order.
   * This is not the best way to create logs fast but
   * getLogs() is not called often.
   */
  getLogs(): BaseLog[] {
    const returnLogs: BaseLog[] = [];
    for (let i = 0; i < LOG_LIMIT; i++) {
      const index = (this.startIndex_ + i) % LOG_LIMIT;
      if (!this.logs_[index]) {
        continue;
      }
      returnLogs.push(this.logs_[index]);
    }
    return returnLogs;
  }

  /** @param text The text string written to the braille display. */
  writeBrailleLog(text: string): void {
    if (SettingsManager.getBoolean(LoggingPrefs.BRAILLE)) {
      const logStr = `Braille "${text}"`;
      this.writeTextLog(logStr, LogType.BRAILLE);
    }
  }

  /**
   * Write a text log to |this.logs_|.
   * To add a message to logs, this function should be called.
   */
  writeTextLog(logContent: string, logType: LogType): void {
    if (this.shouldSkipOutput_) {
      return;
    }

    this.writeLog(new TextLog(logContent, logType));
  }

  /**
   * Write a tree log to this.logs_.
   * To add a message to logs, this function should be called.
   */
  writeTreeLog(logContent: TreeDumper): void {
    if (this.shouldSkipOutput_) {
      return;
    }

    this.writeLog(new TreeLog(logContent));
  }

  /**
   * Write a log to this.logs_.
   * To add a message to logs, this function should be called.
   */
  writeLog(log: BaseLog): void {
    if (this.shouldSkipOutput_) {
      return;
    }

    this.logs_[this.startIndex_] = log;
    this.startIndex_ += 1;
    if (this.startIndex_ === LOG_LIMIT) {
      this.startIndex_ = 0;
    }
  }

  /** Clear this.logs_ and set to initial states. */
  clearLog(): void {
    this.logs_ = Array(LOG_LIMIT);
    this.startIndex_ = 0;
  }

  set shouldSkipOutput(newValue: boolean) {
    this.shouldSkipOutput_ = newValue;
  }

  static init(): void {
    LogStore.instance = new LogStore();

    BridgeHelper.registerHandler(
        TARGET, Action.CLEAR_LOG, () => LogStore.instance.clearLog());
    BridgeHelper.registerHandler(
        TARGET, Action.GET_LOGS,
        () => LogStore.instance.getLogs().map(log => log.serialize()));
  }
}

TestImportManager.exportForTesting(LogStore, ['LOG_LIMIT', LOG_LIMIT]);
