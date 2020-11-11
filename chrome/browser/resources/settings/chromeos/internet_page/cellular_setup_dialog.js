// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-cellular-setup-dialog' embeds the <cellular-setup>
 * that is shared with OOBE in a dialog with OS Settings stylizations.
 */
Polymer({
  is: 'os-settings-cellular-setup-dialog',

  properties: {

    /**
     * Name of cellular dialog page to be selected.
     * @type {!cellularSetup.CellularSetupPageName}
     */
    pageName: String,

    /**
     * @private {!cellular_setup.CellularSetupDelegate}
     */
    delegate_: Object,

    /*** @private */
    dialogTitle_: {
      type: String,
      notify: true,
    },
  },

  /** @override */
  created() {
    this.delegate_ = new settings.CellularSetupSettingsDelegate();
  },

  listeners: {
    'exit-cellular-setup': 'onExitCellularSetup_',
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private*/
  onExitCellularSetup_() {
    this.$.dialog.close();
  },

  /**
   * @param {string} title
   * @returns {boolean}
   * @private
   */
  shouldShowDialogTitle_(title) {
    return !!this.dialogTitle_;
  },
});
