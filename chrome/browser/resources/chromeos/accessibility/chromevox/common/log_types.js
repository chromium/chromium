// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class definitions of log that are stored in LogStore
 */

import {TreeDumper} from './tree_dumper.js';
import {QueueMode} from './tts_types.js';

const AutomationEvent = chrome.automation.AutomationEvent;
const EventType = chrome.automation.EventType;

/**
 * Supported log types.
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
 *   date: !string,
 *   value: string
 * }}
 */
export let SerializableLog;

export class BaseLog {
  constructor(logType) {
    /** @public {!LogType} */
    this.logType = logType;

    /** @public {!Date} */
    this.date = new Date();
  }

  /** @return {!SerializableLog} */
  serialize() {
    return /** @type {!SerializableLog} */ ({
      logType: this.logType,
      date: this.date.toString(),  // Explicit toString(); converts either way.
      value: this.toString(),
    });
  }

  /** @return {string} */
  toString() {
    return '';
  }
}

export class EventLog extends BaseLog {
  /** @param {!AutomationEvent} event */
  constructor(event) {
    super(LogType.EVENT);

    /** @private {EventType} */
    this.type_ = event.type;

    /** @private {string|undefined} */
    this.targetName_ = event.target.name;

    /** @private {string|undefined} */
    this.rootName_ = event.target.root.name;

    /** @private {string|undefined} */
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

    /** @private {string} */
    this.textString_ = textString;

    /** @private {QueueMode} */
    this.queueMode_ = queueMode;

    /** @private {?string} */
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

    /** @private {string} */
    this.logStr_ = logStr;
  }

  /** @override */
  toString() {
    return this.logStr_;
  }
}

export class TreeLog extends BaseLog {
  /** @param {!TreeDumper} tree */
  constructor(tree) {
    super(LogType.TREE);

    /** @private {!TreeDumper} */
    this.tree_ = tree;
  }

  /** @override */
  toString() {
    return this.tree_.treeToString();
  }
}
