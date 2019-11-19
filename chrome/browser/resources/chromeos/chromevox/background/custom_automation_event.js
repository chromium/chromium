// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Custom Automation Event.
 *
 * An object similar to a chrome.automation.AutomationEvent that we can
 * construct, unlike the object from the extension system.
 */

goog.provide('CustomAutomationEvent');

/**
 * An object we can use instead of a chrome.automation.AutomationEvent.
 * @constructor
 * @extends {chrome.automation.AutomationEvent}
 * @param {chrome.automation.EventType} type The event type.
 * @param {!chrome.automation.AutomationNode} target The event target.
 * @param {string} eventFrom The source of this event.
 */
var CustomAutomationEvent = function(type, target, eventFrom) {
  this.type = type;
  this.target = target;
  this.eventFrom = eventFrom;
};

/**
 * @override
 */
CustomAutomationEvent.prototype.stopPropagation = function() {
  throw Error('Can\'t call stopPropagation on a CustomAutomationEvent');
};
