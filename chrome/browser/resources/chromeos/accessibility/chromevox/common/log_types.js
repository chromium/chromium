// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class definitions of log that are stored in LogStore
 */

import {TreeDumper} from './tree_dumper.js';
import {QueueMode} from './tts_types.js';

/**
 * List of all types of logs supported.
 * Note that filter type checkboxes are shown in this order at the log page.
 * @enum {string}
 */
export const LogType = {
  SPEECH: 'speech',
  SPEECH_RULE: 'speechRule',
  BRAILLE: 'braille',
  BRAILLE_RULE: 'brailleRule',
  EARCON: 'earcon',
  EVENT: 'event',
  TEXT: 'text',
  TREE: 'tree',
};

/**
 * @typedef {{
 *   logType: !LogType,
 *   date: !Date,
 *   value: string
 * }}
 */
export let SerializableLog;

export class BaseLog {
  constructor(logType) {
    /**
     * @type {!LogType}
     */
    this.logType = logType;

    /**
     * @type {!Date}
     */
    this.date = new Date();
  }

  /** @return {!SerializableLog} */
  serialize() {
    return /** @type {!SerializableLog} */ (
        {logType: this.logType, date: this.date, value: this.toString()});
  }

  /** @return {string} */
  toString() {
    return '';
  }
}


export class EventLog extends BaseLog {
  /**
   * @param {!chrome.automation.AutomationEvent} event
   */
  constructor(event) {
    super(LogType.EVENT);

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
}


export class SpeechLog extends BaseLog {
  /**
   * @param {!string} textString
   * @param {!QueueMode} queueMode
   * @param {?string} category
   */
  constructor(textString, queueMode, category) {
    super(LogType.SPEECH);

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
}


export class TextLog extends BaseLog {
  /**
   * @param {string} logStr
   * @param {!LogType} logType
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
}


export class TreeLog extends BaseLog {
  /**
   * @param {!TreeDumper} logTree
   */
  constructor(logTree) {
    super(LogType.TREE);

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
}
