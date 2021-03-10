// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrPolicyPrefBehavior} from 'chrome://resources/cr_elements/policy/cr_policy_pref_behavior.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';

import {PrefControlBehavior} from './pref_control_behavior.js';
// clang-format on

/**
 * @fileoverview
 * A behavior to help controls that handle a boolean preference, such as
 * checkbox and toggle button.
 */

/** @polymerBehavior SettingsBooleanControlBehavior */
const SettingsBooleanControlBehaviorImpl = {
  properties: {
    /** Whether the control should represent the inverted value. */
    inverted: {
      type: Boolean,
      value: false,
    },

    /** Whether the control is checked. */
    checked: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true,
    },

    /** Disabled property for the element. */
    disabled: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true,
    },

    /**
     * If true, do not automatically set the preference value. This allows the
     * container to confirm the change first then call either sendPrefChange
     * or resetToPrefValue accordingly.
     */
    noSetPref: {
      type: Boolean,
      value: false,
    },

    /** The main label. */
    label: {
      type: String,
      value: '',
    },

    /** Additional (optional) sub-label. */
    subLabel: {
      type: String,
      value: '',
    },

    /**
     * For numeric prefs only, the integer value equivalent to the unchecked
     * state. This is the value sent to prefs if the user unchecks the control.
     * During initialization, the control is unchecked if and only if the pref
     * value is equal to the this value. (Values 2, 3, 4, etc. all are checked.)
     */
    numericUncheckedValue: {
      type: Number,
      value: 0,
    }
  },

  observers: [
    'prefValueChanged_(pref.value)',
  ],

  notifyChangedByUserInteraction() {
    this.fire('settings-boolean-control-change');

    if (!this.pref || this.noSetPref) {
      return;
    }
    this.sendPrefChange();
  },

  /** Reset the checked state to match the current pref value. */
  resetToPrefValue() {
    this.checked = this.getNewValue_(this.pref.value);
  },

  /** Update the pref to the current |checked| value. */
  sendPrefChange() {
    // Ensure that newValue is the correct type for the pref type, either
    // a boolean or a number.
    if (this.pref.type === chrome.settingsPrivate.PrefType.NUMBER) {
      assert(!this.inverted);
      this.set('pref.value', this.checked ? 1 : this.numericUncheckedValue);
      return;
    }
    this.set('pref.value', this.inverted ? !this.checked : this.checked);
  },

  /**
   * Polymer observer for pref.value.
   * @param {*} prefValue
   * @private
   */
  prefValueChanged_(prefValue) {
    this.checked = this.getNewValue_(prefValue);
  },

  /**
   * @param {*} value
   * @return {boolean} The value as a boolean, inverted if |inverted| is true.
   * @private
   */
  getNewValue_(value) {
    // For numeric prefs, the control is only false if the value is exactly
    // equal to the unchecked-equivalent value.
    if (this.pref.type === chrome.settingsPrivate.PrefType.NUMBER) {
      assert(!this.inverted);
      return value !== this.numericUncheckedValue;
    }
    return this.inverted ? !value : !!value;
  },

  /**
   * @return {boolean} Whether the control should be disabled.
   * @protected
   */
  controlDisabled() {
    return this.disabled || this.isPrefEnforced() ||
        !!(this.pref && this.pref.userControlDisabled);
  },
};

/** @polymerBehavior */
export const SettingsBooleanControlBehavior = [
  CrPolicyPrefBehavior,
  PrefControlBehavior,
  SettingsBooleanControlBehaviorImpl,
];
