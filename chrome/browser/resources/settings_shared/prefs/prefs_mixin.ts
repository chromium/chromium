// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common prefs behavior.
 */

// clang-format off
import {assert} from 'chrome://resources/js/assert.js';
import type { PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

type Constructor<T> = new (...args: any[]) => T;

export const PrefsMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrefsMixinInterface> => {
      class PrefsMixin extends superClass implements PrefsMixinInterface {
        static get properties() {
          return {
            /** Preferences state. */
            prefs: {
              type: Object,
              notify: true,
            },
          };
        }

        declare prefs: Record<string, unknown>;

        /**
         * Gets the pref at the given prefPath. Throws if the pref is not found.
         */
        getPref<T>(prefPath: string): chrome.settingsPrivate.PrefObject<T> {
          const pref = this.get(prefPath, this.prefs);
          assert(typeof pref !== 'undefined', 'Pref is missing: ' + prefPath);
          return pref;
        }

        /**
         * Sets the value of the pref at the given prefPath. Throws if the pref
         * is not found.
         */
        setPrefValue(prefPath: string, value: unknown) {
          this.getPref(prefPath);  // Ensures we throw if the pref is not found.
          this.set('prefs.' + prefPath + '.value', value);
        }

        /**
         * Appends the item to the pref list at the given key if the item is not
         * already in the list. Asserts if the pref itself is not found or is
         * not an Array type.
         */
        appendPrefListItem(key: string, item: unknown) {
          const pref = this.getPref<unknown[]>(key);
          assert(pref && pref.type === chrome.settingsPrivate.PrefType.LIST);
          if (pref.value.indexOf(item) === -1) {
            this.push('prefs.' + key + '.value', item);
          }
        }

        /**
         * Updates the item in the pref list to the new value. Asserts if the
         * pref itself is not found or is not an Array type.
         */
        updatePrefListItem(key: string, item: unknown, newItem: unknown) {
          const pref = this.getPref<unknown[]>(key);
          assert(pref && pref.type === chrome.settingsPrivate.PrefType.LIST);
          const index = pref.value.indexOf(item);
          if (index !== -1) {
            this.set(`prefs.${key}.value.${index}`, newItem);
          }
        }

        /**
         * Deletes the given item from the pref at the given key if the item is
         * found. Asserts if the pref itself is not found or is not an Array
         * type.
         */
        deletePrefListItem(key: string, item: unknown) {
          const pref = this.getPref<unknown[]>(key);
          assert(pref && pref.type === chrome.settingsPrivate.PrefType.LIST);
          const index = pref.value.indexOf(item);
          if (index !== -1) {
            this.splice(`prefs.${key}.value`, index, 1);
          }
        }

        /**
         * Updates the entry in the pref dictionary to the new key value pair.
         * Asserts if the pref itself is not found or is not a dictionary type.
         */
        setPrefDictEntry(prefPath: string, key: string, value: unknown) {
          const pref = this.getPref(prefPath);
          assert(
              pref && pref.type === chrome.settingsPrivate.PrefType.DICTIONARY);
          const dict = pref.value as Record<string, unknown>;
          dict[key] = value;
          this.set('prefs.' + prefPath + '.value', {...dict});
        }

        /**
         * Deletes the given key from the pref dictionary if it is
         * found. Asserts if the pref itself is not found or is not a dictionary
         * type.
         */
        deletePrefDictEntry(prefPath: string, key: string) {
          const pref = this.getPref(prefPath);
          assert(
              pref && pref.type === chrome.settingsPrivate.PrefType.DICTIONARY);
          const dict = pref.value as Record<string, unknown>;
          delete dict[key];
          this.set('prefs.' + prefPath + '.value', {...dict});
        }

        /**
         * Helper to assign a pref as a computed property from a string
         * constant. Usage: computed: `computePref(prefs.${PREF_NAME})`,
         */
        computePref(pref: chrome.settingsPrivate.PrefObject) {
          return pref;
        }
      }

      return PrefsMixin;
    });

export interface PrefsMixinInterface {
  prefs: Record<string, unknown>;
  getPref<T>(prefPath: string): chrome.settingsPrivate.PrefObject<T>;
  setPrefValue(prefPath: string, value: unknown): void;
  appendPrefListItem(key: string, item: unknown): void;
  updatePrefListItem(key: string, item: unknown, newItem: unknown): void;
  deletePrefListItem(key: string, item: unknown): void;
  setPrefDictEntry(prefPath: string, key: string, value: unknown): void;
  deletePrefDictEntry(prefPath: string, key: string): void;
}
