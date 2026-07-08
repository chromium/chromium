// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {PrefServiceObserverMixinLitInterface} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';

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
          };
        }

        accessor prefKey: string = '';

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          if (changedProperties.has('prefKey')) {
            this.onPrefKeyChanged_(
                this.prefKey, changedProperties.get('prefKey'));
          }
        }

        private onPrefKeyChanged_(newKey: string, oldKey: string|undefined) {
          if (newKey === '' && oldKey === undefined) {
            return;
          }

          // Disallow re-assigning the prefKey after initial assignment.
          assert(!oldKey);
          this.mirrorPref(newKey, 'pref');
        }
      }

      return PrefKeyObserverMixinLit;
    };

export interface PrefKeyObserverMixinLitInterface extends
    PrefServiceObserverMixinLitInterface {
  prefKey: string;
}
