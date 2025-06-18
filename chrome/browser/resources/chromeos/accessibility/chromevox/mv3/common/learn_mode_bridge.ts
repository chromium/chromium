// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox learn mode page.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import type {BrailleKeyEvent} from './braille/braille_key_types.js';
import {BridgeConstants} from './bridge_constants.js';
import type {InternalKeyEvent} from './internal_key_event.js'

const LearnModeTarget = BridgeConstants.LearnMode.TARGET;
const LearnModeAction = BridgeConstants.LearnMode.Action;

const LearnModeTestTarget = BridgeConstants.LearnModeTest.TARGET;
const LearnModeTestAction = BridgeConstants.LearnModeTest.Action;

export class LearnModeBridge {
  static onKeyDown(internalEvent: InternalKeyEvent): Promise<boolean> {
    return BridgeHelper.sendMessage(
        LearnModeTarget, LearnModeAction.ON_KEY_DOWN, internalEvent);
  }

  static onKeyUp(): Promise<void> {
    return BridgeHelper.sendMessage(LearnModeTarget, LearnModeAction.ON_KEY_UP);
  }

  static onKeyPress(): Promise<void> {
    return BridgeHelper.sendMessage(
        LearnModeTarget, LearnModeAction.ON_KEY_PRESS);
  }

  static clearTouchExploreOutputTimeForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        LearnModeTestTarget,
        LearnModeTestAction.CLEAR_TOUCH_EXPLORE_OUTPUT_TIME);
  }

  static onAccessibilityGestureForTest(gesture: string): Promise<void> {
    return BridgeHelper.sendMessage(
        LearnModeTestTarget, LearnModeTestAction.ON_ACCESSIBILITY_GESTURE,
        gesture);
  }

  static onBrailleKeyEventForTest(event: BrailleKeyEvent): Promise<void> {
    return BridgeHelper.sendMessage(
        LearnModeTestTarget, LearnModeTestAction.ON_BRAILLE_KEY_EVENT, event);
  }

  static readyForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        LearnModeTestTarget, LearnModeTestAction.READY);
  }
}

TestImportManager.exportForTesting(LearnModeBridge);
