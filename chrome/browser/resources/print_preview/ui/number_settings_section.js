// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import './settings_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InputBehavior} from './input_behavior.js';

Polymer({
  is: 'print-preview-number-settings-section',

  _template: html`{__html_template__}`,

  behaviors: [InputBehavior],

  properties: {
    /** @private {string} */
    inputString_: {
      type: String,
      notify: true,
      observer: 'onInputChanged_',
    },

    /** @type {boolean} */
    inputValid: {
      type: Boolean,
      notify: true,
      reflectToAttribute: true,
      value: true,
    },

    /** @type {string} */
    currentValue: {
      type: String,
      notify: true,
      observer: 'onCurrentValueChanged_',
    },

    defaultValue: String,

    maxValue: Number,

    minValue: Number,

    inputLabel: String,

    inputAriaLabel: String,

    hintMessage: String,

    disabled: Boolean,
  },

  listeners: {
    'input-change': 'onInputChange_',
  },

  /** @return {!CrInputElement} The cr-input field element for InputBehavior. */
  getInput: function() {
    return /** @type {!CrInputElement} */ (this.$.userValue);
  },

  /**
   * @param {!CustomEvent<string>} e Contains the new input value.
   * @private
   */
  onInputChange_: function(e) {
    this.inputString_ = e.detail;
  },

  /**
   * @return {boolean} Whether the input should be disabled.
   * @private
   */
  getDisabled_: function() {
    return this.disabled && this.inputValid;
  },

  /**
   * @param {!KeyboardEvent} e The keyboard event
   */
  onKeydown_: function(e) {
    if (['.', 'e', 'E', '-', '+'].includes(e.key)) {
      e.preventDefault();
      return;
    }

    if (e.key == 'Enter') {
      this.onBlur_();
    }
  },

  /** @private */
  onBlur_: function() {
    if (this.inputString_ == '') {
      this.set('inputString_', this.defaultValue);
    }
    if (this.$.userValue.value == '') {
      this.$.userValue.value = this.defaultValue;
    }
  },

  /** @private */
  onInputChanged_: function() {
    this.inputValid = this.computeValid_();
    this.currentValue = this.inputString_;
  },

  /** @private */
  onCurrentValueChanged_: function() {
    this.inputString_ = this.currentValue;
    this.resetString();
  },

  /**
   * @return {boolean} Whether input value represented by inputString_ is
   *     valid and non-empty, so that it can be used to update the setting.
   * @private
   */
  computeValid_: function() {
    // Make sure value updates first, in case inputString_ was updated by JS.
    this.$.userValue.value = this.inputString_;
    return !this.$.userValue.invalid;
  },
});
