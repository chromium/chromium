// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox learn mode page.
 */

import {BrailleKeyEvent} from './braille/braille_key_types.js';
import {BridgeConstants} from './bridge_constants.js';
import {BridgeHelper} from './bridge_helper.js';

const TARGET = BridgeConstants.LearnMode.TARGET;
const Action = BridgeConstants.LearnMode.Action;

export class LearnModeBridge {
  /** @return {!Promise} */
  static clearTouchExploreOutputTime() {
    return BridgeHelper.sendMessage(
        TARGET, Action.CLEAR_TOUCH_EXPLORE_OUTPUT_TIME);
  }

  /**
   * @param {string} gesture
   * @return {!Promise}
   */
  static onAccessibilityGesture(gesture) {
    return BridgeHelper.sendMessage(
        TARGET, Action.ON_ACCESSIBILITY_GESTURE, gesture);
  }

  /**
   * @param {BrailleKeyEvent} event
   * @return {!Promise}
   */
  static onBrailleKeyEvent(event) {
    return BridgeHelper.sendMessage(TARGET, Action.ON_BRAILLE_KEY_EVENT, event);
  }

  /**
   * @param {Event} event
   * @return {!Promise}
   */
  static onKeyDown(event) {
    return BridgeHelper.sendMessage(TARGET, Action.ON_KEY_DOWN, event);
  }

  /**
   * @param {Event} event
   * @return {!Promise}
   */
  static onKeyUp(event) {
    return BridgeHelper.sendMessage(TARGET, Action.ON_KEY_UP, event);
  }
}
