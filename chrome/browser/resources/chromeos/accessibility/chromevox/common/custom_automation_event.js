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

/**
 * @typedef{chrome.automation.AutomationEvent|CustomAutomationEvent}
 */
export let ChromeVoxEvent;

/**
 * An object we can use instead of a chrome.automation.AutomationEvent.
 */
export class CustomAutomationEvent {
  /**
   * @param {chrome.automation.EventType} type The event type.
   * @param {!chrome.automation.AutomationNode} target The event target.
   * @param {!{eventFrom: (string|undefined),
   *           eventFromAction: (chrome.automation.ActionType|undefined),
   *           intents: (!Array<chrome.automation.AutomationIntent>|undefined)
   *        }} params
   */
  constructor(type, target, params = {}) {
    this.type = type;
    this.target = target;
    this.eventFrom = params.eventFrom || '';
    this.eventFromAction = params.eventFromAction || '';
    this.intents = params.intents || [];
  }

  /**
   * Stops the propagation of this event.
   */
  stopPropagation() {
    throw Error('Can\'t call stopPropagation on a CustomAutomationEvent');
  }
}

TestImportManager.exportForTesting(CustomAutomationEvent);
