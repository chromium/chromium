// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox mouse handler.
 */

goog.provide('BackgroundMouseHandler');

goog.require('BaseAutomationHandler');

const INTERVAL_MS_BETWEEN_HIT_TESTS = 50;

var AutomationEvent = chrome.automation.AutomationEvent;
var EventType = chrome.automation.EventType;

/**
 * @constructor
 * @extends {BaseAutomationHandler}
 */
BackgroundMouseHandler = function() {
  chrome.automation.getDesktop(function(desktop) {
    BaseAutomationHandler.call(this, desktop);
    /**
     *
     * The desktop node.
     *
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;
    this.addListener_(EventType.MOUSE_MOVED, this.onMouseMove);

    // When |this.isWaitingBeforeHitTest| is true, we should never run a
    // hittest, and there should be a timer running so that it can be set to
    // false.
    this.isWaitingBeforeHitTest_ = false;
    this.hasPendingEvents_ = false;
    this.mouseX_ = 0;
    this.mouseY_ = 0;
  }.bind(this));
};

BackgroundMouseHandler.prototype = {
  __proto__: BaseAutomationHandler.prototype,

  /**
   * Starts a timer which, when it finishes, calls runHitTest if an event was
   * waiting to resolve to a node.
   */
  startTimer: function() {
    this.isWaitingBeforeHitTest_ = true;
    setTimeout(() => {
      this.isWaitingBeforeHitTest_ = false;
      if (this.hasPendingEvents_) {
        this.runHitTest();
      }
    }, INTERVAL_MS_BETWEEN_HIT_TESTS);
  },

  /**
   * Performs a hittest using the most recent mouse coordinates received in
   * onMouseMove, and then starts a timer so that further hittests don't occur
   * immediately after.
   *
   * Note that runHitTest is only ever called when |isWaitingBeforeHitTest| is
   * false and |hasPendingEvents| is true.
   */
  runHitTest: function() {
    this.desktop_.hitTest(this.mouseX_, this.mouseY_, EventType.HOVER);
    this.hasPendingEvents_ = false;
    this.startTimer();
  },

  /**
   * Handles mouse move events. If the timer is running, it will perform a
   * hittest on the most recent event; otherwise request a hittest immediately.
   * @param {AutomationEvent} evt The mouse move event to process.
   */
  onMouseMove: function(evt) {
    this.mouseX_ = evt.mouseX;
    this.mouseY_ = evt.mouseY;
    this.hasPendingEvents_ = true;
    if (!this.isWaitingBeforeHitTest_) {
      this.runHitTest();
    }
  },
};
