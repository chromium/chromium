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

  behaviors: [
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

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
      computed: 'setEnableButtons_(installerShowing_, exportImportInProgress_)',
    },

    /** @private */
    installerShowing_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    exportImportInProgress_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kBackupLinuxAppsAndFiles,
        chromeos.settings.mojom.Setting.kRestoreLinuxAppsAndFiles,
      ]),
    },
  },

  attached() {
    this.addWebUIListener(
        'crostini-export-import-operation-status-changed', inProgress => {
          this.exportImportInProgress_ = inProgress;
        });
    this.addWebUIListener(
        'crostini-installer-status-changed', installerShowing => {
          this.installerShowing_ = installerShowing;
        });

    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniExportImportOperationStatus();
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniInstallerStatus();
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.CROSTINI_EXPORT_IMPORT) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onExportClick_() {
    settings.CrostiniBrowserProxyImpl.getInstance().exportCrostiniContainer();
  },

  /** @private */
  onImportClick_() {
    this.showImportConfirmationDialog_ = true;
  },

  /** @private */
  onImportConfirmationDialogClose_() {
    this.showImportConfirmationDialog_ = false;
  },

  /** @private */
  setEnableButtons_: function(installerShowing, exportImportInProgress) {
    return !(installerShowing || exportImportInProgress);
  },
});
