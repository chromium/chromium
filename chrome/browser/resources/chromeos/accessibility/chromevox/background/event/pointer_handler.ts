// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox pointer handler. A pointer, in this context, is
 * either user touch or mouse input.
 */
import {AsyncUtil} from '/common/async_util.js';
import {AutomationPredicate} from '/common/automation_predicate.js';
import {EventGenerator} from '/common/event_generator.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {CustomAutomationEvent} from '../../common/custom_automation_event.js';
import {EarconId} from '../../common/earcon_id.js';
import {SettingsManager} from '../../common/settings_manager.js';
import {QueueMode} from '../../common/tts_types.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange} from '../chromevox_range.js';
import {Output} from '../output/output.js';

import {BaseAutomationHandler} from './base_automation_handler.js';
import {DesktopAutomationInterface} from './desktop_automation_interface.js';

type AutomationNode = chrome.automation.AutomationNode;
import AutomationEvent = chrome.automation.AutomationEvent;
import DeviceType = chrome.chromeosInfoPrivate.DeviceType;
import EventType = chrome.automation.EventType;
import RoleType = chrome.automation.RoleType;

async function getDeviceType(): Promise<DeviceType> {
  return new Promise(
      resolve => chrome.chromeosInfoPrivate.get(
          ['deviceType'], data => resolve(data.deviceType!)));
}

export class PointerHandler extends BaseAutomationHandler {
  private mouseX_: number|undefined;
  private mouseY_: number|undefined;
  private lastNoPointerAnchorEarconPlayedTime_: Date;
  private expectingHoverCount_: number;
  private isChromebox_: boolean;
  private lastHoverRequested_: Date;
  private ready_: boolean;
  private speakTextUnderMouse_: boolean = false;

  constructor() {
    super();

    this.lastNoPointerAnchorEarconPlayedTime_ = new Date();
    this.expectingHoverCount_ = 0;
    this.isChromebox_ = false;
    this.lastHoverRequested_ = new Date();
    this.ready_ = false;

    // Asynchronously initialize the listeners. Sets this.ready_ when done.
    this.initListeners_();
  }

