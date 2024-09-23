// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Utility functions to help use prefs in Polymer controls. */

import {assertNotReached} from 'chrome://resources/js/assert.js';

/**
 * Converts a string value to a type corresponding to the given preference.
 */
export function stringToPrefValue(
    value: string, pref: chrome.settingsPrivate.PrefObject):
    (boolean|number|string|undefined) {
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
 */
export function prefToString(pref: chrome.settingsPrivate.PrefObject): string {
  switch (pref.type) {
    case chrome.settingsPrivate.PrefType.BOOLEAN:
    case chrome.settingsPrivate.PrefType.NUMBER:
      return pref.value.toString();
    case chrome.settingsPrivate.PrefType.STRING:
    case chrome.settingsPrivate.PrefType.URL:
      return pref.value;
    default:
      assertNotReached('No conversion from ' + pref.type + ' pref to string');
  }
}
