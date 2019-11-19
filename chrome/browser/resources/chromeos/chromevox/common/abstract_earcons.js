// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for implementing earcons.
 *
 * When adding earcons, please add them to getEarconName and getEarconId.
 *
 */

goog.provide('AbstractEarcons');
goog.provide('Earcon');
goog.provide('EarconDescription');


/**
 * Earcon names.
 * @enum {string}
 */
Earcon = {
  ALERT_MODAL: 'alert_modal',
  ALERT_NONMODAL: 'alert_nonmodal',
  BUTTON: 'button',
  CHECK_OFF: 'check_off',
  CHECK_ON: 'check_on',
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
  WRAP: 'wrap',
  WRAP_EDGE: 'wrap_edge',
};

/**
 * Maps a earcon id to a message id description.
 * Only add mappings for earcons used in ChromeVox Next. This map gets
 * used to generate tutorial content.
 * @type {Object<string, string>}
 */
var EarconDescription = {
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


/**
 * @constructor
 */
AbstractEarcons = function() {};


/**
 * Public static flag set to enable or disable earcons. Callers should prefer
 * toggle(); however, this member is public for initialization.
 * @type {boolean}
 */
AbstractEarcons.enabled = true;


/**
 * Plays the specified earcon sound.
 * @param {Earcon} earcon An earcon identifier.
 * @param {Object=} opt_location A location associated with the earcon such as a
 * control's bounding rectangle.
 */
AbstractEarcons.prototype.playEarcon = function(earcon, opt_location) {};


/**
 * Cancels the specified earcon sound.
 * @param {Earcon} earcon An earcon identifier.
 */
AbstractEarcons.prototype.cancelEarcon = function(earcon) {};


/**
 * Whether or not earcons are available.
 * @return {boolean} True if earcons are available.
 */
AbstractEarcons.prototype.earconsAvailable = function() {
  return true;
};


/**
 * Toggles earcons on or off.
 * @return {boolean} True if earcons are now enabled; false otherwise.
 */
AbstractEarcons.prototype.toggle = function() {
  AbstractEarcons.enabled = !AbstractEarcons.enabled;
  return AbstractEarcons.enabled;
};
