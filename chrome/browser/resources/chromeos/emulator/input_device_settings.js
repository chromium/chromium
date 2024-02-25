// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './icons.js';
import './shared_styles.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'input-device-settings',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  ready() {
    this.addWebUIListener(
        'touchpad-exists-changed', this.setTouchpadExists_.bind(this));
    this.addWebUIListener(
        'mouse-exists-changed', this.setMouseExists_.bind(this));
  },

  /**
   * @param {!Event} e
   * Callback when the user toggles the touchpad.
   */
  onTouchpadChange(e) {
    chrome.send('setHasTouchpad', [e.target.checked]);
    this.$.changeDescription.opened = true;
  },

  /**
   * @param {!Event} e
   * Callback when the user toggles the mouse.
   */
  onMouseChange(e) {
    chrome.send('setHasMouse', [e.target.checked]);
    this.$.changeDescription.opened = true;
  },

  /**
   * Callback when the existence of a fake mouse changes.
   * @param {boolean} exists
   * @private
   */
  setMouseExists_(exists) {
    this.$.mouse.checked = exists;
  },

  /**
   * Callback when the existence of a fake touchpad changes.
   * @param {boolean} exists
   * @private
   */
  setTouchpadExists_(exists) {
    this.$.touchpad.checked = exists;
  },
});
