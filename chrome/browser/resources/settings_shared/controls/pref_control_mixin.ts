// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrSettingsPrefs} from '../prefs/prefs_types.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * Tracks the initialization of a specified preference and logs an error if the
 * pref is not defined after prefs have been fetched.
 */
export const PrefControlMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrefControlMixinInterface> => {
      class PrefControlMixin extends superClass {
        static get properties() {
          return {
            /** The Preference object being tracked. */
            pref: {
              type: Object,
              notify: true,
              observer: 'validatePref_',
            },
          };
        }

        pref?: chrome.settingsPrivate.PrefObject;

        override connectedCallback() {
          super.connectedCallback();
          this.validatePref_();
        }

        /**
         * Logs an error once prefs are initialized if the tracked pref is not
         * found.
         */
        private validatePref_() {
          CrSettingsPrefs.initialized.then(() => {
            if (this.pref === undefined) {
              console.error(this.getErrorInfo('not found'));
            } else if (typeof this.pref === 'string') {
              console.error(this.getErrorInfo('incorrect type string'));
            } else if (
                this.pref.enforcement ===
                chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED) {
              console.error(
                  'PARENT_SUPERVISED is not enforced by pref controls');
            }
          });
        }

        /**
         * Produce an error message with additional information about the
         * element and host causing the error.
         */
        private getErrorInfo(message: string): string {
          let error = `Pref error [${message}] for element ${this.tagName}`;
          if (this.id) {
            error += `#${this.id}`;
          }
          error += ` in ${(this.getRootNode() as ShadowRoot).host.tagName}`;
          return error;
        }
      }

      return PrefControlMixin;
    });

export interface PrefControlMixinInterface {
  pref?: chrome.settingsPrivate.PrefObject;
}
