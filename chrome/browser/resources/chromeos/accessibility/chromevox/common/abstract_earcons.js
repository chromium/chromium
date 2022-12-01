// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for implementing earcons.
 *
 * When adding earcons, please add them to getEarconName and getEarconId.
 *
 */
import {LocalStorage} from '../../common/local_storage.js';

/**
 * Earcon names.
 * @enum {string}
 */
export const Earcon = {
  ALERT_MODAL: 'alert_modal',
  ALERT_NONMODAL: 'alert_nonmodal',
  BUTTON: 'button',
  CHECK_OFF: 'check_off',
  CHECK_ON: 'check_on',
  CHROMEVOX_LOADING: 'chromevox_loading',
  CHROMEVOX_LOADED: 'chromevox_loaded',
  EDITABLE_TEXT: 'editable_text',
  INVALID_KEYPRESS: 'invalid_keypress',
  LINK: 'link',
  LISTBOX: 'listbox',
  LIST_ITEM: 'list_item',
  LONG_DESC: 'long_desc',
  MATH: 'math',
  OBJECT_CLOSE: 'object_close',
  OBJECT_ENTER: 'object_enter',
  OBJECT_EXIT: 'object_exit',
  OBJECT_OPEN: 'object_open',
  OBJECT_SELECT: 'object_select',
  PAGE_FINISH_LOADING: 'page_finish_loading',
  PAGE_START_LOADING: 'page_start_loading',
  POP_UP_BUTTON: 'pop_up_button',
  RECOVER_FOCUS: 'recover_focus',
  SELECTION: 'selection',
  SELECTION_REVERSE: 'selection_reverse',
  SKIP: 'skip',
  SLIDER: 'slider',
  SMART_STICKY_MODE_OFF: 'smart_sticky_mode_off',
  SMART_STICKY_MODE_ON: 'smart_sticky_mode_on',
  NO_POINTER_ANCHOR: 'no_pointer_anchor',
  WRAP: 'wrap',
  WRAP_EDGE: 'wrap_edge',
};

/**
 * Maps a earcon id to a message id description.
 * Only add mappings for earcons used in ChromeVox Next. This map gets
 * used to generate tutorial content.
 * @type {Object<string, string>}
 */
export const EarconDescription = {
  alert_modal: 'alert_modal_earcon_description',
  alert_nonmodal: 'alert_nonmodal_earcon_description',
  button: 'button_earcon_description',
  check_off: 'check_off_earcon_description',
  check_on: 'check_on_earcon_description',
  editable_text: 'editable_text_earcon_description',
  invalid_keypress: 'invalid_keypress_earcon_description',
  link: 'link_earcon_description',
  listbox: 'listbox_earcon_description',
  page_start_loading: 'page_start_loading_earcon_description',
  pop_up_button: 'pop_up_button_earcon_description',
  slider: 'slider_earcon_description',
  wrap: 'wrap_earcon_description',
};


export class AbstractEarcons {
  /**
   * Plays the specified earcon sound.
   * @param {Earcon} earcon An earcon identifier.
   * @param {chrome.automation.Rect=} opt_location A location associated with
   *     the earcon such as a control's bounding rectangle.
   */
  playEarcon(earcon, opt_location) {}

  /**
   * Cancels the specified earcon sound.
   * @param {Earcon} earcon An earcon identifier.
   */
  cancelEarcon(earcon) {}

  /**
   * Whether or not earcons are available.
   * @return {boolean} True if earcons are available.
   */
  earconsAvailable() {
    return true;
  }

  /**
   * Whether or not earcons are enabled.
   * @return {boolean} True if earcons are enabled.
   */
  get enabled() {
    return LocalStorage.get('earcons');
  }

  /**
   * Set whether or not earcons are enabled.
   * @param {boolean} value True turns on earcons, false turns off earcons.
   */
  set enabled(value) {
    LocalStorage.set('earcons', value);
  }
}
