// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * PrefControlMixinInternal is a mixin that enhances the client element with
 * pref read/write capabilities (i.e. ability to control a pref). The
 * subscribing element is not required to provide a pref object and should be
 * able to work with or without a pref object.
 *
 * This mixin should only be used by settings components, hence the "internal"
 * suffix.
 */

import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import {assert} from 'chrome://resources/js/assert.js';
import {dedupingMixin, type PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Constructor, UserActionSettingPrefChangeEvent} from '../../common/types.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

export interface PrefControlMixinInternalInterface {
  disabled: boolean;
  readonly isPrefEnforced: boolean;
  pref?: PrefObject;
  validPrefTypes: chrome.settingsPrivate.PrefType[];
  updatePrefValueFromUserAction(value: any): void;
  validatePref(): void;
}

export const PrefControlMixinInternal = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrefControlMixinInternalInterface> => {
      class PrefControlMixinInternalImpl extends superClass {
        static get properties() {
          return {
            /**
             * The PrefObject being controlled by this element.
             */
            pref: {
              type: Object,
              notify: false,  // Two-way binding is intentionally unsupported.
              value: undefined,
            },

            /**
             * Represents if `pref` is enforced by a policy, in which case
             * user-initiated updates are prevented.
             */
            isPrefEnforced: {
              type: Boolean,
              computed: 'computeIsPrefEnforced_(pref.*)',
              readOnly: true,
              value: false,
            },

            /**
             * Represents if this element should be disabled or not. If
             * `isPrefEnforced` is true, then `disabled` is always true.
             */
            disabled: {
              type: Boolean,
              reflectToAttribute: true,
              value: false,
            },
          };
        }

        static get observers() {
          return ['syncPrefEnforcementToDisabled_(disabled, isPrefEnforced)'];
        }

        disabled: boolean;
        readonly isPrefEnforced: boolean;
        pref?: PrefObject;

        /**
         * Array of valid types for `pref`. An empty array means all pref types
         * are supported. A non-empty array means that only those specified
         * types are supported.
         */
        validPrefTypes: chrome.settingsPrivate.PrefType[] = [];

        override connectedCallback(): void {
          super.connectedCallback();

          CrSettingsPrefs.initialized.then(() => {
            this.validatePref();
          });
        }

        /**
         * Logs an error once prefs are initialized if the tracked pref is not
         * found. Subscribing elements can override and/or extend this method to
         * handle additional validation.
         */
        validatePref(): void {
          // Pref is not a required property.
          if (this.pref === undefined) {
            return;
          }

          assert(this.pref, this.makeErrorMessage_('Pref not found.'));

          // Assignment for `pref` happens via Polymer data-binding in HTML
          // which has no typechecking. This check catches data-binding errors
          // like `pref="prefs.foo.bar"` during runtime.
          assert(
              typeof this.pref !== 'string',
              this.makeErrorMessage_('Invalid string literal.'));

          if (this.validPrefTypes.length > 0) {
            assert(
                this.validPrefTypes.includes(this.pref.type),
                this.makeErrorMessage_(`Invalid pref type ${this.pref.type}.`));
          }
        }

        /**
         * Updates the value of `pref` with the given `value` and dispatches
         * a `user-action-setting-pref-change` event to sync the pref update.
         * Raises an error if called when `pref` is not defined.
         * @param value the new value of the pref.
         */
        updatePrefValueFromUserAction(value: any): void {
          assert(
              this.pref,
              'updatePrefValueFromUserAction() requires pref to be defined.');

          this.set('pref.value', value);
          this.dispatchPrefChange_(value);
        }

        /**
         * Dispatches a `user-action-setting-pref-change` event to notify the
         * any subscribing elements (i.e. os-settings-ui) about a pending pref
         * change. Raises an error if called when `pref` is not defined.
         * @param value the new value of the pref.
         */
        private dispatchPrefChange_(value: any): void {
          assert(
              this.pref, 'dispatchPrefChange_() requires pref to be defined.');

          const event: UserActionSettingPrefChangeEvent =
              new CustomEvent('user-action-setting-pref-change', {
                bubbles: true,
                composed: true,
                detail: {prefKey: this.pref.key, value},
              });
          this.dispatchEvent(event);
        }

        /**
         * PrefObjects are marked as enforced per `PrefsUtil::GetPref()` in
         * `chrome/browser/extensions/api/settings_private/prefs_util.cc`.
         * @returns true if `pref` exists and is enforced. Else returns false.
         */
        private computeIsPrefEnforced_(): boolean {
          return this.pref?.enforcement ===
              chrome.settingsPrivate.Enforcement.ENFORCED;
        }

        /**
         * Observes changes to the `isPrefEnforced` property and updates the
         * `disabled` property accordingly.
         */
        private syncPrefEnforcementToDisabled_(): void {
          this.disabled = this.disabled || this.isPrefEnforced;
        }

        /**
         * Generates an error message, including additional information about
         * the element causing the error.
         */
        private makeErrorMessage_(message: string): string {
          let error = this.tagName;
          if (this.id) {
            error += `#${this.id}`;
          }
          error += ` error: ${message}`;
          return error;
        }
      }

      return PrefControlMixinInternalImpl;
    });
