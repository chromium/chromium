// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and warns users of the expected outcome
 * when disabling peripheral data access setup.
 */

Polymer({
  is: 'settings-peripheral-data-access-protection-dialog',

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /**
   * Closes the warning dialog and transitions to the disabling dialog.
   * @private
   */
  onDisableClicked_() {
    // Send the new state immediately, this will also toggle the underlying
    // setting-toggle-button associated with this pref.
    this.setPrefValue('cros.device.peripheral_data_access_enabled', true);
    this.$$('#warningDialog').close();
  },

  /** @private */
  onCancelButtonClicked_() {
    this.$$('#warningDialog').close();
  },
});