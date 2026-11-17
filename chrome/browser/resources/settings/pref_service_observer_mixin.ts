// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type PrefObject<T> = chrome.settingsPrivate.PrefObject<T>;
type ObserverCallback<T> = (pref: Readonly<PrefObject<T>>) => void;

type Constructor<T> = new (...args: any[]) => T;

export const PrefServiceObserverMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrefServiceObserverMixinInterface> => {
      class PrefServiceObserverMixin extends superClass {
        private prefServiceObserverIds_: number[] = [];

        override disconnectedCallback() {
          super.disconnectedCallback();
          for (const id of this.prefServiceObserverIds_) {
            PrefService.getInstance().removeObserver(id);
          }
          this.prefServiceObserverIds_ = [];
        }

        addPrefObserver<T>(prefKey: string, callback: ObserverCallback<T>):
            number {
          const id =
              PrefService.getInstance().addObserver<T>(prefKey, callback);
          this.prefServiceObserverIds_.push(id);
          return id;
        }

        removePrefObserver(id: number) {
          PrefService.getInstance().removeObserver(id);
          const index = this.prefServiceObserverIds_.indexOf(id);
          if (index !== -1) {
            this.prefServiceObserverIds_.splice(index, 1);
          }
        }

        /**
         * Sets up a one-way binding from a preference to a Polymer property.
         * The property will be updated automatically when the preference
         * changes.
         */
        mirrorPref(prefKey: string, propName: string) {
          this.addPrefObserver(prefKey, pref => {
            this.set(propName, pref);
          });
        }

        /**
         * Sets up multiple one-way bindings from preferences to Polymer
         * properties.
         */
        mirrorPrefs(prefMap: Record<string, string>) {
          for (const [prefKey, propName] of Object.entries(prefMap)) {
            this.mirrorPref(prefKey, propName);
          }
        }
      }

      return PrefServiceObserverMixin;
    });

export interface PrefServiceObserverMixinInterface {
  addPrefObserver<T>(prefKey: string, callback: ObserverCallback<T>): number;
  removePrefObserver(id: number): void;
  mirrorPref(prefKey: string, propName: string): void;
  mirrorPrefs(prefMap: Record<string, string>): void;
}
