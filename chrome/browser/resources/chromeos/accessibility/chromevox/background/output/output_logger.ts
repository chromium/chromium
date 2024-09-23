// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output logger.
 */
import {LocalStorage} from '/common/local_storage.js';

import {LogType} from '../../common/log_types.js';
import {LogStore} from '../logging/log_store.js';

import {OutputRuleSpecifier} from './output_rules.js';

export class OutputFormatLogger {
  private str_: string = '';
  private storageEnabledKey_: string;
  private logType_: LogType;


  /** @param enableKey The key to enable logging in LocalStorage. */
  constructor(enableKey: string, type: LogType) {
    this.storageEnabledKey_ = enableKey;
    this.logType_ = type;
  }

  get loggingDisabled(): boolean {
    return !LocalStorage.get(this.storageEnabledKey_);
  }

  /** Sends the queued logs to the LogStore. */
  commitLogs(): void {
    if (this.str_) {
      LogStore.instance.writeTextLog(this.str_, this.logType_);
    }
  }

  write(str: string): void {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += str;
  }

  writeTokenWithValue(token: string, value?: string): void {
    if (this.loggingDisabled) {
      return;
    }
    this.writeToken(token);
    if (value) {
      this.str_ += value;
    } else {
      this.str_ += 'EMPTY';
    }
    this.str_ += '\n';
  }

  writeToken(token: string): void {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += '$' + token + ': ';
  }

  writeRule(rule: OutputRuleSpecifier): void {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += 'RULE: ';
    this.str_ += rule.event + ' ' + rule.role;
    if (rule.navigation) {
      this.str_ += ' ' + rule.navigation;
    }
    if (rule.output) {
      this.str_ += ' ' + rule.output;
    }
    this.str_ += '\n';
  }

  bufferClear(): void {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += '\nBuffer is cleared.\n';
  }

  writeError(errorMsg: string): void {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += 'ERROR with message: ';
    this.str_ += errorMsg;
    this.str_ += '\n';
  }
}
