// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-export-import' is the settings backup and restore subpage for
 * Crostini.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import './crostini_import_confirmation_dialog.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-crostini-export-import',

  behaviors: [
    DeepLinkingBehavior,
    RouteObserverBehavior,
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

    CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniExportImportOperationStatus();
    CrostiniBrowserProxyImpl.getInstance().requestCrostiniInstallerStatus();
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.CROSTINI_EXPORT_IMPORT) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onExportClick_() {
    CrostiniBrowserProxyImpl.getInstance().exportCrostiniContainer();
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
