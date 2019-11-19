// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './icons.js';
import './shared_styles.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'input-device-settings',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  ready: function() {
    this.addWebUIListener(
        'touchpad-exists-changed', this.setTouchpadExists_.bind(this));
    this.addWebUIListener(
        'mouse-exists-changed', this.setMouseExists_.bind(this));
  },

  /**
   * @param {!Event} e
   * Callback when the user toggles the touchpad.
   */
  onTouchpadChange: function(e) {
    chrome.send('setHasTouchpad', [e.target.checked]);
    this.$.changeDescription.opened = true;
  },

  /**
   * @param {!Event} e
   * Callback when the user toggles the mouse.
   */
  onMouseChange: function(e) {
    chrome.send('setHasMouse', [e.target.checked]);
    this.$.changeDescription.opened = true;
  },

  /**
   * Callback when the existence of a fake mouse changes.
   * @param {boolean} exists
   * @private
   */
  setMouseExists_: function(exists) {
    this.$.mouse.checked = exists;
  },

  /**
   * Callback when the existence of a fake touchpad changes.
   * @param {boolean} exists
   * @private
   */
  setTouchpadExists_: function(exists) {
    this.$.touchpad.checked = exists;
  },
});
