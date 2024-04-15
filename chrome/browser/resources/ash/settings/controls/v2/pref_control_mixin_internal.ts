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

import {dedupingMixin, type PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Constructor} from '../../common/types.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

export interface PrefControlMixinInternalInterface {
  disabled: boolean;
  isPrefEnforced: boolean;
  pref?: PrefObject;
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
             * Represents if this element should be disabled or not. If `pref`
             * exists, `isPrefEnforced` is considered.
             */
            disabled: {
              type: Boolean,
              reflectToAttribute: true,
              value: false,
            },
          };
        }

        static get observers() {
          return ['syncPrefEnforcementToDisabled_(isPrefEnforced)'];
        }

        disabled: boolean;
        readonly isPrefEnforced: boolean;
        pref?: PrefObject;

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

        // TODO(b/333454004) Add pref validation logic.
        // TODO(b/333453826) Add dispatch pref change event for one-way binding.
      }

      return PrefControlMixinInternalImpl;
    });
