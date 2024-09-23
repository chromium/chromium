// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox learn mode page.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BrailleKeyEvent} from './braille/braille_key_types.js';
import {BridgeConstants} from './bridge_constants.js';

const TARGET = BridgeConstants.LearnMode.TARGET;
const Action = BridgeConstants.LearnMode.Action;

export class LearnModeBridge {
  static clearTouchExploreOutputTime(): Promise<void> {
    return BridgeHelper.sendMessage(
        TARGET, Action.CLEAR_TOUCH_EXPLORE_OUTPUT_TIME);
  }

  static onAccessibilityGesture(gesture: string): Promise<void> {
    return BridgeHelper.sendMessage(
        TARGET, Action.ON_ACCESSIBILITY_GESTURE, gesture);
  }

  static onBrailleKeyEvent(event: BrailleKeyEvent): Promise<void> {
    return BridgeHelper.sendMessage(TARGET, Action.ON_BRAILLE_KEY_EVENT, event);
  }

  static onKeyDown(event: KeyboardEvent): Promise<void> {
    return BridgeHelper.sendMessage(TARGET, Action.ON_KEY_DOWN, event);
  }

  static onKeyUp(event: KeyboardEvent): Promise<void> {
    return BridgeHelper.sendMessage(TARGET, Action.ON_KEY_UP, event);
  }

  static ready(): Promise<void> {
    return BridgeHelper.sendMessage(TARGET, Action.READY);
  }
}

TestImportManager.exportForTesting(LearnModeBridge);
