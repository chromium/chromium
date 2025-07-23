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
  // We need to call this method before sending a message to the learn mode
  // page, otherwise, we will encounter errors like "Unchecked
  // runtime.lastError: The message port closed before a response was received".
  private static async isLearnModeReady_(): Promise<boolean> {
    if (chrome.runtime && chrome.runtime.getContexts) {
      // If the calling context has access to the necessary API, then use it
      // directly.
      const learnModeUrl =
          chrome.runtime.getURL('chromevox/mv3/learn_mode/learn_mode.html');
      const existingContexts = await chrome.runtime.getContexts({
        documentUrls: [learnModeUrl],
      });

      return existingContexts.length > 0;
    }

    // Otherwise, ask the service worker if learn mode is ready.
    return BridgeHelper.sendMessage(
        BridgeConstants.ChromeVoxState.TARGET,
        BridgeConstants.ChromeVoxState.Action.IS_LEARN_MODE_READY);
  }

  static async onKeyDown(internalEvent: InternalKeyEvent): Promise<boolean> {
    const ready = await LearnModeBridge.isLearnModeReady_();
    if (!ready) {
      return false;
    }

    return BridgeHelper.sendMessage(
        LearnModeTarget, LearnModeAction.ON_KEY_DOWN, internalEvent);
  }

  static async onKeyUp(): Promise<void> {
    const ready = await LearnModeBridge.isLearnModeReady_();
    if (!ready) {
      return;
    }

    return BridgeHelper.sendMessage(LearnModeTarget, LearnModeAction.ON_KEY_UP);
  }

  static async onKeyPress(): Promise<void> {
    const ready = await LearnModeBridge.isLearnModeReady_();
    if (!ready) {
      return;
    }

    return BridgeHelper.sendMessage(
        LearnModeTarget, LearnModeAction.ON_KEY_PRESS);
  }

  static async clearTouchExploreOutputTimeForTest(): Promise<void> {
    const ready = await LearnModeBridge.isLearnModeReady_();
    if (!ready) {
      return;
    }

    return BridgeHelper.sendMessage(
        LearnModeTestTarget,
        LearnModeTestAction.CLEAR_TOUCH_EXPLORE_OUTPUT_TIME);
  }

  static async onAccessibilityGestureForTest(gesture: string): Promise<void> {
    const ready = await LearnModeBridge.isLearnModeReady_();
    if (!ready) {
      return;
    }

    return BridgeHelper.sendMessage(
        LearnModeTestTarget, LearnModeTestAction.ON_ACCESSIBILITY_GESTURE,
        gesture);
  }

  static async onBrailleKeyEventForTest(event: BrailleKeyEvent): Promise<void> {
    const ready = await LearnModeBridge.isLearnModeReady_();
    if (!ready) {
      return;
    }

    return BridgeHelper.sendMessage(
        LearnModeTestTarget, LearnModeTestAction.ON_BRAILLE_KEY_EVENT, event);
  }
}

TestImportManager.exportForTesting(LearnModeBridge);
