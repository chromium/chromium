// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-export-import' is the settings backup and restore subpage for
 * Crostini.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './crostini_import_confirmation_dialog.js';
import '../../settings_shared_css.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsCrostiniExportImportElementBase = mixinBehaviors(
    [DeepLinkingBehavior, RouteObserverBehavior, WebUIListenerBehavior],
    PolymerElement);

/** @polymer */
class SettingsCrostiniExportImportElement extends
    SettingsCrostiniExportImportElementBase {
  static get is() {
    return 'settings-crostini-export-import';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
        computed:
            'setEnableButtons_(installerShowing_, exportImportInProgress_)',
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
    };
  }

  constructor() {
    super();

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'crostini-export-import-operation-status-changed', inProgress => {
          this.exportImportInProgress_ = inProgress;
        });
    this.addWebUIListener(
        'crostini-installer-status-changed', installerShowing => {
          this.installerShowing_ = installerShowing;
        });

    this.browserProxy_.requestCrostiniExportImportOperationStatus();
    this.browserProxy_.requestCrostiniInstallerStatus();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.CROSTINI_EXPORT_IMPORT) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  onExportClick_() {
    this.browserProxy_.exportCrostiniContainer({
      vm_name: DEFAULT_CROSTINI_VM,
      container_name: DEFAULT_CROSTINI_CONTAINER
    });
  }

  /** @private */
  onImportClick_() {
    this.showImportConfirmationDialog_ = true;
  }

  /** @private */
  onImportConfirmationDialogClose_() {
    this.showImportConfirmationDialog_ = false;
  }

  /**
   * @param {boolean} installerShowing
   * @param {boolean} exportImportInProgress
   * @private
   */
  setEnableButtons_(installerShowing, exportImportInProgress) {
    return !(installerShowing || exportImportInProgress);
  }
}

customElements.define(
    SettingsCrostiniExportImportElement.is,
    SettingsCrostiniExportImportElement);
