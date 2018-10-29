// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Store ChromeVox log.
 */

goog.provide('LogStore');
goog.provide('BaseLog');
goog.provide('TextLog');
goog.provide('TreeLog');

goog.require('TreeDumper');

/** @constructor */
BaseLog = function(logType) {
  /**
   * @type {!TextLog.LogType | !TreeLog.LogType}
   */
  this.logType = logType;

  /**
   * @type {!Date}
   */
  this.date = new Date();
};

/** @return {string} */
BaseLog.prototype.toString = function() {
  return '';
};

/**
 * @param {string} logStr
 * @param {!TextLog.LogType} logType
 * @constructor
 * @extends {BaseLog}
 */
TextLog = function(logStr, logType) {
  BaseLog.call(this, logType);

  /**
   * @type {string}
   * @private
   */
  this.logStr_ = logStr;
};

TextLog.prototype = {
  __proto__: BaseLog.prototype,

  /** @override */
  toString: function() {
    return this.logStr_;
  },
};

/**
 * Filter type checkboxes are shown in this order at the log page.
 * @enum {string}
 */
TextLog.LogType = {
  SPEECH: 'speech',
  SPEECH_RULE: 'speechRule',
  BRAILLE: 'braille',
  BRAILLE_RULE: 'brailleRule',
  EARCON: 'earcon',
  EVENT: 'event',
};

/**
 * @param {!TreeDumper} logTree
 * @constructor
 * @extends {BaseLog}
 */
TreeLog = function(logTree) {
  BaseLog.call(this, TreeLog.LogType.TREE);

  /**
   * @type {!TreeDumper}
   * @private
   */
  this.logTree_ = logTree;
};

TreeLog.prototype = {
  __proto__: BaseLog.prototype,

  /** @override */
  toString: function() {
    return this.logTree_.treeToString();
  },
};

/** @enum {string} */
TreeLog.LogType = {
  TREE: 'tree',
};

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
 * List of all LogTypes.
 * @return {!Array<!TextLog.LogType | !TreeLog.LogType>}
 */
LogStore.logTypes = function() {
  var types = [];
  for (var type in TextLog.LogType)
    types.push(TextLog.LogType[type]);
  for (var type in TreeLog.LogType)
    types.push(TreeLog.LogType[type]);
  return types;
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
    if (!this.logs_[index])
      continue;
    returnLogs.push(this.logs_[index]);
  }
  return returnLogs;
};

/**
 * Write a text log to this.logs_.
 * To add a message to logs, this function shuold be called.
 * @param {string} logContent
 * @param {!TextLog.LogType} logType
 */
LogStore.prototype.writeTextLog = function(logContent, logType) {
  if (this.shouldSkipOutput_())
    return;

  var log = new TextLog(logContent, logType);
  this.logs_[this.startIndex_] = log;
  this.startIndex_ += 1;
  if (this.startIndex_ == LogStore.LOG_LIMIT)
    this.startIndex_ = 0;
};

/**
 * Write a tree log to this.logs_.
 * To add a message to logs, this function shuold be called.
 * @param {!TreeDumper} logContent
 */
LogStore.prototype.writeTreeLog = function(logContent) {
  if (this.shouldSkipOutput_())
    return;

  var log = new TreeLog(logContent);
  this.logs_[this.startIndex_] = log;
  this.startIndex_ += 1;
  if (this.startIndex_ == LogStore.LOG_LIMIT)
    this.startIndex_ = 0;
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
  if (ChromeVoxState.instance.currentRange &&
      ChromeVoxState.instance.currentRange.start &&
      ChromeVoxState.instance.currentRange.start.node &&
      ChromeVoxState.instance.currentRange.start.node.root) {
    return ChromeVoxState.instance.currentRange.start.node.root.docUrl.indexOf(
               chrome.extension.getURL('cvox2/background/log.html')) == 0;
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
  if (!LogStore.instance)
    LogStore.instance = new LogStore();
  return LogStore.instance;
};
