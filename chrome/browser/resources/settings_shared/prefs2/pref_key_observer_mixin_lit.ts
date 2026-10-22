// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {assert} from 'chrome://resources/js/assert.js';

import {PrefServiceObserverMixinLit} from './pref_service_observer_mixin_lit.js';
import type {PrefServiceObserverMixinLitInterface} from './pref_service_observer_mixin_lit.js';

type Constructor<T> = new (...args: any[]) => T;

export const PrefKeyObserverMixinLit =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<PrefKeyObserverMixinLitInterface>&
    Constructor<PrefServiceObserverMixinLitInterface> => {
      const superClassBase = PrefServiceObserverMixinLit(superClass);

      class PrefKeyObserverMixinLit extends superClassBase implements
          PrefKeyObserverMixinLitInterface {
        static get properties() {
          return {
            prefKey: {type: String},
            pref: {type: Object},
          };
        }

        accessor prefKey: string = '';
        accessor pref: chrome.settingsPrivate.PrefObject|undefined = undefined;
        private observedPref_: chrome.settingsPrivate.PrefObject|undefined =
            undefined;

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          if (changedProperties.has('prefKey')) {
            this.onPrefKeyChanged_(
                this.prefKey, changedProperties.get('prefKey'));
          }

          if (changedProperties.has('pref')) {
            // Disallow setting `pref` directly instead of via `prefKey`.
            assert(this.pref === this.observedPref_);
          }
        }

        private onPrefKeyChanged_(newKey: string, oldKey: string|undefined) {
          if (newKey === '' && oldKey === undefined) {
            return;
          }

          // Disallow re-assigning the prefKey after initial assignment.
          assert(!oldKey);
          this.addPrefObserver(newKey, pref => {
            this.observedPref_ = pref;
            this.pref = pref;
          });
        }
      }

      return PrefKeyObserverMixinLit;
    };

export interface PrefKeyObserverMixinLitInterface extends
    PrefServiceObserverMixinLitInterface {
  prefKey: string;
  pref: chrome.settingsPrivate.PrefObject|undefined;
}
