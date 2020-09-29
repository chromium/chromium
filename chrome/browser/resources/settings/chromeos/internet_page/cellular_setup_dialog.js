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
     * @private {!cellular_setup.CellularSetupDelegate}
     */
    delegate_: Object
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

  onExitCellularSetup_() {
    this.$.dialog.close();
  }

});
