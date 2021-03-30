// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Utility functions to help use prefs in Polymer controls. */

import {assertNotReached} from 'chrome://resources/js/assert.m.js';

/**
 * Converts a string value to a type corresponding to the given preference.
 * @param {string} value
 * @param {!chrome.settingsPrivate.PrefObject} pref
 * @return {boolean|number|string|undefined}
 */
export function stringToPrefValue(value, pref) {
  switch (pref.type) {
    case chrome.settingsPrivate.PrefType.BOOLEAN:
      return value === 'true';
    case chrome.settingsPrivate.PrefType.NUMBER:
      const n = parseFloat(value);
      if (isNaN(n)) {
        console.error(
            'Argument to stringToPrefValue for number pref ' +
            'was unparsable: ' + value);
        return undefined;
      }
      return n;
    case chrome.settingsPrivate.PrefType.STRING:
    case chrome.settingsPrivate.PrefType.URL:
      return value;
    default:
      assertNotReached('No conversion from string to ' + pref.type + ' pref');
  }
}

  /**
   * Returns the value of the pref as a string.
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {string}
   */
export function prefToString(pref) {
  switch (pref.type) {
    case chrome.settingsPrivate.PrefType.BOOLEAN:
    case chrome.settingsPrivate.PrefType.NUMBER:
      return pref.value.toString();
    case chrome.settingsPrivate.PrefType.STRING:
    case chrome.settingsPrivate.PrefType.URL:
      return /** @type {string} */ (pref.value);
    default:
      assertNotReached('No conversion from ' + pref.type + ' pref to string');
  }
}
