// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs} from '../prefs/prefs_types.js';

/**
 * @polymerBehavior Tracks the initialization of a specified preference and
 * logs an error if the pref is not defined after prefs have been fetched.
 */
export const PrefControlBehavior = {
  properties: {
    /**
     * The Preference object being tracked.
     * @type {!chrome.settingsPrivate.PrefObject|undefined}
     */
    pref: {
      type: Object,
      notify: true,
      observer: 'validatePref_',
    },
  },

  /** @override */
  ready() {
    this.validatePref_();
  },

  /**
   * Logs an error once prefs are initialized if the tracked pref is not found.
   * @private
   */
  validatePref_() {
    CrSettingsPrefs.initialized.then(() => {
      if (this.pref === undefined) {
        let error = 'Pref not found for element ' + this.tagName;
        if (this.id) {
          error += '#' + this.id;
        }
        error += ' in ' + this.getRootNode().host.tagName;
        console.error(error);
      } else if (
          this.pref.enforcement ===
          chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED) {
        console.error('PARENT_SUPERVISED is not enforced by pref controls');
      }
    });
  },
};
