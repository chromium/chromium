// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Store ChromeVox log.
 */

goog.provide('LogStore');

goog.require('TreeDumper');
goog.require('BaseLog');
goog.require('EventLog');
goog.require('SpeechLog');
goog.require('TextLog');
goog.require('TreeLog');

/** @constructor */
LogStore = function() {
  /**
   * Ring buffer of size this.LOG_LIMIT
   * @type {!Array<BaseLog>}
   * @private
   */
  this.logs_ = Array(LogStore.LOG_LIMIT);

  /*
   * this.logs_ is implemented as a ring buffer which starts
   * from this.startIndex_ and ends at this.startIndex_-1
   * In the initial state, this array is filled by undefined.
   * @type {number}
   * @private
   */
  this.startIndex_ = 0;
};

/**
 * @const
 * @private
 */
LogStore.LOG_LIMIT = 3000;

/**
 * List of all LogType.
 * Note that filter type checkboxes are shown in this order at the log page.
 * @enum {string}
 */
LogStore.LogType = {
  SPEECH: 'speech',
  SPEECH_RULE: 'speechRule',
  BRAILLE: 'braille',
  BRAILLE_RULE: 'brailleRule',
  EARCON: 'earcon',
  EVENT: 'event',
  TREE: 'tree',
};

/**
 * Creates logs of type |type| in order.
 * This is not the best way to create logs fast but
 * getLogsOfType() is not called often.
 * @param {!LogStore.LogType} LogType
 * @return {!Array<BaseLog>}
 */
LogStore.prototype.getLogsOfType = function(LogType) {
  var returnLogs = [];
  for (var i = 0; i < LogStore.LOG_LIMIT; i++) {
    var index = (this.startIndex_ + i) % LogStore.LOG_LIMIT;
    if (!this.logs_[index]) {
      continue;
    }
    if (this.logs_[index].logType == LogType) {
      returnLogs.push(this.logs_[index]);
    }
  }
  return returnLogs;
};

/**
 * Create logs in order.
 * This is not the best way to create logs fast but
 * getLogs() is not called often.
 * @return {!Array<BaseLog>}
 */
LogStore.prototype.getLogs = function() {
  var returnLogs = [];
  for (var i = 0; i < LogStore.LOG_LIMIT; i++) {
    var index = (this.startIndex_ + i) % LogStore.LOG_LIMIT;
    if (!this.logs_[index]) {
      continue;
    }
    returnLogs.push(this.logs_[index]);
  }
  return returnLogs;
};

/**
 * Write a text log to this.logs_.
 * To add a message to logs, this function should be called.
 * @param {string} logContent
 * @param {!LogStore.LogType} LogType
 */
LogStore.prototype.writeTextLog = function(logContent, LogType) {
  if (this.shouldSkipOutput_()) {
    return;
  }

  this.writeLog(new TextLog(logContent, LogType));
};

/**
 * Write a tree log to this.logs_.
 * To add a message to logs, this function should be called.
 * @param {!TreeDumper} logContent
 */
LogStore.prototype.writeTreeLog = function(logContent) {
  if (this.shouldSkipOutput_()) {
    return;
  }

  this.writeLog(new TreeLog(logContent));
};

/**
 * Write a log to this.logs_.
 * To add a message to logs, this function should be called.
 * @param {!BaseLog} log
 */
LogStore.prototype.writeLog = function(log) {
  if (this.shouldSkipOutput_()) {
    return;
  }

  this.logs_[this.startIndex_] = log;
  this.startIndex_ += 1;
  if (this.startIndex_ == LogStore.LOG_LIMIT) {
    this.startIndex_ = 0;
  }
};

/**
 * Clear this.logs_.
 * Set to initial states.
 */
LogStore.prototype.clearLog = function() {
  this.logs_ = Array(LogStore.LOG_LIMIT);
  this.startIndex_ = 0;
};

/** @private @return {boolean} */
LogStore.prototype.shouldSkipOutput_ = function() {
  var ChromeVoxState = chrome.extension.getBackgroundPage()['ChromeVoxState'];
  if (ChromeVoxState.instance && ChromeVoxState.instance.currentRange &&
      ChromeVoxState.instance.currentRange.start &&
      ChromeVoxState.instance.currentRange.start.node &&
      ChromeVoxState.instance.currentRange.start.node.root) {
    return ChromeVoxState.instance.currentRange.start.node.root.docUrl.indexOf(
               chrome.extension.getURL('background/logging/log.html')) == 0;
  }
  return false;
};

/**
 * Global instance.
 * @type {LogStore}
 */
LogStore.instance;

/**
 * @return {LogStore}
 */
LogStore.getInstance = function() {
  if (!LogStore.instance) {
    LogStore.instance = new LogStore();
  }
  return LogStore.instance;
};
