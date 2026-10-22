// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefServiceObserverMixin} from './pref_service_observer_mixin.js';
import type {PrefServiceObserverMixinInterface} from './pref_service_observer_mixin.js';

type Constructor<T> = new (...args: any[]) => T;

export const PrefKeyObserverMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrefKeyObserverMixinInterface>&
    Constructor<PrefServiceObserverMixinInterface> => {
      const superClassBase = PrefServiceObserverMixin(superClass);

      class PrefKeyObserverMixin extends superClassBase {
        static get properties() {
          return {
            prefKey: {
              type: String,
              value: '',
              observer: 'onPrefKeyChanged_',
            },

            pref: Object,
          };
        }

        declare prefKey: string;
        declare pref?: chrome.settingsPrivate.PrefObject;

        private onPrefKeyChanged_(newKey: string, oldKey: string|undefined) {
          if (newKey === '' && oldKey === undefined) {
            return;
          }

          // Disallow re-assigning the prefKey after initial assignment.
          assert(!oldKey);
          this.mirrorPref(newKey, 'pref');
        }
      }

      return PrefKeyObserverMixin;
    });

export interface PrefKeyObserverMixinInterface extends
    PrefServiceObserverMixinInterface {
  prefKey: string;
  pref?: chrome.settingsPrivate.PrefObject;
}
