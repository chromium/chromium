// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyPressMacro} from '/common/action_fulfillment/macros/key_press_macro.js';
import {Macro, ToggleDirection} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {FacialGesture} from './facial_gestures.js';

/** Handles setting the text content of the FaceGaze bubble UI. */
export class BubbleController {
  private resetBubbleTimeoutId_: number|undefined;
  private baseText_: string[] = [];
  private getStateGesture_: () => BubbleController.GetStateGestureResult;

  constructor(getStateGesture: () => BubbleController.GetStateGestureResult) {
    this.getStateGesture_ = getStateGesture;
  }

  updateBubble(text: string): void {
    chrome.accessibilityPrivate.updateFaceGazeBubble(
        text, /*isWarning=*/ false);
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
    const {
      paused,
      scrollMode,
      longClick,
      dictation,
      heldActions,
    } = this.getStateGesture_();

    if (heldActions) {
      heldActions.forEach((displayText) => {this.baseText_.push(displayText)});
    }

    if (paused) {
      this.baseText_.push(chrome.i18n.getMessage(
          'facegaze_state_paused',
          BubbleController.getDisplayTextForGesture_(paused)));
    } else if (scrollMode) {
      this.baseText_.push(chrome.i18n.getMessage(
          'facegaze_state_scroll_active',
          BubbleController.getDisplayTextForGesture_(scrollMode)));
    } else if (longClick) {
      this.baseText_.push(chrome.i18n.getMessage(
          'facegaze_state_long_click_active',
          BubbleController.getDisplayTextForGesture_(longClick)));
    } else if (dictation) {
      this.baseText_.push(chrome.i18n.getMessage(
          'facegaze_state_dictation_active',
          BubbleController.getDisplayTextForGesture_(dictation)));
    }

    chrome.accessibilityPrivate.updateFaceGazeBubble(
        this.baseText_.join(', '), /*isWarning=*/ this.baseText_.length > 0);
  }

  static getDisplayText(gesture: FacialGesture, macro: Macro): string {
    return chrome.i18n.getMessage('facegaze_display_text', [
      BubbleController.getDisplayTextForMacro_(macro),
      BubbleController.getDisplayTextForGesture_(gesture)
    ]);
  }

