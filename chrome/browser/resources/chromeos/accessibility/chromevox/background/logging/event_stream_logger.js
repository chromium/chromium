// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Creates event stream logger.
 */

import {LocalStorage} from '../../../common/local_storage.js';
import {BridgeConstants} from '../../common/bridge_constants.js';
import {BridgeHelper} from '../../common/bridge_helper.js';
import {EventLog} from '../../common/log_types.js';

import {LogStore} from './log_store.js';

const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const Constants = BridgeConstants.EventStreamLogger;

export class EventStreamLogger {
  constructor(node) {
    /** @private {!AutomationNode} */
    this.node_ = node;

    /** @private {function(!AutomationEvent): void} */
    this.watcher_ = event => this.eventStreamLogging(event);
  }

  /**
   * Adds eventStreamLogging to this handler.
   * @param {EventType} eventType
   * @protected
   */
  addWatcher_(eventType) {
    this.node_.addEventListener(eventType, this.watcher_, false);
  }

  /**
   * Removes eventStreamLogging from this handler.
   * @param {EventType} eventType
   * @protected
   */
  removeWatcher_(eventType) {
    this.node_.removeEventListener(eventType, this.watcher_, false);
  }

  /**
   * @param {!AutomationEvent} evt
   */
  eventStreamLogging(evt) {
    const eventLog = new EventLog(evt);
    LogStore.instance.writeLog(eventLog);
    console.log(eventLog.toString());
  }

  /**
   * @param {EventType} eventType
   * @param {boolean} checked
   */
  notifyEventStreamFilterChanged(eventType, checked) {
    if (checked) {
      this.addWatcher_(eventType);
    } else {
      this.removeWatcher_(eventType);
    }
  }

  /** @param {boolean} checked */
  notifyEventStreamFilterChangedAll(checked) {
    for (const type in EventType) {
      if (LocalStorage.get(EventType[type])) {
        this.notifyEventStreamFilterChanged(EventType[type], checked);
      }
    }
  }

  /** Initializes global state for EventStreamLogger. */
  static init() {
    chrome.automation.getDesktop(function(desktop) {
      EventStreamLogger.instance = new EventStreamLogger(desktop);
      EventStreamLogger.instance.notifyEventStreamFilterChangedAll(
          LocalStorage.get('enableEventStreamLogging'));

      BridgeHelper.registerHandler(
          Constants.TARGET, Constants.Action.NOTIFY_EVENT_STREAM_FILTER_CHANGED,
          (name, enabled) =>
              EventStreamLogger.instance.notifyEventStreamFilterChanged(
                  name, enabled));
    });
  }
}

/**
 * Global instance.
 * @type {EventStreamLogger}
 */
EventStreamLogger.instance;
