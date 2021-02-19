// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and warns users of the expected outcome
 * when disabling peripheral data access setup.
 */

const DISABLE_INDETERMINATE_TIMEOUT_MS = 3000;

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

    /** @private */
    showDisablingDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Closes the warning dialog and transitions to the disabling dialog.
   * @private
   */
  onDisableClicked_() {
    this.showDisablingDialog_ = true;

    // Send the new state immediately but display a timed spinner dialog
    // to indicate to users that enabling this state may take a few seconds.
    this.setPrefValue('cros.device.peripheral_data_access_enabled', true);
    setTimeout(() => {
      this.$$('#warningDialog').close();
    }, DISABLE_INDETERMINATE_TIMEOUT_MS);
  },

  /** @private */
  onCancelButtonClicked_() {
    this.$$('#warningDialog').close();
  },
});