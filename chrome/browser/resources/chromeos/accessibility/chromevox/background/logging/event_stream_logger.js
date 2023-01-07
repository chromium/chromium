// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Creates event stream logger.
 */

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
    /**
     * @type {!chrome.automation.AutomationNode}
     */
    this.node_ = node;

    /**
     * @type {function(!chrome.automation.AutomationEvent): void}
     * @private
     */
    this.watcher_ = this.eventStreamLogging.bind(this);
  }

  /**
   * Adds eventStreamLogging to this handler.
   * @param {chrome.automation.EventType} eventType
   * @protected
   */
  addWatcher_(eventType) {
    this.node_.addEventListener(eventType, this.watcher_, false);
  }

  /**
   * Removes eventStreamLogging from this handler.
   * @param {chrome.automation.EventType} eventType
   * @protected
   */
  removeWatcher_(eventType) {
    this.node_.removeEventListener(eventType, this.watcher_, false);
  }

  /**
   * @param {!chrome.automation.AutomationEvent} evt
   */
  eventStreamLogging(evt) {
    const eventLog = new EventLog(evt);
    LogStore.getInstance().writeLog(eventLog);
    console.log(eventLog.toString());
  }

  /**
   * @param {chrome.automation.EventType} eventType
   * @param {boolean} checked
   */
  notifyEventStreamFilterChanged(eventType, checked) {
    if (checked) {
      this.addWatcher_(eventType);
    } else {
      this.removeWatcher_(eventType);
    }
  }

  /**
   * @param {boolean} checked
   */
  notifyEventStreamFilterChangedAll(checked) {
    for (const type in chrome.automation.EventType) {
      if (localStorage[chrome.automation.EventType[type]] === 'true') {
        this.notifyEventStreamFilterChanged(
            chrome.automation.EventType[type], checked);
      }
    }
  }

  /**
   * Initializes global state for EventStreamLogger.
   * @private
   */
  static init_() {
    chrome.automation.getDesktop(function(desktop) {
      EventStreamLogger.instance = new EventStreamLogger(desktop);
      EventStreamLogger.instance.notifyEventStreamFilterChangedAll(
          localStorage['enableEventStreamLogging'] === 'true');
    });
  }
}


/**
 * Global instance.
 * @type {EventStreamLogger}
 */
EventStreamLogger.instance;


EventStreamLogger.init_();

BridgeHelper.registerHandler(
    Constants.TARGET, Constants.Action.NOTIFY_EVENT_STREAM_FILTER_CHANGED,
    (name, enabled) =>
        EventStreamLogger.instance.notifyEventStreamFilterChanged(
            name, enabled));
