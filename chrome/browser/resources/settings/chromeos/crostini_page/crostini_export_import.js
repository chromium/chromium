// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-export-import' is the settings backup and restore subpage for
 * Crostini.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './crostini_import_confirmation_dialog.js';
import '../../settings_shared.css.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {ContainerInfo, GuestId} from '../guest_os/guest_os_browser_proxy.js';
import {equalContainerId} from '../guest_os/guest_os_container_select.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, DEFAULT_CONTAINER_ID, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';

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
            'isEnabledButtons_(installerShowing_, exportImportInProgress_)',
      },

      /**
       * Whether the container select element is displayed.
       * @private {boolean}
       */
      showContainerSelect_: {
        type: Boolean,
        computed: 'isMultiContainer_(allContainers_)',
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
       * The known containers for display in the UI.
       * @private {!Array<!ContainerInfo>}
       */
      allContainers_: {
        type: Array,
        notify: true,
        value() {
          return [];
        },
      },

      /**
       * The GuestId of the container to be exported.
       * @private {!GuestId}
       */
      exportContainerId_: {
        type: Object,
        value() {
          return DEFAULT_CONTAINER_ID;
        },
      },

      /**
       * The GuestId of the container to be overwritten by an imported
       * container file.
       * @private {!GuestId}
       */
      importContainerId_: {
        type: Object,
        value() {
          return DEFAULT_CONTAINER_ID;
        },
      },

      /** @private {string} */
      defaultVmName_: {
        type: String,
        value: DEFAULT_CROSTINI_VM,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kBackupLinuxAppsAndFiles,
          Setting.kRestoreLinuxAppsAndFiles,
        ]),
      },
    };
  }

  constructor() {
    super();

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  /** @override */
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
    this.addWebUIListener(
        'crostini-container-info', (infos) => this.onContainerInfo_(infos));

    this.browserProxy_.requestCrostiniExportImportOperationStatus();
    this.browserProxy_.requestCrostiniInstallerStatus();
    this.browserProxy_.requestContainerInfo();
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

  /**
   * @param {!Array<!ContainerInfo>} containerInfos
   * @private
   */
  onContainerInfo_(containerInfos) {
    this.allContainers_ = containerInfos;
    if (!this.isMultiContainer_(containerInfos)) {
      this.exportContainerId_ = DEFAULT_CONTAINER_ID;
      this.importContainerId_ = DEFAULT_CONTAINER_ID;
    }
  }

  /** @private */
  onExportClick_() {
    this.browserProxy_.exportCrostiniContainer(this.exportContainerId_);
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
   * @param {!Boolean} installerShowing
   * @param {!Boolean} exportImportInProgress
   * @private
   */
  isEnabledButtons_(installerShowing, exportImportInProgress) {
    return !(installerShowing || exportImportInProgress);
  }


  /**
   * @param {!Array<!ContainerInfo>} allContainers
   * @return boolean
   * @private
   */
  isMultiContainer_(allContainers) {
    return !(
        allContainers.length === 1 &&
        equalContainerId(allContainers[0].id, DEFAULT_CONTAINER_ID));
  }

  getSettingsBoxClass_(allContainers) {
    return this.isMultiContainer_(allContainers) ? 'two-line-settings-box' : '';
  }
}

customElements.define(
    SettingsCrostiniExportImportElement.is,
    SettingsCrostiniExportImportElement);
