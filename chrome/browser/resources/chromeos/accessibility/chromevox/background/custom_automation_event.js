// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Custom Automation Event.
 *
 * An object similar to a chrome.automation.AutomationEvent that we can
 * construct, unlike the object from the extension system.
 */

goog.provide('ChromeVoxEvent');
goog.provide('CustomAutomationEvent');

/**
 * @typedef{chrome.automation.AutomationEvent|CustomAutomationEvent}
 */
let ChromeVoxEvent;

/**
 * An object we can use instead of a chrome.automation.AutomationEvent.
 */
CustomAutomationEvent = class {
  /**
   * @param {chrome.automation.EventType} type The event type.
   * @param {!chrome.automation.AutomationNode} target The event target.
   * @param {string} eventFrom The source of this event.
   * @param {string} eventFromAction The accessibility action that caused this
   *     event.
   * @param {!Array<chrome.automation.AutomationIntent>} intents Intents for
   *     this event.
   */
  constructor(type, target, eventFrom, eventFromAction, intents) {
    this.type = type;
    this.target = target;
    this.eventFrom = eventFrom;
    this.eventFromAction = eventFromAction;
    this.intents = intents;
  }

  /**
   * Stops the propagation of this event.
   */
  stopPropagation() {
    throw Error('Can\'t call stopPropagation on a CustomAutomationEvent');
  }
};
