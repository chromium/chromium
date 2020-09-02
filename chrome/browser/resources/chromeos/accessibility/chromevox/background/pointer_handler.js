// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox pointer handler. A pointer, in this context, is
 * either user touch or mouse input.
 */

goog.provide('PointerHandler');

goog.require('BaseAutomationHandler');

const AutomationEvent = chrome.automation.AutomationEvent;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;

PointerHandler = class extends BaseAutomationHandler {
  constructor() {
    super(null);

    /** @private {boolean|undefined} */
    this.hasPendingEvents_;
    /** @private {number|undefined} */
    this.mouseX_;
    /** @private {number|undefined} */
    this.mouseY_;
    /** @private {!Date} */
    this.lastNoPointerAnchorEarconPlayedTime_ = new Date();
    /** @private {!AutomationNode|undefined} */
    this.lastValidNodeBeforePointerInvalidation_;

    chrome.automation.getDesktop((desktop) => {
      this.node_ = desktop;
      this.addListener_(EventType.MOUSE_MOVED, this.onMouseMove);

      // This is needed for ARC++ which sends back hovers when we send mouse
      // moves.
      this.addListener_(EventType.HOVER, (evt) => {
        this.handleHitTestResult(evt.target);
        this.runHitTest();
      });

      this.hasPendingEvents_ = false;
      this.mouseX_ = 0;
      this.mouseY_ = 0;
    });

    if (localStorage['speakTextUnderMouse'] == String(true)) {
      chrome.accessibilityPrivate.enableChromeVoxMouseEvents(true);
    }
  }

  /**
   * Performs a hit test using the most recent mouse coordinates received in
   * onMouseMove or onMove (a e.g. for touch explore).
   *
   * Note that runHitTest only ever does a hit test when |hasPendingEvents| is
   * true.
   * @param {boolean} isTouch
   */
  runHitTest(isTouch = false) {
    if (this.mouseX_ === undefined || this.mouseY_ === undefined) {
      return;
    }
    if (!this.hasPendingEvents_) {
      return;
    }

    if (isTouch) {
      // TODO(accessibility): hit testing seems to be broken in some cases e.g.
      // on the main CFM UI. Synthesize mouse moves with the touch
      // accessibility flag for now for touch-based user gestures. Eliminate
      // this branch once hit testing is fixed.
      this.synthesizeMouseMove();
    } else {
      // Otherwise, use hit testing.
      this.node_.hitTestWithReply(this.mouseX_, this.mouseY_, (target) => {
        this.handleHitTestResult(target);
        this.runHitTest();
      });
    }
    this.hasPendingEvents_ = false;
  }

  /**
   * Handles mouse move events.
   * @param {AutomationEvent} evt The mouse move event to process.
   */
  onMouseMove(evt) {
    this.onMove(evt.mouseX, evt.mouseY);
  }

  /**
   * Handles touch move events.
   * @param {number} x
   * @param {number} y
   */
  onTouchMove(x, y) {
    this.onMove(x, y, true);
  }

  /**
   * Inform this handler of a move to (x, y).
   * @param {number} x
   * @param {number} y
   * @param {boolean} isTouch
   */
  onMove(x, y, isTouch = false) {
    this.mouseX_ = x;
    this.mouseY_ = y;
    this.hasPendingEvents_ = true;
    this.runHitTest(isTouch);
  }

  /**
   * Synthesizes a mouse move on the current mouse location.
   */
  synthesizeMouseMove() {
    if (this.mouseX_ === undefined || this.mouseY_ === undefined) {
      return;
    }

    EventGenerator.sendMouseMove(
        this.mouseX_, this.mouseY_, true /* touchAccessibility */);
  }

  /**
   * Handles the result of a test test e.g. speaking the node.
   * @param {chrome.automation.AutomationNode} result
   */
  handleHitTestResult(result) {
    if (!result) {
      return;
    }

    let target = result;

    // If the target is in an ExoSurface, which hosts remote content, trigger a
    // mouse move. This only occurs when we programmatically hit test content
    // within ARC++ for now. Mouse moves automatically trigger Android to send
    // hover events back.
    if (target.role == RoleType.WINDOW &&
        target.className.indexOf('ExoSurface') == 0) {
      this.synthesizeMouseMove();
      return;
    }

    let targetLeaf = null;
    let targetObject = null;
    while (target && target != target.root) {
      if (!targetObject && AutomationPredicate.touchObject(target)) {
        targetObject = target;
      }
      if (AutomationPredicate.touchLeaf(target)) {
        targetLeaf = target;
      }
      target = target.parent;
    }

    target = targetLeaf || targetObject;
    if (!target) {
      if (ChromeVoxState.instance.currentRange) {
        this.lastValidNodeBeforePointerInvalidation_ =
            ChromeVoxState.instance.currentRange.start.node;
      }

      // This clears the anchor point in the TouchExplorationController (so when
      // a user touch explores back to the previous range, it will be announced
      // again).
      ChromeVoxState.instance.setCurrentRange(null);

      // Play a earcon to let the user know they're in the middle of nowhere.
      if ((new Date() - this.lastNoPointerAnchorEarconPlayedTime_) >
          PointerHandler.MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS) {
        ChromeVox.earcons.playEarcon(Earcon.NO_POINTER_ANCHOR);
        this.lastNoPointerAnchorEarconPlayedTime_ = new Date();
      }
      chrome.tts.stop();
      return;
    }

    if (ChromeVoxState.instance.currentRange &&
        target == ChromeVoxState.instance.currentRange.start.node) {
      return;
    }

    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    DesktopAutomationHandler.instance.onEventDefault(
        new CustomAutomationEvent(EventType.HOVER, target, '', []));
  }

  /**
   * @return {!AutomationNode|undefined} The last valid ChromeVox range start
   *     node prior to an explicit invalidation/clearing of the range triggered
   *     by a touch or mouse event on a "unpointable" target. That is, a target
   *     ChromeVox never wants to place its range upon.
   */
  get lastValidNodeBeforePointerInvalidation() {
    return this.lastValidNodeBeforePointerInvalidation_;
  }
};

/** @const {number} */
PointerHandler.MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS = 500;
