// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides bindings to instantiate objects in the automation API.
 *
 * Due to restrictions in the extension system, it is not ordinarily possible to
 * construct an object defined by the extension API. However, given an instance
 * of that object, we can save its constructor for future use.
 */

goog.provide('AutomationObjectConstructorInstaller');

/**
 * Installs the AutomationNode and AutomationEvent classes based on an
 * AutomationNode instance.
 * @param {chrome.automation.AutomationNode} node
 * @param {function()} callback Called when installation finishes.
 */
AutomationObjectConstructorInstaller.init = function(node, callback) {
  chrome.automation.AutomationNode =
      /** @type {function (new:chrome.automation.AutomationNode)} */ (
          node.constructor);
  node.addEventListener(
      chrome.automation.EventType.CHILDREN_CHANGED,
      function installAutomationEvent(e) {
        chrome.automation.AutomationEvent =
            /** @type {function (new:chrome.automation.AutomationEvent)} */ (
                e.constructor);
        node.removeEventListener(
            chrome.automation.EventType.CHILDREN_CHANGED,
            installAutomationEvent, true);
        callback();
      },
      true);
};
