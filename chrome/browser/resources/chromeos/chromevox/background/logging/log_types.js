// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class definitions of log that are stored in LogStore
 */

goog.provide('BaseLog');
goog.provide('EventLog');
goog.provide('SpeechLog');
goog.provide('TextLog');
goog.provide('TreeLog');

goog.require('QueueMode');

/** @constructor */
BaseLog = function(logType) {
  /**
   * @type {!LogStore.LogType}
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
 * @param {!chrome.automation.AutomationEvent} event
 * @constructor
 * @extends {BaseLog}
 */
EventLog = function(event) {
  BaseLog.call(this, LogStore.LogType.EVENT);

  /**
   * @type {chrome.automation.EventType}
   * @private
   */
  this.type_ = event.type;

  /**
   * @type {string | undefined}
   * @private
   */
  this.targetName_ = event.target.name;

  /**
   * @type {string | undefined}
   * @private
   */
  this.rootName_ = event.target.root.name;

  /**
   * @type {string | undefined}
   * @private
   */
  this.docUrl_ = event.target.docUrl;
};
goog.inherits(EventLog, BaseLog);

/** @override */
EventLog.prototype.toString = function() {
  return `EventType = ${this.type_}, TargetName = ${this.targetName_}, ` +
      `RootName = ${this.rootName_}, DocumentURL = ${this.docUrl_}`;
};

/**
 * @param {!string} textString
 * @param {!QueueMode} queueMode
 * @param {?string} category
 * @constructor
 * @extends {BaseLog}
 */
SpeechLog = function(textString, queueMode, category) {
  BaseLog.call(this, LogStore.LogType.SPEECH);

  /**
   * @type {string}
   * @private
   */
  this.textString_ = textString;

  /**
   * @type {QueueMode}
   * @private
   */
  this.queueMode_ = queueMode;

  /**
   * @type {?string}
   * @private
   */
  this.category_ = category;
};
goog.inherits(SpeechLog, BaseLog);

/** @override */
SpeechLog.prototype.toString = function() {
  let logStr = 'Speak';
  if (this.queueMode_ == QueueMode.FLUSH) {
    logStr += ' (I)';
  } else if (this.queueMode_ == QueueMode.CATEGORY_FLUSH) {
    logStr += ' (C)';
  } else {
    logStr += ' (Q)';
  }
  if (this.category_) {
    logStr += ' category=' + this.category_;
  }
  logStr += ' "' + this.textString_ + '"';
  return logStr;
};

/**
 * @param {string} logStr
 * @param {!LogStore.LogType} logType
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
goog.inherits(TextLog, BaseLog);

/** @override */
TextLog.prototype.toString = function() {
  return this.logStr_;
};

/**
 * @param {!TreeDumper} logTree
 * @constructor
 * @extends {BaseLog}
 */
TreeLog = function(logTree) {
  BaseLog.call(this, LogStore.LogType.TREE);

  /**
   * @type {!TreeDumper}
   * @private
   */
  this.logTree_ = logTree;
};
goog.inherits(TreeLog, BaseLog);

/** @override */
TreeLog.prototype.toString = function() {
  return this.logTree_.treeToString();
};
