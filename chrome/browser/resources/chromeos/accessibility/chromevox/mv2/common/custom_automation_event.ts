// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Custom Automation Event.
 *
 * An object similar to a chrome.automation.AutomationEvent that we can
 * construct, unlike the object from the extension system.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

type ActionType = chrome.automation.ActionType;
type AutomationEvent = chrome.automation.AutomationEvent;
type AutomationIntent = chrome.automation.AutomationIntent;
type AutomationNode = chrome.automation.AutomationNode;
type EventType = chrome.automation.EventType;

export interface CustomEventProperties {
  eventFrom?: string;
  eventFromAction?: ActionType;
  intents?: AutomationIntent[];
}

/**
 * An object we can use instead of a chrome.automation.AutomationEvent.
 */
export class CustomAutomationEvent {
  type: EventType;
  target: AutomationNode;
  eventFrom: string;
  eventFromAction: ActionType|undefined;
  intents: AutomationIntent[];

  constructor(
      type: EventType, target: AutomationNode,
      params: CustomEventProperties = {}) {
    this.type = type;
    this.target = target;
    this.eventFrom = params.eventFrom || '';
    this.eventFromAction = params.eventFromAction;
    this.intents = params.intents || [];
  }

  /**
   * Stops the propagation of this event.
   */
  stopPropagation(): void {
    throw Error('Can\'t call stopPropagation on a CustomAutomationEvent');
  }
}

export type ChromeVoxEvent = AutomationEvent|CustomAutomationEvent;

TestImportManager.exportForTesting(CustomAutomationEvent);
