// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output logger.
 */
import {LocalStorage} from '../../../common/local_storage.js';
import {LogType} from '../../common/log_types.js';
import {LogStore} from '../logging/log_store.js';

import {OutputRuleSpecifier} from './output_rules.js';

export class OutputFormatLogger {
  /**
   * @param {string} enableKey The key to enable logging in LocalStorage
   * @param {!LogType} type
   */
  constructor(enableKey, type) {
    /** @private {string} */
    this.str_ = '';
    /** @private {string} */
    this.storageEnabledKey_ = enableKey;
    /** @private {!LogType} */
    this.logType_ = type;
  }

  /** @return {boolean} */
  get loggingDisabled() {
    return !LocalStorage.get(this.storageEnabledKey_);
  }

  /** Sends the queued logs to the LogStore. */
  commitLogs() {
    if (this.str_) {
      LogStore.instance.writeTextLog(this.str_, this.logType_);
    }
  }

  /** @param {string} str */
  write(str) {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += str;
  }

  /**
   * @param {string} token
   * @param {string|undefined} value
   */
  writeTokenWithValue(token, value) {
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

  /** @param {string} token */
  writeToken(token) {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += '$' + token + ': ';
  }

  /**
   * @param {OutputRuleSpecifier} rule
   */
  writeRule(rule) {
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

  bufferClear() {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += '\nBuffer is cleared.\n';
  }

  /** @param {string} errorMsg */
  writeError(errorMsg) {
    if (this.loggingDisabled) {
      return;
    }
    this.str_ += 'ERROR with message: ';
    this.str_ += errorMsg;
    this.str_ += '\n';
  }
}
