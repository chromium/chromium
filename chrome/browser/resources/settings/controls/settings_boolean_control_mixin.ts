// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A behavior to help controls that handle a boolean preference, such as
 * checkbox and toggle button.
 */

// clang-format off
import {assert} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyPrefMixin, CrPolicyPrefMixinInterface} from './cr_policy_pref_mixin.js';
import {PrefControlMixin, PrefControlMixinInterface} from './pref_control_mixin.js';

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
             * For numeric prefs only, the integer value equivalent to the
             * unchecked state. This is the value sent to prefs if the user
             * unchecks the control. During initialization, the control is
             * unchecked if and only if the pref value is equal to the this
             * value. (Values 2, 3, 4, etc. all are checked.)
             */
            numericUncheckedValue: {
              type: Number,
              value: DEFAULT_UNCHECKED_VALUE,
              reflectToAttribute: true,
            },

            /**
             * For numeric prefs only, the integer value equivalent to the
             * checked state. This is the value sent to prefs if the user
             * checked the control.
             */
            numericCheckedValue: {
              type: Number,
              value: DEFAULT_CHECKED_VALUE,
              reflectToAttribute: true,
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
        numericUncheckedValue: number;
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
          this.checked = this.getNewValue_(this.pref!.value);
        }

        /** Update the pref to the current |checked| value. */
        sendPrefChange() {
          // Ensure that newValue is the correct type for the pref type, either
          // a boolean or a number.
          if (this.pref!.type === chrome.settingsPrivate.PrefType.NUMBER) {
            assert(!this.inverted);
            this.set(
                'pref.value',
                this.checked ? this.numericCheckedValue :
                               this.numericUncheckedValue);
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
          // exactly equal to the unchecked-equivalent value.
          if (this.pref!.type === chrome.settingsPrivate.PrefType.NUMBER) {
            assert(!this.inverted);
            return value !== this.numericUncheckedValue;
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
  numericUncheckedValue: number;
  numericCheckedValue: number;
  controlDisabled(): boolean;
  notifyChangedByUserInteraction(): void;
  resetToPrefValue(): void;
  sendPrefChange(): void;
}
