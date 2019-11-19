// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-export-import' is the settings backup and restore subpage for
 * Crostini.
 */

Polymer({
  is: 'settings-crostini-export-import',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @private */
    showImportConfirmationDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the export import buttons should be enabled. Initially false
     * until status has been confirmed.
     * @private {boolean}
     */
    enableButtons_: {
      type: Boolean,
      value: false,
    },
  },

  attached: function() {
    this.addWebUIListener(
        'crostini-export-import-operation-status-changed', inProgress => {
          this.enableButtons_ = !inProgress;
        });
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniExportImportOperationStatus();
  },

  /** @private */
  onExportClick_: function() {
    settings.CrostiniBrowserProxyImpl.getInstance().exportCrostiniContainer();
  },

  /** @private */
  onImportClick_: function() {
    this.showImportConfirmationDialog_ = true;
  },

  /** @private */
  onImportConfirmationDialogClose_: function() {
    this.showImportConfirmationDialog_ = false;
  },
});
