// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Data relating to the earcons used in ChromeVox.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

/** Earcon names. */
export enum EarconId {
  ALERT_MODAL = 'alert_modal',
  ALERT_NONMODAL = 'alert_nonmodal',
  BUTTON = 'button',
  CHECK_OFF = 'check_off',
  CHECK_ON = 'check_on',
  CHROMEVOX_LOADING = 'chromevox_loading',
  CHROMEVOX_LOADED = 'chromevox_loaded',
  EDITABLE_TEXT = 'editable_text',
  INVALID_KEYPRESS = 'invalid_keypress',
  LINK = 'link',
  LISTBOX = 'listbox',
  LIST_ITEM = 'list_item',
  LONG_DESC = 'long_desc',
  MATH = 'math',
  NO_POINTER_ANCHOR = 'no_pointer_anchor',
  OBJECT_CLOSE = 'object_close',
  OBJECT_ENTER = 'object_enter',
  OBJECT_EXIT = 'object_exit',
  OBJECT_OPEN = 'object_open',
  OBJECT_SELECT = 'object_select',
  PAGE_FINISH_LOADING = 'page_finish_loading',
  PAGE_START_LOADING = 'page_start_loading',
  POP_UP_BUTTON = 'pop_up_button',
  RECOVER_FOCUS = 'recover_focus',
  SELECTION = 'selection',
  SELECTION_REVERSE = 'selection_reverse',
  SKIP = 'skip',
  SLIDER = 'slider',
  SMART_STICKY_MODE_OFF = 'smart_sticky_mode_off',
  SMART_STICKY_MODE_ON = 'smart_sticky_mode_on',
  WRAP = 'wrap',
  WRAP_EDGE = 'wrap_edge',
}

export namespace EarconId {
  export function fromName(name: string): EarconId {
    return (EarconId as {[key: string]: any})[name];
  }
}

/**
 * Maps a earcon id to a message id description.
 * Only add mappings for earcons used in ChromeVox Next. This map gets
 * used to generate tutorial content.
 */
export const EarconDescription: Partial<Record<EarconId, string>> = {
  [EarconId.ALERT_MODAL]: 'alert_modal_earcon_description',
  [EarconId.ALERT_NONMODAL]: 'alert_nonmodal_earcon_description',
  [EarconId.BUTTON]: 'button_earcon_description',
  [EarconId.CHECK_OFF]: 'check_off_earcon_description',
  [EarconId.CHECK_ON]: 'check_on_earcon_description',
  [EarconId.CHROMEVOX_LOADING]: 'chromevox_loading_earcon_description',
  [EarconId.EDITABLE_TEXT]: 'editable_text_earcon_description',
  [EarconId.INVALID_KEYPRESS]: 'invalid_keypress_earcon_description',
  [EarconId.LINK]: 'link_earcon_description',
  [EarconId.LISTBOX]: 'listbox_earcon_description',
  [EarconId.NO_POINTER_ANCHOR]: 'no_pointer_anchor_earcon_description',
  [EarconId.PAGE_START_LOADING]: 'page_start_loading_earcon_description',
  [EarconId.POP_UP_BUTTON]: 'pop_up_button_earcon_description',
  [EarconId.SLIDER]: 'slider_earcon_description',
  [EarconId.SMART_STICKY_MODE_OFF]: 'smart_sticky_mode_off_earcon_description',
  [EarconId.SMART_STICKY_MODE_ON]: 'smart_sticky_mode_on_earcon_description',
  [EarconId.WRAP]: 'wrap_earcon_description',
};

TestImportManager.exportForTesting(['EarconId', EarconId]);
