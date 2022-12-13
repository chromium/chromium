// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common prefs behavior.
 */

import {assert} from 'chrome://resources/ash/common/assert.js';

/** @polymerBehavior */
export const PrefsBehavior = {
  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /**
   * Gets the pref at the given prefPath. Throws if the pref is not found.
   * @param {string} prefPath
   * @return {!chrome.settingsPrivate.PrefObject}
   * @protected
   */
  getPref(prefPath) {
    const pref = /** @type {!chrome.settingsPrivate.PrefObject} */ (
        this.get(prefPath, this.prefs));
    assert(typeof pref !== 'undefined', 'Pref is missing: ' + prefPath);
    return pref;
  },

  /**
   * Sets the value of the pref at the given prefPath. Throws if the pref is not
   * found.
   * @param {string} prefPath
   * @param {*} value
   * @protected
   */
  setPrefValue(prefPath, value) {
    this.getPref(prefPath);  // Ensures we throw if the pref is not found.
    this.set('prefs.' + prefPath + '.value', value);
  },

  /**
   * Appends the item to the pref list at the given key if the item is not
   * already in the list. Asserts if the pref itself is not found or is not an
   * Array type.
   * @param {string} key
   * @param {*} item
   * @protected
   */
  appendPrefListItem(key, item) {
    const pref = this.getPref(key);
    assert(pref && pref.type === chrome.settingsPrivate.PrefType.LIST);
    if (pref.value.indexOf(item) === -1) {
      this.push('prefs.' + key + '.value', item);
    }
  },

  /**
   * Deletes the given item from the pref at the given key if the item is found.
   * Asserts if the pref itself is not found or is not an Array type.
   * @param {string} key
   * @param {*} item
   * @protected
   */
  deletePrefListItem(key, item) {
    assert(this.getPref(key).type === chrome.settingsPrivate.PrefType.LIST);
    this.arrayDelete('prefs.' + key + '.value', item);
  },
};

/** @interface */
export class PrefsBehaviorInterface {
  /** @return  {!Object} */
  get prefs() {}

  /** @param obj {!Object} */
  set prefs(obj) {}

  /**
   * @param {string} prefPath
   * @return {!chrome.settingsPrivate.PrefObject}
   */
  getPref(prefPath) {}

  /*
   * @param {string} prefPath
   * @param {*} value
   */
  setPrefValue(prefPath, value) {}

  /**
   * @param {string} key
   * @param {*} item
   */
  appendPrefListItem(key, item) {}

  /**
   * @param {string} key
   * @param {*} item
   */
  deletePrefListItem(key, item) {}
}
