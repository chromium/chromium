// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled settings prefs.
 */

import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export const CrPolicyPrefMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<CrPolicyPrefMixinInterface> => {
      class CrPolicyPrefMixin extends superClass {
        static get properties() {
          return {
            /**
             * Showing that an extension is controlling a pref is sometimes done
             * with a different UI (e.g. extension-controlled-indicator). In
             * those cases, avoid showing an (extra) indicator here.
             */
            noExtensionIndicator: Boolean,

            pref: Object,
          };
        }

        noExtensionIndicator: boolean;
        pref: chrome.settingsPrivate.PrefObject;

        /**
         * Is the |pref| controlled by something that prevents user control of
         * the preference.
         * @return True if |this.pref| is controlled by an enforced policy.
         */
        isPrefEnforced(): boolean {
          return !!this.pref &&
              this.pref.enforcement ===
              chrome.settingsPrivate.Enforcement.ENFORCED;
        }

        /**
         * @return True if |this.pref| has a recommended or enforced policy.
         */
        hasPrefPolicyIndicator(): boolean {
          if (!this.pref) {
            return false;
          }
          if (this.noExtensionIndicator &&
              this.pref.controlledBy ===
                  chrome.settingsPrivate.ControlledBy.EXTENSION) {
            return false;
          }
          return this.isPrefEnforced() ||
              this.pref.enforcement ===
              chrome.settingsPrivate.Enforcement.RECOMMENDED;
        }
      }

      return CrPolicyPrefMixin;
    });

export interface CrPolicyPrefMixinInterface {
  noExtensionIndicator: boolean;
  isPrefEnforced(): boolean;
}
