// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox pointer handler. A pointer, in this context, is
 * either user touch or mouse input.
 */
import {AutomationPredicate} from '../../common/automation_predicate.js';
import {EventGenerator} from '../../common/event_generator.js';
import {LocalStorage} from '../../common/local_storage.js';
import {Earcon} from '../common/abstract_earcons.js';
import {CustomAutomationEvent} from '../common/custom_automation_event.js';
import {QueueMode} from '../common/tts_types.js';

import {BaseAutomationHandler} from './base_automation_handler.js';
import {ChromeVox} from './chromevox.js';
import {ChromeVoxState} from './chromevox_state.js';
import {DesktopAutomationInterface} from './desktop_automation_interface.js';
import {Output} from './output/output.js';

const AutomationNode = chrome.automation.AutomationNode;
const AutomationEvent = chrome.automation.AutomationEvent;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;

export class PointerHandler extends BaseAutomationHandler {
  constructor() {
    super(null);

    /** @private {number|undefined} */
    this.mouseX_;
    /** @private {number|undefined} */
    this.mouseY_;
    /** @private {!Date} */
    this.lastNoPointerAnchorEarconPlayedTime_ = new Date();
    /** @private {number} */
    this.expectingHoverCount_ = 0;
    /** @private {boolean} */
    this.isChromebox_ = false;
    /** @private {!Date} */
    this.lastHoverRequested_ = new Date();

    chrome.automation.getDesktop(desktop => {
      this.node_ = desktop;
      this.addListener_(EventType.MOUSE_MOVED, this.onMouseMove);

      // This is needed for ARC++ and Lacros. They send mouse move and hit test
      // respectively. Each responds with hover.
      this.addListener_(EventType.HOVER, evt => {
        if (this.expectingHoverCount_ === 0) {
          return;
        }

        // Stop honoring expectingHoverCount_ if it comes far after its
        // corresponding requested hit test.
        if (new Date() - this.lastHoverRequested_ > 500) {
          this.expectingHoverCount_ = 0;
        }

        this.expectingHoverCount_--;
        this.handleHitTestResult(evt.target);
      });

      this.mouseX_ = 0;
      this.mouseY_ = 0;
    });

    if (LocalStorage.get('speakTextUnderMouse')) {
      chrome.accessibilityPrivate.enableMouseEvents(true);
    }

    chrome.chromeosInfoPrivate.get(['deviceType'], result => {
      this.isChromebox_ = result['deviceType'] ===
          chrome.chromeosInfoPrivate.DeviceType.CHROMEBOX;
    });
  }

  /**
   * Performs a hit test using the most recent mouse coordinates received in
   * onMouseMove or onMove (a e.g. for touch explore).
   * @param {boolean} isTouch
   * @param {AutomationNode} specificNode
   */
  runHitTest(isTouch = false, specificNode = null) {
    if (this.mouseX_ === undefined || this.mouseY_ === undefined) {
      return;
    }

    if (isTouch && this.isChromebox_) {
      // TODO(accessibility): hit testing seems to be broken in some cases e.g.
      // on the main CFM UI. Synthesize mouse moves with the touch
      // accessibility flag for now for touch-based user gestures. Eliminate
      // this branch once hit testing is fixed.
      this.synthesizeMouseMove();
      return;
    }

    const actOnNode = specificNode ? specificNode : this.node_;
    actOnNode.hitTestWithReply(this.mouseX_, this.mouseY_, target => {
      this.handleHitTestResult(target);
    });
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
    this.runHitTest(isTouch);
  }

  /**
   * Synthesizes a mouse move on the current mouse location.
   */
  synthesizeMouseMove() {
    if (this.mouseX_ === undefined || this.mouseY_ === undefined) {
      return;
    }

    this.expectingHoverCount_++;
    this.lastHoverRequested_ = new Date();
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

    // The target is in an ExoSurface, which hosts remote content.
    if (target.role === RoleType.WINDOW &&
        target.className.indexOf('ExoSurface') === 0) {
      // We're in ARC++, which still requires a synthesized mouse
      // event.
      this.synthesizeMouseMove();
      return;
    }

    let targetLeaf = null;
    let targetObject = null;
    while (target && target !== target.root) {
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
        target === ChromeVoxState.instance.currentRange.start.node) {
      return;
    }

    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    DesktopAutomationInterface.instance.onEventDefault(
        new CustomAutomationEvent(
            EventType.HOVER, target,
            {eventFromAction: chrome.automation.ActionType.HIT_TEST}));
  }
}

/** @const {number} */
PointerHandler.MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS = 500;