  private static getDisplayTextForMacro_(macro: Macro): string {
    const macroName = macro.getName();
    switch (macroName) {
      case MacroName.CUSTOM_KEY_COMBINATION:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_custom_key_combo',
            this.getDisplayTextForKeyCombo_(macro as KeyPressMacro));
      case MacroName.KEY_PRESS_DOWN:
        return chrome.i18n.getMessage('facegaze_macro_text_key_press_down');
      case MacroName.KEY_PRESS_LEFT:
        return chrome.i18n.getMessage('facegaze_macro_text_key_press_left');
      case MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE:
        return chrome.i18n.getMessage('facegaze_macro_text_media_play_pause');
      case MacroName.KEY_PRESS_RIGHT:
        return chrome.i18n.getMessage('facegaze_macro_text_key_press_right');
      case MacroName.KEY_PRESS_SCREENSHOT:
        return chrome.i18n.getMessage('facegaze_macro_text_screenshot');
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
      case MacroName.MOUSE_CLICK_LEFT_TRIPLE:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_mouse_click_left_triple');
      case MacroName.MOUSE_CLICK_RIGHT:
        return chrome.i18n.getMessage('facegaze_macro_text_mouse_click_right');
      case MacroName.MOUSE_LONG_CLICK_LEFT:
        return macro.getToggleDirection() === ToggleDirection.ON ?
            chrome.i18n.getMessage(
                'facegaze_macro_text_mouse_long_click_left_on') :
            chrome.i18n.getMessage(
                'facegaze_macro_text_mouse_long_click_left_off');
      case MacroName.RESET_CURSOR:
        return chrome.i18n.getMessage('facegaze_macro_text_reset_cursor');
      case MacroName.TOGGLE_DICTATION:
        return macro.getToggleDirection() === ToggleDirection.ON ?
            chrome.i18n.getMessage('facegaze_macro_text_toggle_dictation_on') :
            chrome.i18n.getMessage('facegaze_macro_text_toggle_dictation_off');
      case MacroName.TOGGLE_FACEGAZE:
        return macro.getToggleDirection() === ToggleDirection.ON ?
            chrome.i18n.getMessage('facegaze_macro_text_toggle_facegaze_on') :
            chrome.i18n.getMessage('facegaze_macro_text_toggle_facegaze_off');
      case MacroName.TOGGLE_SCROLL_MODE:
        return macro.getToggleDirection() === ToggleDirection.ON ?
            chrome.i18n.getMessage(
                'facegaze_macro_text_toggle_scroll_mode_on') :
            chrome.i18n.getMessage(
                'facegaze_macro_text_toggle_scroll_mode_off');
      case MacroName.TOGGLE_VIRTUAL_KEYBOARD:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_toggle_virtual_keyboard');
      default:
        console.error(
            `Display text requested for unsupported macro ${macroName}`);
        return '';
    }
  }

  private static getDisplayTextForKeyCombo_(macro: KeyPressMacro): string {
    const keyCombo = macro.getKeyCombination();

    // Pre-defined key press macros, like for MEDIA_PLAY_PAUSE and SNAPSHOT,
    // should not request display text for their key combinations.
    if (!keyCombo || !keyCombo.keyDisplay) {
      console.error(
          `Key combo text requested for unsupported macro ${macro.getName()}`);
      return '';
    }

    const keys: string[] = [];

    if (keyCombo.modifiers?.ctrl) {
      keys.push(chrome.i18n.getMessage('facegaze_macro_text_key_ctrl'));
    }
    if (keyCombo.modifiers?.alt) {
      keys.push(chrome.i18n.getMessage('facegaze_macro_text_key_alt'));
    }
    if (keyCombo.modifiers?.shift) {
      keys.push(chrome.i18n.getMessage('facegaze_macro_text_key_shift'));
    }
    if (keyCombo.modifiers?.search) {
      keys.push(chrome.i18n.getMessage('facegaze_macro_text_key_search'));
    }

    keys.push(keyCombo.keyDisplay);

    switch (keys.length) {
      case 2:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_keyboard_combo_one_modifier', keys);
      case 3:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_keyboard_combo_two_modifiers', keys);
      case 4:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_keyboard_combo_three_modifiers', keys);
      case 5:
        return chrome.i18n.getMessage(
            'facegaze_macro_text_keyboard_combo_four_modifiers', keys);
      default:
        // keyDisplay comes directly from the original KeyEvent and should be
        // preserved as-is since keys may appear differently on keyboards
        // depending on locale and layout.
        return keyCombo.keyDisplay;
    }
  }

  private static getDisplayTextForGesture_(gesture: FacialGesture): string {
    switch (gesture) {
      case FacialGesture.BROW_INNER_UP:
        return chrome.i18n.getMessage('facegaze_gesture_text_brow_inner_up');
      case FacialGesture.BROWS_DOWN:
        return chrome.i18n.getMessage('facegaze_gesture_text_brows_down');
      case FacialGesture.EYE_SQUINT_LEFT:
        return chrome.i18n.getMessage('facegaze_gesture_text_eye_squint_left');
      case FacialGesture.EYE_SQUINT_RIGHT:
        return chrome.i18n.getMessage('facegaze_gesture_text_eye_squint_right');
      case FacialGesture.EYES_BLINK:
        return chrome.i18n.getMessage('facegaze_gesture_text_eyes_blink');
      case FacialGesture.EYES_LOOK_DOWN:
        return chrome.i18n.getMessage('facegaze_gesture_text_eyes_look_down');
      case FacialGesture.EYES_LOOK_LEFT:
        return chrome.i18n.getMessage('facegaze_gesture_text_eyes_look_left');
      case FacialGesture.EYES_LOOK_RIGHT:
        return chrome.i18n.getMessage('facegaze_gesture_text_eyes_look_right');
      case FacialGesture.EYES_LOOK_UP:
        return chrome.i18n.getMessage('facegaze_gesture_text_eyes_look_up');
      case FacialGesture.JAW_LEFT:
        return chrome.i18n.getMessage('facegaze_gesture_text_jaw_left');
      case FacialGesture.JAW_OPEN:
        return chrome.i18n.getMessage('facegaze_gesture_text_jaw_open');
      case FacialGesture.JAW_RIGHT:
        return chrome.i18n.getMessage('facegaze_gesture_text_jaw_right');
      case FacialGesture.MOUTH_FUNNEL:
        return chrome.i18n.getMessage('facegaze_gesture_text_mouth_funnel');
      case FacialGesture.MOUTH_LEFT:
        return chrome.i18n.getMessage('facegaze_gesture_text_mouth_left');
      case FacialGesture.MOUTH_PUCKER:
        return chrome.i18n.getMessage('facegaze_gesture_text_mouth_pucker');
      case FacialGesture.MOUTH_RIGHT:
        return chrome.i18n.getMessage('facegaze_gesture_text_mouth_right');
      case FacialGesture.MOUTH_SMILE:
        return chrome.i18n.getMessage('facegaze_gesture_text_mouth_smile');
      case FacialGesture.MOUTH_UPPER_UP:
        return chrome.i18n.getMessage('facegaze_gesture_text_mouth_upper_up');
      default:
        console.error(
            'Display text requested for unsupported FacialGesture ' + gesture);
        return '';
    }
  }
}

export namespace BubbleController {
  export const RESET_BUBBLE_TIMEOUT_MS = 2500;

  export interface GetStateGestureResult {
    paused: FacialGesture|undefined;
    scrollMode: FacialGesture|undefined;
    longClick: FacialGesture|undefined;
    dictation: FacialGesture|undefined;
    heldActions: string[];
  }
}

TestImportManager.exportForTesting(BubbleController);
