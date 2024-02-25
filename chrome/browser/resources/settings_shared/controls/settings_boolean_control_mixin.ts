// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A behavior to help controls that handle a boolean preference, such as
 * checkbox and toggle button.
 */

// clang-format off
import {assert} from 'chrome://resources/js/assert.js';
import type { PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type { CrPolicyPrefMixinInterface} from './cr_policy_pref_mixin.js';
import {CrPolicyPrefMixin} from './cr_policy_pref_mixin.js';
import type { PrefControlMixinInterface} from './pref_control_mixin.js';
import {PrefControlMixin} from './pref_control_mixin.js';

// clang-format on

export const DEFAULT_UNCHECKED_VALUE = 0;
export const DEFAULT_CHECKED_VALUE = 1;

type Constructor<T> = new (...args: any[]) => T;

export const SettingsBooleanControlMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SettingsBooleanControlMixinInterface> => {
      const superClassBase = CrPolicyPrefMixin(PrefControlMixin(superClass));

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
             * If true, do not automatically set the preference value. This
             * allows the container to confirm the change first then call either
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
             * For numeric prefs only. The integer values equivalent to the
             * initial unchecked state. During initialization, the control is
             * unchecked if and only if the pref value is equal to one of the
             * values in the array. When sendPrefChange() is called the *first*
             * value in this array will be sent to the backend.
             */
            numericUncheckedValues: {
              type: Array,
              value: () => [DEFAULT_UNCHECKED_VALUE],
            },

            /**
             * For numeric prefs only, the integer value equivalent to the
             * checked state. This is the value sent to prefs if the user
             * checked the control.
             */
            numericCheckedValue: {
              type: Number,
              value: DEFAULT_CHECKED_VALUE,
            },
          };
        }

        static get observers() {
          return ['prefValueChanged_(pref.value)'];
        }

        inverted: boolean;
        checked: boolean;
        disabled: boolean;
        noSetPref: boolean;
        label: string;
        subLabel: string;
        numericUncheckedValues: number[];
        numericCheckedValue: number;

        notifyChangedByUserInteraction() {
          this.dispatchEvent(new CustomEvent(
              'settings-boolean-control-change',
              {bubbles: true, composed: true}));

          if (!this.pref || this.noSetPref) {
            return;
          }
          this.sendPrefChange();
        }

        /** Reset the checked state to match the current pref value. */
        resetToPrefValue() {
          // Pref can be undefined, and will lead to console errors if accessed.
          if (this.pref === undefined) {
            this.checked = false;
            return;
          }

          this.checked = this.getNewValue_(this.pref!.value);
        }

        /** Update the pref to the current |checked| value. */
        sendPrefChange() {
          // Ensure that newValue is the correct type for the pref type, either
          // a boolean or a number.
          if (this.pref!.type === chrome.settingsPrivate.PrefType.NUMBER) {
            assert(!this.inverted);
            assert(this.numericUncheckedValues.length > 0);
            this.set(
                'pref.value',
                this.checked ? this.numericCheckedValue :
                               this.numericUncheckedValues[0]);
            return;
          }
          this.set('pref.value', this.inverted ? !this.checked : this.checked);
        }

        private prefValueChanged_(prefValue: number|boolean) {
          this.checked = this.getNewValue_(prefValue);
        }

        /**
         * @return The value as a boolean, inverted if |inverted| is true.
         */
        private getNewValue_(value: number|boolean): boolean {
          // For numeric prefs, the control is only false if the value is
          // a member of `numericUncheckedValues` value.
          if (this.pref!.type === chrome.settingsPrivate.PrefType.NUMBER) {
            assert(!this.inverted);
            return !this.numericUncheckedValues.includes(value as number);
          }
          return this.inverted ? !value : !!value;
        }

        controlDisabled(): boolean {
          return this.disabled || this.isPrefEnforced() ||
              !!(this.pref && this.pref.userControlDisabled);
        }
      }

      return SettingsBooleanControlMixin;
    });

export interface SettingsBooleanControlMixinInterface extends
    PrefControlMixinInterface, CrPolicyPrefMixinInterface {
  inverted: boolean;
  checked: boolean;
  disabled: boolean;
  noSetPref: boolean;
  label: string;
  subLabel: string;
  numericUncheckedValues: number[];
  numericCheckedValue: number;
  controlDisabled(): boolean;
  notifyChangedByUserInteraction(): void;
  resetToPrefValue(): void;
  sendPrefChange(): void;
}
