// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {FacialGesture} from './facial_gestures.js';

/** Handles setting the text content of the FaceGaze bubble UI. */
export class BubbleController {
  private resetBubbleTimeoutId_: number|undefined;
  private baseText_: string[] = [];
  private getState_: () => BubbleController.GetStateResult;

  constructor(getState: () => BubbleController.GetStateResult) {
    this.getState_ = getState;
  }

  updateBubble(text: string): void {
    chrome.accessibilityPrivate.updateFaceGazeBubble(text);
    this.setResetBubbleTimeout_();
  }

  private setResetBubbleTimeout_(): void {
    this.clearTimeout_();
    this.resetBubbleTimeoutId_ = setTimeout(
        () => this.resetBubble(), BubbleController.RESET_BUBBLE_TIMEOUT_MS);
  }

  private clearTimeout_(): void {
    if (this.resetBubbleTimeoutId_ !== undefined) {
      clearTimeout(this.resetBubbleTimeoutId_);
      this.resetBubbleTimeoutId_ = undefined;
    }
  }

  resetBubble(): void {
    this.baseText_ = [];
    const {paused, scrollModeActive} = this.getState_();
    // TODO(b/341770655): Localize these strings.
    if (paused) {
      this.baseText_.push('FaceGaze paused');
    }
    if (scrollModeActive) {
      this.baseText_.push('Scroll mode active');
    }

    chrome.accessibilityPrivate.updateFaceGazeBubble(this.baseText_.join(', '));
  }

  static getDisplayText(gesture: FacialGesture, macro: Macro): string {
    // TODO(b:341770655): Localize this string.
    return `${BubbleController.getDisplayTextForMacro_(macro)} (${
        BubbleController.getDisplayTextForGesture_(gesture)})`;
  }

  private static getDisplayTextForMacro_(macro: Macro): string {
    // TODO(b:341770655): Localize these strings.
    switch (macro.getName()) {
      case MacroName.CUSTOM_KEY_COMBINATION:
        return 'Perform a custom key combination';
      case MacroName.KEY_PRESS_DOWN:
        return 'Press the down arrow key';
      case MacroName.KEY_PRESS_LEFT:
        return 'Press the left arrow key';
      case MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE:
        return 'Play or pause media';
      case MacroName.KEY_PRESS_RIGHT:
        return 'Press the right arrow key';
      case MacroName.KEY_PRESS_SPACE:
        return 'Press the space key';
      case MacroName.KEY_PRESS_TOGGLE_OVERVIEW:
        return 'Open overview of windows';
      case MacroName.KEY_PRESS_UP:
        return 'Press the up arrow key';
      case MacroName.MOUSE_CLICK_LEFT:
        return 'Left-click the mouse';
      case MacroName.MOUSE_CLICK_LEFT_DOUBLE:
        return 'Double click the mouse';
      case MacroName.MOUSE_CLICK_RIGHT:
        return 'Right-click the mouse';
      case MacroName.MOUSE_LONG_CLICK_LEFT:
        return 'Drag and drop';
      case MacroName.RESET_CURSOR:
        return 'Reset cursor to center';
      case MacroName.TOGGLE_DICTATION:
        return 'Start or stop dictation';
      case MacroName.TOGGLE_FACEGAZE:
        return 'Pause or resume face control';
      case MacroName.TOGGLE_SCROLL_MODE:
        return 'Toggle scroll mode';
      case MacroName.TOGGLE_VIRTUAL_KEYBOARD:
        return 'Show or hide the virtual keyboard';
      default:
        console.error(
            'Display text requested for unsupported macro ' + macro.getName());
        return '';
    }
  }

  private static getDisplayTextForGesture_(gesture: FacialGesture): string {
    // TODO(b:341770655): Localize these strings.
    switch (gesture) {
      case FacialGesture.BROW_INNER_UP:
        return 'Raise eyebrows';
      case FacialGesture.BROWS_DOWN:
        return 'Lower eyebrows';
      case FacialGesture.EYE_SQUINT_LEFT:
        return 'Squint left eye';
      case FacialGesture.EYE_SQUINT_RIGHT:
        return 'Squint right eye';
      case FacialGesture.EYES_BLINK:
        return 'Blink both eyes';
      case FacialGesture.EYES_LOOK_DOWN:
        return 'Look down';
      case FacialGesture.EYES_LOOK_LEFT:
        return 'Look left';
      case FacialGesture.EYES_LOOK_RIGHT:
        return 'Look right';
      case FacialGesture.EYES_LOOK_UP:
        return 'Look up';
      case FacialGesture.JAW_LEFT:
        return 'Shift jaw left';
      case FacialGesture.JAW_OPEN:
        return 'Open your mouth wide';
      case FacialGesture.JAW_RIGHT:
        return 'Shift jaw right';
      case FacialGesture.MOUTH_FUNNEL:
        return 'Make a circle with your lips';
      case FacialGesture.MOUTH_LEFT:
        return 'Stretch left corner of your mouth';
      case FacialGesture.MOUTH_PUCKER:
        return 'Pucker by squeezing lips together';
      case FacialGesture.MOUTH_RIGHT:
        return 'Stretch right corner of your mouth';
      case FacialGesture.MOUTH_SMILE:
        return 'Smile';
      case FacialGesture.MOUTH_UPPER_UP:
        return 'Wrinkle your nose';
      default:
        console.error(
            'Display text requested for unsupported FacialGesture ' + gesture);
        return '';
    }
  }
}

export namespace BubbleController {
  export const RESET_BUBBLE_TIMEOUT_MS = 5000;

  export interface GetStateResult {
    paused: boolean;
    scrollModeActive: boolean;
  }
}

TestImportManager.exportForTesting(BubbleController);
