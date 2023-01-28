// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Creates event stream logger.
 */
import {AsyncUtil} from '../../../common/async_util.js';
import {LocalStorage} from '../../../common/local_storage.js';
import {BridgeConstants} from '../../common/bridge_constants.js';
import {BridgeHelper} from '../../common/bridge_helper.js';
import {EventLog} from '../../common/log_types.js';
import {SettingsManager} from '../../common/settings_manager.js';

import {LogStore} from './log_store.js';

const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const Action = BridgeConstants.EventStreamLogger.Action;
const TARGET = BridgeConstants.EventStreamLogger.TARGET;

export class EventStreamLogger {
  constructor(node) {
    /** @private {!AutomationNode} */
    this.node_ = node;

    /** @private {function(!AutomationEvent): void} */
    this.listener_ = event => this.onEvent_(event);
  }

  /** Initializes global state for EventStreamLogger. */
  static async init() {
    const desktop = await AsyncUtil.getDesktop();
    EventStreamLogger.instance = new EventStreamLogger(desktop);
    EventStreamLogger.instance.updateAllFilters(
        SettingsManager.getBoolean('enableEventStreamLogging'));

    BridgeHelper.registerHandler(
        TARGET, Action.NOTIFY_EVENT_STREAM_FILTER_CHANGED,
        (name, enabled) =>
            EventStreamLogger.instance.onFilterChanged_(name, enabled));
  }

  /** @param {boolean} checked */
  updateAllFilters(checked) {
    for (const type of Object.values(EventType)) {
      if (LocalStorage.get(type)) {
        this.onFilterChanged_(type, checked);
      }
    }
  }

  // ============ Private methods =============

  /**
   * Adds onEvent_ to this handler.
   * @param {EventType} eventType
   * @private
   */
  addListener_(eventType) {
    this.node_.addEventListener(eventType, this.listener_, false);
  }

  /**
   * Removes onEvent_ from this handler.
   * @param {EventType} eventType
   * @private
   */
  removeListener_(eventType) {
    this.node_.removeEventListener(eventType, this.listener_, false);
  }

  /**
   * @param {!AutomationEvent} evt
   * @private
   */
  onEvent_(evt) {
    const eventLog = new EventLog(evt);
    LogStore.instance.writeLog(eventLog);
    console.log(eventLog.toString());
  }

  /**
   * @param {EventType} eventType
   * @param {boolean} checked
   * @private
   */
  onFilterChanged_(eventType, checked) {
    if (checked) {
      this.addListener_(eventType);
    } else {
      this.removeListener_(eventType);
    }
  }
}

/** @type {EventStreamLogger} */
EventStreamLogger.instance;
