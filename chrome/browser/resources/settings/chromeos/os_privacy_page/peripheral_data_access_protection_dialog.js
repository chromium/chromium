// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and warns users of the expected outcome
 * when disabling peripheral data access setup.
 */

const DISABLE_INDETERMINATE_TIMEOUT_MS = 5000;

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

    /** @private */
    resetPrefState_: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * Closes the warning dialog and transitions to the disabling dialog.
   * @private
   */
  onDisableClicked_() {
    this.resetPrefState_ = false;
    this.showDisablingDialog_ = true;

    setTimeout(() => {
      this.$$('#warningDialog').close();
    }, DISABLE_INDETERMINATE_TIMEOUT_MS);
  },

  /** @private */
  onCancelButtonClicked_() {
    this.$$('#warningDialog').close();
    this.handleDialogClosed_();
  },

  /** @private */
  handleDialogClosed_() {
    // If we're closing the dialog because we're advancing to the disabling
    // dialog, do not flip the pref state.
    if (this.resetPrefState_) {
      this.setPrefValue('cros.device.peripheral_data_access_enabled', true);
    }
  }
});