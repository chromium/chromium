// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class definitions of log that are stored in LogStore
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {TreeDumper} from './tree_dumper.js';
import {QueueMode} from './tts_types.js';

type AutomationEvent = chrome.automation.AutomationEvent;
type EventType = chrome.automation.EventType;

/**
 * Supported log types.
 * Note that filter type checkboxes are shown in this order at the log page.
 */
export enum LogType {
  SPEECH = 'speech',
  SPEECH_RULE = 'speechRule',
  BRAILLE = 'braille',
  BRAILLE_RULE = 'brailleRule',
  EARCON = 'earcon',
  EVENT = 'event',
  TEXT = 'text',
  TREE = 'tree',
}

// TODO(anastasi): Convert this to an interface after typescript migration.
export abstract class SerializableLog {
  abstract logType: LogType;
  abstract date: string;
  abstract value: string;
}

export class BaseLog {
  logType: LogType;
  date: Date;

  constructor(logType: LogType) {
    this.logType = logType;
    this.date = new Date();
  }

  serialize(): SerializableLog {
    return {
      logType: this.logType,
      date: this.date.toString(),  // Explicit toString(); converts either way.
      value: this.toString(),
    };
  }

  toString(): string {
    return '';
  }
}

export class EventLog extends BaseLog {
  private docUrl_?: string;
  private rootName_?: string;
  private targetName_?: string;
  private type_: EventType;

  constructor(event: AutomationEvent) {
    super(LogType.EVENT);

    this.type_ = event.type;
    this.targetName_ = event.target.name;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.rootName_ = event.target.root!.name;
    this.docUrl_ = event.target.docUrl;
  }

  override toString(): string {
    return `EventType = ${this.type_}, TargetName = ${this.targetName_}, ` +
        `RootName = ${this.rootName_}, DocumentURL = ${this.docUrl_}`;
  }
}

export class SpeechLog extends BaseLog {
  private category_?: string;
  private queueMode_: QueueMode;
  private textString_: string;

  constructor(textString: string, queueMode: QueueMode, category?: string) {
    super(LogType.SPEECH);

    this.textString_ = textString;
    this.queueMode_ = queueMode;
    this.category_ = category;
  }

  override toString(): string {
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
  private logStr_: string;

  constructor(logStr: string, logType: LogType) {
    super(logType);

    this.logStr_ = logStr;
  }

  override toString(): string {
    return this.logStr_;
  }
}

export class TreeLog extends BaseLog {
  private tree_: TreeDumper;

  constructor(tree: TreeDumper) {
    super(LogType.TREE);

    this.tree_ = tree;
  }

  override toString(): string {
    return this.tree_.treeToString();
  }
}

TestImportManager.exportForTesting(['LogType', LogType]);
