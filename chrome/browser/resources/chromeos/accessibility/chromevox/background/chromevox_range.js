// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes that handle the ChromeVox range.
 */
import {AsyncUtil} from '../../common/async_util.js';
import {AutomationUtil} from '../../common/automation_util.js';
import {CursorRange} from '../../common/cursors/range.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';

import {ChromeVox} from './chromevox.js';
import {ChromeVoxState} from './chromevox_state.js';
import {FocusBounds} from './focus_bounds.js';

const RoleType = chrome.automation.RoleType;
const Action = BridgeConstants.ChromeVoxRange.Action;
const TARGET = BridgeConstants.ChromeVoxRange.TARGET;

/**
 * An interface implemented by objects to observe ChromeVox range changes.
 * @interface
 */
export class ChromeVoxRangeObserver {
  /**
   * @param {?CursorRange} range The new range.
   * @param {boolean=} opt_fromEditing
   */
  onCurrentRangeChanged(range, opt_fromEditing = undefined) {}
}

/**
 * A class that handles tracking and changes to the ChromeVox range.
 *
 * ================ THIS CLASS IS MID-MIGRATION ================
 *
 * The logic relating to the ChromeVox range is being moved here from
 * ChromeVoxState in small chunks. During this transition, the logic will be
 * split between those two locations.
 */
export class ChromeVoxRange {
  /** @private */
  constructor() {
    /** @private {?CursorRange} */
    this.current_ = null;
    /** @private {?CursorRange} */
    this.previous_ = null;
  }

  static init() {
    if (ChromeVoxRange.instance) {
      throw new Error('Cannot create more than one ChromeVoxRange');
    }
    ChromeVoxRange.instance = new ChromeVoxRange();

    BridgeHelper.registerHandler(
        TARGET, Action.CLEAR_CURRENT_RANGE, () => ChromeVoxRange.set(null));
  }

  /** @param {ChromeVoxRangeObserver} observer */
  static addObserver(observer) {
    ChromeVoxRange.observers_.push(observer);
  }

  /** @param {ChromeVoxRangeObserver} observer */
  static removeObserver(observer) {
    const index = ChromeVoxRange.observers_.indexOf(observer);
    if (index > -1) {
      ChromeVoxRange.observers_.splice(index, 1);
    }
  }

  /** @return {?CursorRange} */
  static getCurrentRangeWithoutRecovery() {
    return ChromeVoxRange.instance.current_;
  }

  /** @return {?CursorRange} */
  static get current() {
    if (ChromeVoxRange.instance.current_?.isValid()) {
      return ChromeVoxRange.instance.current_;
    }
    return null;
  }

  /** @return {?CursorRange} */
  static get previous() {
    return ChromeVoxRange.instance.previous_;
  }

  /**
   * @param {?CursorRange} newRange The new range.
   * @param {boolean=} opt_fromEditing
   */
  static set(newRange, opt_fromEditing) {
    ChromeVoxRange.instance.set_(...arguments);
  }

  /**
   * @param {?CursorRange} newRange
   * @param {boolean=} opt_fromEditing
   * @private
   */
  set_(newRange, opt_fromEditing) {
    // Clear anything that was frozen on the braille display whenever
    // the user navigates.
    ChromeVox.braille.thaw();

    // There's nothing to be updated in this case.
    if ((!newRange && !this.current_) || (newRange && !newRange.isValid())) {
      FocusBounds.set([]);
      return;
    }

    this.previous_ = this.current_;
    this.current_ = newRange;

    ChromeVoxState.ready().then(
        ChromeVoxRange.onCurrentRangeChanged(newRange, opt_fromEditing));

    if (!this.current_) {
      FocusBounds.set([]);
      return;
    }

    const start = this.current_.start.node;
    start.makeVisible();
    start.setAccessibilityFocus();

    const root = AutomationUtil.getTopLevelRoot(start);
    if (!root || root.role === RoleType.DESKTOP || root === start) {
      return;
    }

    const position = {};
    const loc = start.unclippedLocation;
    position.x = loc.left + loc.width / 2;
    position.y = loc.top + loc.height / 2;
    let url = root.docUrl;
    url = url.substring(0, url.indexOf('#')) || url;
    ChromeVoxState.position[url] = position;
  }

  /**
   * @param {?CursorRange} range The new range.
   * @param {boolean=} opt_fromEditing
   */
  static onCurrentRangeChanged(range, opt_fromEditing = undefined) {
    for (const observer of ChromeVoxRange.observers_) {
      observer.onCurrentRangeChanged(range, opt_fromEditing);
    }
  }

  /**
   * Check for loss of focus which results in us invalidating our current
   * range. Note the getFocus() callback is synchronous, so the focus will be
   * updated when this function returns (despite being technicallly a separate
   * function call). Note: do not convert this method to async, as it would
   * change the execution order described above.
   */
  static maybeResetFromFocus() {
    chrome.automation.getFocus(focus => {
      const cur = ChromeVoxRange.current;
      // If the current node is not valid and there's a current focus:
      if (cur && !cur.isValid() && focus) {
        ChromeVoxRange.set(CursorRange.fromNode(focus));
      }

      // If there's no focused node:
      if (!focus) {
        ChromeVoxRange.set(null);
        return;
      }

      // This case detects when TalkBack (in ARC++) is enabled (which also
      // covers when the ARC++ window is active). Clear the ChromeVox range
      // so keys get passed through for ChromeVox commands.
      if (ChromeVoxState.instance.talkBackEnabled &&
          // This additional check is not strictly necessary, but we use it to
          // ensure we are never inadvertently losing focus. ARC++ windows set
          // "focus" on a root view.
          focus.role === RoleType.CLIENT) {
        ChromeVoxRange.set(null);
      }
    });
  }
}

/** @private {!Array<ChromeVoxRangeObserver>} */
ChromeVoxRange.observers_ = [];
