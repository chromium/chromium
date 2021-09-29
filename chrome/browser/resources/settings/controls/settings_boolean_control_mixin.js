// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A behavior to help controls that handle a boolean preference, such as
 * checkbox and toggle button.
 */

// clang-format off
import {CrPolicyPrefMixin, CrPolicyPrefMixinInterface} from 'chrome://resources/cr_elements/policy/cr_policy_pref_mixin.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


import {PrefControlMixin, PrefControlMixinInterface} from './pref_control_mixin.js';
// clang-format on

/**
 * @polymer
 * @mixinFunction
 */
export const SettingsBooleanControlMixin = dedupingMixin(superClass => {
  /**
   * @constructor
   * @extends {PolymerElement}
   * @implements {PrefControlMixinInterface}
   * @implements {CrPolicyPrefMixinInterface}
   */
  const superClassBase = CrPolicyPrefMixin(PrefControlMixin(superClass));

  /**
   * @polymer
   * @mixinClass
   * @implements {SettingsBooleanControlMixinInterface}
   */
  class SettingsBooleanControlMixin extends superClassBase {
    static get properties() {
      return {
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
         * If true, do not automatically set the preference value. This allows
         * the container to confirm the change first then call either
         * sendPrefChange or resetToPrefValue accordingly.
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
         * state. This is the value sent to prefs if the user unchecks the
         * control. During initialization, the control is unchecked if and only
         * if the pref value is equal to the this value. (Values 2, 3, 4, etc.
         * all are checked.)
         */
        numericUncheckedValue: {
          type: Number,
          value: 0,
        },
      };
    }

    static get observers() {
      return ['prefValueChanged_(pref.value)'];
    }

    notifyChangedByUserInteraction() {
      this.dispatchEvent(new CustomEvent(
          'settings-boolean-control-change', {bubbles: true, composed: true}));

      if (!this.pref || this.noSetPref) {
        return;
      }
      this.sendPrefChange();
    }

    /** Reset the checked state to match the current pref value. */
    resetToPrefValue() {
      this.checked = this.getNewValue_(this.pref.value);
    }

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
    }

    /**
     * Polymer observer for pref.value.
     * @param {*} prefValue
     * @private
     */
    prefValueChanged_(prefValue) {
      this.checked = this.getNewValue_(prefValue);
    }

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
    }

    /**
     * @return {boolean} Whether the control should be disabled.
     * @protected
     */
    controlDisabled() {
      return this.disabled || this.isPrefEnforced() ||
          !!(this.pref && this.pref.userControlDisabled);
    }
  }

  return SettingsBooleanControlMixin;
});

/**
 * @interface
 * @extends {PrefControlMixinInterface}
 */
export class SettingsBooleanControlMixinInterface {
  constructor() {
    /** @type {boolean} */
    this.checked;

    /** @type {string} */
    this.label;
  }

  /** @return {boolean} */
  controlDisabled() {}

  notifyChangedByUserInteraction() {}
  resetToPrefValue() {}
  sendPrefChange() {}
}