  /**
   * Performs a hit test using the most recent mouse coordinates received in
   * onMouseMove or onMove (a e.g. for touch explore).
   */
  // TODO(b:314204374): use undefined rather than null.
  runHitTest(
      isTouch: boolean = false,
      specificNode: AutomationNode|null = null): void {
    if (this.mouseX_ === undefined || this.mouseY_ === undefined) {
      return;
    }

    if (isTouch && this.isChromebox_) {
      // TODO(accessibility): hit testing seems to be broken in some cases e.g.
      // on the main CFM UI. Synthesize mouse moves with the touch
      // accessibility flag for now for touch-based user gestures. Eliminate
      // this branch once hit testing is fixed.
      this.synthesizeMouseMove_();
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    const actOnNode = specificNode ? specificNode : this.node_!;
    actOnNode.hitTestWithReply(
        this.mouseX_, this.mouseY_, (target: AutomationNode) => {
          this.handleHitTestResult_(target);
        });
  }

  /**
   * Handles mouse move events.
   */
  onMouseMove(evt: AutomationEvent): void {
    this.onMove_(evt.mouseX, evt.mouseY);
  }

  /**
   * Handles touch move events.
   */
  onTouchMove(x: number, y: number): void {
    this.onMove_(x, y, true);
  }

  // =========== Private Methods =============

  private async initListeners_(): Promise<void> {
    this.node_ = await AsyncUtil.getDesktop();
    // @ts-ignore: BaseAutomationHandler needs to be converted to TS.
    this.addListener_(EventType.MOUSE_MOVED, this.onMouseMove_);

    // This is needed for ARC++ and Lacros. They send mouse move and hit test
    // respectively. Each responds with hover.
    // @ts-ignore: BaseAutomationHandler needs to be converted to TS.
    this.addListener_(EventType.HOVER, this.onHover_);

    this.mouseX_ = 0;
    this.mouseY_ = 0;

    if (SettingsManager.get('speakTextUnderMouse')) {
      chrome.accessibilityPrivate.enableMouseEvents(true);
      this.speakTextUnderMouse_ = true;
    }
    // Rather than disabling mouse events, instead we just do not
    // speak the text under the mouse when the setting is disabled by the
    // user. This has the benefit of not turning off mouse events for
    // other features that are using them, like magnifier and facegaze.
    SettingsManager.addListenerForKey(
        'speakTextUnderMouse',
        (newValue: boolean) => {
          if (newValue) {
            chrome.accessibilityPrivate.enableMouseEvents(true);
          }
          this.speakTextUnderMouse_ = newValue;
    });

    const deviceType = await getDeviceType();
    this.isChromebox_ = deviceType === DeviceType.CHROMEBOX;

    this.ready_ = true;
  }

  /**
   * Performs a hit test using the most recent mouse coordinates received in
   * onMouseMove or onMove (a e.g. for touch explore).
   */
  // TODO(b:314204374): use undefined rather than null.
  private runHitTest_(
      isTouch: boolean = false,
      specificNode: AutomationNode|null = null): void {
    if (!this.ready_ || !this.mouseX_ || !this.mouseY_) {
      return;
    }

    if (isTouch && this.isChromebox_) {
      // TODO(accessibility): hit testing seems to be broken in some cases e.g.
      // on the main CFM UI. Synthesize mouse moves with the touch
      // accessibility flag for now for touch-based user gestures. Eliminate
      // this branch once hit testing is fixed.
      this.synthesizeMouseMove_();
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    const actOnNode = specificNode ? specificNode : this.node_!;
    actOnNode.hitTestWithReply(
        this.mouseX_, this.mouseY_, (target: AutomationNode) => {
          this.handleHitTestResult_(target);
        });
  }

  /**
   * This is needed for ARC++ and Lacros. They send mouse move and hit test
   * respectively. Each responds with hover.
   */
  private onHover_(evt: AutomationEvent): void {
    if (this.expectingHoverCount_ === 0) {
      return;
    }

    // Stop honoring expectingHoverCount_ if it comes far after its
    // corresponding requested hit test.
    if (new Date().getTime() - this.lastHoverRequested_.getTime() > 500) {
      this.expectingHoverCount_ = 0;
    }

    this.expectingHoverCount_--;
    this.handleHitTestResult_(evt.target);
  }

  /**
   * Handles mouse move events.
   */
  private onMouseMove_(evt: AutomationEvent): void {
    if (!this.speakTextUnderMouse_) {
      return;
    }
    this.onMove_(evt.mouseX, evt.mouseY);
  }

  /**
   * Inform this handler of a move to (x, y).
   */
  private onMove_(x: number, y: number, isTouch: boolean = false): void {
    if (x === undefined || y === undefined) {
      return;
    }

    this.mouseX_ = x;
    this.mouseY_ = y;
    this.runHitTest_(isTouch);
  }

  /**
   * Synthesizes a mouse move on the current mouse location.
   */
  private synthesizeMouseMove_(): void {
    if (!this.ready_ || !this.mouseX_ || !this.mouseY_) {
      return;
    }

    this.expectingHoverCount_++;
    this.lastHoverRequested_ = new Date();
    EventGenerator.sendMouseMove(
        this.mouseX_, this.mouseY_, true /* touchAccessibility */);
  }

  /**
   * Handles the result of a test test e.g. speaking the node.
   */
  private handleHitTestResult_(result: AutomationNode): void {
    if (!result) {
      return;
    }

    let target: AutomationNode|undefined|null = result;

    // The target is in an ExoSurface, which hosts remote content.
    if (target.role === RoleType.WINDOW && target.className &&
        target.className.indexOf('ExoSurface') === 0) {
      // We're in ARC++, which still requires a synthesized mouse
      // event.
      this.synthesizeMouseMove_();
      return;
    }

    // TODO(b:314204374): Change null to undefined.
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
      ChromeVoxRange.set(null);

      // Play a earcon to let the user know they're in the middle of nowhere.
      if ((new Date().getTime() -
           this.lastNoPointerAnchorEarconPlayedTime_.getTime()) >
          PointerHandler.MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS) {
        ChromeVox.earcons.playEarcon(EarconId.NO_POINTER_ANCHOR);
        this.lastNoPointerAnchorEarconPlayedTime_ = new Date();
      }
      chrome.tts.stop();
      return;
    }

    if (ChromeVoxRange.current &&
        target === ChromeVoxRange.current.start.node) {
      return;
    }

    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    DesktopAutomationInterface.instance!.onEventDefault(
        // @ts-ignore: Need to convert base_automation_handler.js to Typescript.
        new CustomAutomationEvent(EventType.HOVER, target, {
          eventFromAction: chrome.automation.ActionType.HIT_TEST,
          eventFrom: undefined,
          intents: undefined,
        }));
  }
}

export namespace PointerHandler {
  export const MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS = 500;
}

TestImportManager.exportForTesting(PointerHandler);
