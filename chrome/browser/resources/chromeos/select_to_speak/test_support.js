// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Stubs out extension API functions so that SelectToSpeakUnitTest
 * can load.
 */

chrome.automation = {};

/**
 * Stub
 */
chrome.automation.getDesktop = function() {};

/**
 * Set necessary constants.
 */
chrome.automation.RoleType = {
  CHECK_BOX: 'checkBox',
  INLINE_TEXT_BOX: 'inlineTextBox',
  MENU_ITEM_CHECK_BOX: 'menuItemCheckBox',
  MENU_ITEM_RADIO: 'menuItemRadio',
  PARAGRAPH: 'paragraph',
  RADIO_BUTTON: 'radioButton',
  ROOT_WEB_AREA: 'rootWebArea',
  STATIC_TEXT: 'staticText',
  SVG_ROOT: 'svgRoot',
  TEXT_FIELD: 'textField',
  WINDOW: 'window'
};

chrome.automation.StateType = {
  INVISIBLE: 'invisible'
};

chrome.metricsPrivate = {
  recordUserAction: function() {},
  recordValue: function() {},
  MetricTypeType: {HISTOGRAM_LINEAR: 1}
};

chrome.commandLinePrivate = {
  hasSwitch: function() {}
};

chrome.accessibilityPrivate = {};

chrome.accessibilityPrivate.SelectToSpeakState = {
  INACTIVE: 'inactive',
  SELECTING: 'selecting',
  SPEAKING: 'speaking'
};

chrome.i18n = {
  getMessage: function(key) {
    if (key == 'select_to_speak_checkbox_checked') {
      return 'checked';
    }
    if (key == 'select_to_speak_checkbox_unchecked') {
      return 'unchecked';
    }
    if (key == 'select_to_speak_checkbox_mixed') {
      return 'partially checked';
    }
    if (key == 'select_to_speak_radiobutton_selected') {
      return 'selected';
    }
    if (key == 'select_to_speak_radiobutton_unselected') {
      return 'unselected';
    }
    if (key == 'select_to_speak_radiobutton_mixed') {
      return 'partially selected';
    }
    return '';
  }
};
