// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {State} from '../data/state.js';

import {InputBehavior} from './input_behavior.js';
import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-pin-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, InputBehavior],

  properties: {
    /** @type {!State} */
    state: Number,

    disabled: Boolean,

    /** @private {boolean} */
    checkboxDisabled_: {
      type: Boolean,
      computed: 'computeCheckboxDisabled_(inputValid_, disabled, ' +
          'settings.pin.setByPolicy)',
    },

    /** @private {boolean} */
    pinEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    inputString_: {
      type: String,
      value: '',
      observer: 'onInputChanged_',
    },

    /** @private */
    inputValid_: {
      type: Boolean,
      value: true,
      reflectToAttribute: true,
    },
  },

  observers: [
    'onSettingsChanged_(settings.pin.value, settings.pinValue.value)',
    'changePinValueSetting_(state)',
  ],

  listeners: {
    'input-change': 'onInputChange_',
  },

  /** @return {!CrInputElement} The cr-input field element for InputBehavior. */
  getInput: function() {
    return /** @type {!CrInputElement} */ (this.$.pinValue);
  },

  /**
   * @param {!CustomEvent<string>} e Contains the new input value.
   * @private
   */
  onInputChange_: function(e) {
    this.inputString_ = e.detail;
  },

  /** @private */
  onCollapseChanged_: function() {
    if (this.pinEnabled_) {
      /** @type {!CrInputElement} */ (this.$.pinValue).focusInput();
    }
  },

  /**
   * @param {boolean} inputValid Whether pin value is valid.
   * @param {boolean} disabled Whether pin setting is disabled.
   * @param {boolean} managed Whether pin setting is managed.
   * @return {boolean} Whether pin checkbox should be disabled.
   * @private
   */
  computeCheckboxDisabled_: function(inputValid, disabled, managed) {
    return managed || (inputValid && disabled);
  },

  /**
   * @return {boolean} Whether to disable the pin value input.
   * @private
   */
  inputDisabled_: function() {
    return !this.pinEnabled_ || (this.inputValid_ && this.disabled);
  },

  /**
   * Updates the checkbox state when the setting has been initialized.
   * @private
   */
  onSettingsChanged_: function() {
    const pinEnabled = /** @type {boolean} */ (this.getSetting('pin').value);
    this.$.pin.checked = pinEnabled;
    this.pinEnabled_ = pinEnabled;
    const pinValue = this.getSetting('pinValue');
    this.inputString_ = /** @type {string} */ (pinValue.value);
    this.resetString();
  },

  /** @private */
  onPinChange_: function() {
    this.setSetting('pin', this.$.pin.checked);
    // We need to set validity of pinValue to true to return to READY state
    // after unchecking the pin and to check the validity again after checking
    // the pin.
    if (!this.$.pin.checked) {
      this.setSettingValid('pinValue', true);
    } else {
      this.changePinValueSetting_();
    }
  },

  /**
   * @private
   */
  onInputChanged_: function() {
    this.changePinValueSetting_();
  },

  /**
   * Updates pin value setting based on the current value of the pin value
   * input.
   * @private
   */
  changePinValueSetting_: function() {
    if (this.settings === undefined) {
      return;
    }
    // If the state is not READY and current pinValue is valid (so it's not the
    // cause of the error) we need to wait until the state will be READY again.
    // It's done because we don't permit multiple simultaneous validation errors
    // in Print Preview and we also don't want to set the value when sticky
    // settings may not yet have been set.
    if (this.state != State.READY && this.settings.pinValue.valid) {
      return;
    }
    this.inputValid_ = this.computeValid_();
    this.setSettingValid('pinValue', this.inputValid_);

    // We allow to save the empty string as sticky setting value to give users
    // the opportunity to unset their PIN in sticky settings.
    if ((this.inputValid_ || this.inputString_ == '') &&
        this.inputString_ !== this.getSettingValue('pinValue')) {
      this.setSetting('pinValue', this.inputString_);
    }
  },

  /**
   * @return {boolean} Whether input value represented by inputString_ is
   *     valid, so that it can be used to update the setting.
   * @private
   */
  computeValid_: function() {
    // Make sure value updates first, in case inputString_ was updated by JS.
    this.$.pinValue.value = this.inputString_;
    this.$.pinValue.validate();
    return !this.$.pinValue.invalid;
  },
});
