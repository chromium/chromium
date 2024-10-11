// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  static getDisplayText(gesture: FacialGesture, macroName: MacroName): string {
    // TODO(b:341770655): Localize this string.
    return `${BubbleController.getDisplayTextForMacro_(macroName)} (${
        BubbleController.getDisplayTextForGesture_(gesture)})`;
  }

  private static getDisplayTextForMacro_(macroName: MacroName): string {
    switch (macroName) {
      case MacroName.CUSTOM_KEY_COMBINATION:
        return chrome.i18n.getMessage('facegaze_macro_text_custom_key_combo');
      case MacroName.KEY_PRESS_DOWN:
        return chrome.i18n.getMessage('facegaze_macro_text_key_press_down');
      case MacroName.KEY_PRESS_LEFT:
        return chrome.i18n.getMessage('facegaze_macro_text_key_press_left');
      case MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE:
        return chrome.i18n.getMessage('facegaze_macro_text_media_play_pause');
      case MacroName.KEY_PRESS_RIGHT:
        return chrome.i18n.getMessage('facegaze_macro_text_key_press_right');
      case MacroName.KEY_PRESS_SPACE:
        return chrome.i18n.getMessage('facegaze_macro_text_key_press_space');
      case MacroName.KEY_PRESS_TOGGLE_OVERVIEW:
        return chrome.i18n.getMessage('facegaze_macro_text_toggle_overview');
      case MacroName.KEY_PRESS_UP:
        return chrome.i18n.getMessage('facegaze_macro_text_key_press_up');
      case MacroName.MOUSE_CLICK_LEFT:
        return chrome.i18n.getMessage('facegaze_macro_text_mouse_click_left');
      case MacroName.MOUSE_CLICK_LEFT_DOUBLE:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_mouse_click_left_double');
      case MacroName.MOUSE_CLICK_RIGHT:
        return chrome.i18n.getMessage('facegaze_macro_text_mouse_click_right');
      case MacroName.MOUSE_LONG_CLICK_LEFT:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_mouse_long_click_left');
      case MacroName.RESET_CURSOR:
        return chrome.i18n.getMessage('facegaze_macro_text_reset_cursor');
      case MacroName.TOGGLE_DICTATION:
        return chrome.i18n.getMessage('facegaze_macro_text_toggle_dictation');
      case MacroName.TOGGLE_FACEGAZE:
        return chrome.i18n.getMessage('facegaze_macro_text_toggle_facegaze');
      case MacroName.TOGGLE_SCROLL_MODE:
        return chrome.i18n.getMessage('facegaze_macro_text_toggle_scroll_mode');
      case MacroName.TOGGLE_VIRTUAL_KEYBOARD:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_toggle_virtual_keyboard');
      default:
        console.error(
            'Display text requested for unsupported macro ' + macroName);
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
  export const RESET_BUBBLE_TIMEOUT_MS = 2500;

  export interface GetStateResult {
    paused: boolean;
    scrollModeActive: boolean;
  }
}

TestImportManager.exportForTesting(BubbleController);
