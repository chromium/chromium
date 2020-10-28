// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Creates event stream logger.
 */

goog.provide('EventStreamLogger');

goog.require('LogStore');
goog.require('EventLog');

goog.scope(function() {
const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
EventStreamLogger = class {
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
};


/**
 * Global instance.
 * @type {EventStreamLogger}
 */
EventStreamLogger.instance;


EventStreamLogger.init_();
});  // goog.scope
