// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A mixin to help controls that handle a boolean preference, such as
 * checkbox and toggle button, in Lit.
 */

import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrPolicyPrefMixinLitInterface} from '/shared/settings/controls/cr_policy_pref_mixin_lit.js';
import {CrPolicyPrefMixinLit} from '/shared/settings/controls/cr_policy_pref_mixin_lit.js';
import type {PrefKeyObserverMixinLitInterface} from '/shared/settings/prefs2/pref_key_observer_mixin_lit.js';
import {PrefKeyObserverMixinLit} from '/shared/settings/prefs2/pref_key_observer_mixin_lit.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {assert} from 'chrome://resources/js/assert.js';

export const DEFAULT_UNCHECKED_VALUE = 0;
export const DEFAULT_CHECKED_VALUE = 1;

type Constructor<T> = new (...args: any[]) => T;

export const SettingsBooleanControlMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<SettingsBooleanControlMixinInterface> => {
      const superClassBase =
          CrPolicyPrefMixinLit(PrefKeyObserverMixinLit(superClass));

      class SettingsBooleanControlMixin extends superClassBase implements
          SettingsBooleanControlMixinInterface {
        static get properties() {
          return {
            /** Whether the control should represent the inverted value. */
            inverted: {type: Boolean},

            /** Whether the control is checked. */
            checked: {
              type: Boolean,
              notify: true,
              reflect: true,
            },

            /** Disabled property for the element. */
            disabled: {
              type: Boolean,
              notify: true,
              reflect: true,
            },

            /**
             * If true, do not automatically set the preference value. This
             * allows the container to confirm the change first then call either
             * sendPrefChange or resetToPrefValue accordingly.
             */
            noSetPref: {type: Boolean},

            /** The main label. */
            label: {type: String},

            /** Additional (optional) sub-label. */
            subLabel: {type: String},

            /**
             * For numeric prefs only. The integer values equivalent to the
             * initial unchecked state. During initialization, the control is
             * unchecked if and only if the pref value is equal to one of the
             * values in the array. When sendPrefChange() is called the *first*
             * value in this array will be sent to the backend.
             */
            numericUncheckedValues: {type: Array},

            /**
             * For numeric prefs only, the integer value equivalent to the
             * checked state. This is the value sent to prefs if the user
             * checked the control.
             */
            numericCheckedValue: {type: Number},
          };
        }

        accessor inverted: boolean = false;
        accessor checked: boolean = false;
        accessor disabled: boolean = false;
        accessor noSetPref: boolean = false;
        accessor label: string = '';
        accessor subLabel: string = '';
        accessor numericUncheckedValues: number[] = [DEFAULT_UNCHECKED_VALUE];
        accessor numericCheckedValue: number = DEFAULT_CHECKED_VALUE;

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          if (changedProperties.has('pref') ||
              changedProperties.has('numericUncheckedValues')) {
            if (this.pref === undefined) {
              return;
            }
            this.checked = this.getNewValue_(this.pref.value);
          }
        }

        async notifyChangedByUserInteraction() {
          if (this.pref && !this.noSetPref) {
            this.sendPrefChange();
          }

          await this.updateComplete;
          this.fire('settings-boolean-control-change');
        }

        /** Reset the checked state to match the current pref value. */
        resetToPrefValue() {
          // Pref can be undefined, and will lead to console errors if accessed.
          if (this.pref === undefined) {
            this.checked = false;
            return;
          }

          this.checked = this.getNewValue_(this.pref.value);
        }

        /** Update the pref to the current |checked| value. */
        sendPrefChange() {
          assert(this.prefKey);
          // Ensure that newValue is the correct type for the pref type, either
          // a boolean or a number.
          if (this.pref!.type === chrome.settingsPrivate.PrefType.NUMBER) {
            assert(!this.inverted);
            assert(this.numericUncheckedValues.length > 0);
            PrefService.getInstance().setPrefValue(
                this.prefKey,
                this.checked ? this.numericCheckedValue :
                               this.numericUncheckedValues[0]);
            return;
          }
          PrefService.getInstance().setPrefValue(
              this.prefKey, this.inverted ? !this.checked : this.checked);
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
    };

export interface SettingsBooleanControlMixinInterface extends
    PrefKeyObserverMixinLitInterface, CrPolicyPrefMixinLitInterface {
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
