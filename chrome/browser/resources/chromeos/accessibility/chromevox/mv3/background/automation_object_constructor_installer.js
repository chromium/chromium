// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides bindings to instantiate objects in the automation API.
 *
 * Due to restrictions in the extension system, it is not ordinarily possible to
 * construct an object defined by the extension API. However, given an instance
 * of that object, we can save its constructor for future use.
 */

const AutomationNode = chrome.automation.AutomationNode;
const AutomationEvent = chrome.automation.AutomationEvent;
const EventType = chrome.automation.EventType;

export const AutomationObjectConstructorInstaller = {
  /**
   * Installs the AutomationNode and AutomationEvent classes based on an
   * AutomationNode instance.
   * @param {AutomationNode} node
   */
  async init(node) {
    return new Promise(resolve => {
      chrome.automation.AutomationNode =
          /** @type {function (new:AutomationNode)} */ (node.constructor);
      node.addEventListener(
          EventType.CHILDREN_CHANGED, function installAutomationEvent(e) {
            chrome.automation.AutomationEvent =
                /** @type {function (new:AutomationEvent)} */ (e.constructor);
            node.removeEventListener(
                chrome.automation.EventType.CHILDREN_CHANGED,
                installAutomationEvent, true);
            resolve();
          }, true);
    });
  },
};
