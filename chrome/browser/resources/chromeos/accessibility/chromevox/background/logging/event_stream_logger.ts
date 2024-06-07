// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Creates event stream logger.
 */
import {AsyncUtil} from '/common/async_util.js';
import {BridgeHelper} from '/common/bridge_helper.js';
import {LocalStorage} from '/common/local_storage.js';

import {BridgeConstants} from '../../common/bridge_constants.js';
import {EventLog} from '../../common/log_types.js';
import {SettingsManager} from '../../common/settings_manager.js';

import {LogStore} from './log_store.js';

type AutomationEvent = chrome.automation.AutomationEvent;
type AutomationNode = chrome.automation.AutomationNode;
import EventType = chrome.automation.EventType;

const Action = BridgeConstants.EventStreamLogger.Action;
const TARGET = BridgeConstants.EventStreamLogger.TARGET;

export class EventStreamLogger {
  private listener_ = (event: AutomationEvent): void => this.onEvent_(event);
  private node_: AutomationNode;

  static instance: EventStreamLogger;

  constructor(node: AutomationNode) {
    this.node_ = node;
  }

  /** Initializes global state for EventStreamLogger. */
  static async init(): Promise<void> {
    const desktop = await AsyncUtil.getDesktop();
    EventStreamLogger.instance = new EventStreamLogger(desktop);
    EventStreamLogger.instance.updateAllFilters(
        SettingsManager.getBoolean('enableEventStreamLogging'));

    BridgeHelper.registerHandler(
        TARGET, Action.NOTIFY_EVENT_STREAM_FILTER_CHANGED,
        (name: EventType, enabled: boolean) =>
            EventStreamLogger.instance.onFilterChanged_(name, enabled));
  }

  updateAllFilters(checked: boolean): void {
    for (const type of Object.values(EventType)) {
      if (LocalStorage.get(type)) {
        this.onFilterChanged_(type, checked);
      }
    }
  }

  // ============ Private methods =============

  /** Adds onEvent_ to this handler. */
  private addListener_(eventType: EventType): void {
    this.node_.addEventListener(eventType, this.listener_, false);
  }

  /** Removes onEvent_ from this handler. */
  private removeListener_(eventType: EventType): void {
    this.node_.removeEventListener(eventType, this.listener_, false);
  }

  private onEvent_(evt: AutomationEvent): void {
    const eventLog = new EventLog(evt);
    LogStore.instance.writeLog(eventLog);
    console.log(eventLog.toString());
  }

  private onFilterChanged_(eventType: EventType, checked: boolean): void {
    if (checked) {
      this.addListener_(eventType);
    } else {
      this.removeListener_(eventType);
    }
  }
}
