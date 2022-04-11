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

BaseLog = class {
  constructor(logType) {
    /**
     * @type {!LogStore.LogType}
     */
    this.logType = logType;

    /**
     * @type {!Date}
     */
    this.date = new Date();
  }

  /** @return {string} */
  toString() {
    return '';
  }
};


EventLog = class extends BaseLog {
  /**
   * @param {!chrome.automation.AutomationEvent} event
   */
  constructor(event) {
    super(LogStore.LogType.EVENT);

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
  }

  /** @override */
  toString() {
    return `EventType = ${this.type_}, TargetName = ${this.targetName_}, ` +
        `RootName = ${this.rootName_}, DocumentURL = ${this.docUrl_}`;
  }
};


SpeechLog = class extends BaseLog {
  /**
   * @param {!string} textString
   * @param {!QueueMode} queueMode
   * @param {?string} category
   */
  constructor(textString, queueMode, category) {
    super(LogStore.LogType.SPEECH);

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
  }

  /** @override */
  toString() {
    let logStr = 'Speak';
    if (this.queueMode_ === QueueMode.FLUSH) {
      logStr += ' (F)';
    } else if (this.queueMode_ === QueueMode.CATEGORY_FLUSH) {
      logStr += ' (C)';
    } else if (this.queueMode_ === QueueMode.INTERJECT) {
      logStr += ' (I)';
    } else {
      logStr += ' (Q)';
    }
    if (this.category_) {
      logStr += ' category=' + this.category_;
    }
    logStr += ' "' + this.textString_ + '"';
    return logStr;
  }
};


TextLog = class extends BaseLog {
  /**
   * @param {string} logStr
   * @param {!LogStore.LogType} logType
   */
  constructor(logStr, logType) {
    super(logType);

    /**
     * @type {string}
     * @private
     */
    this.logStr_ = logStr;
  }

  /** @override */
  toString() {
    return this.logStr_;
  }
};


TreeLog = class extends BaseLog {
  /**
   * @param {!TreeDumper} logTree
   */
  constructor(logTree) {
    super(LogStore.LogType.TREE);

    /**
     * @type {!TreeDumper}
     * @private
     */
    this.logTree_ = logTree;
  }

  /** @override */
  toString() {
    return this.logTree_.treeToString();
  }
};
