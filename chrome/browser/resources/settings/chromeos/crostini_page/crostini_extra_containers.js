// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-extra-containers' is the settings extras containers subpage for
 * Crostini.
 */
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './crostini_extra_containers_create_dialog.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContainerInfo, GuestId} from '../guest_os/guest_os_browser_proxy.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {WebUIListenerBehaviorInterface}
 */
const ExtraContainersElementBase =
    mixinBehaviors([WebUIListenerBehavior], PolymerElement);

/** @polymer */
class ExtraContainersElement extends ExtraContainersElementBase {
  static get is() {
    return 'settings-crostini-extra-containers';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      showCreateContainerDialog_: {
        type: Boolean,
        value: false,
      },

      allContainers_: {
        type: Array,
        notify: true,
        value() {
          return [];
        },
      },

      lastMenuContainerInfo_: {
        type: Object,
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

      /** @private */
      installerShowing_: {
        type: Boolean,
        value: false,
      },

      // TODO(b/231890242): Disable delete and stop buttons when a container is
      // being exported or imported.
      /** @private */
      exportImportInProgress_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    /**
     * Tracks the last container that was selected for delete.
     * @private {?ContainerInfo}
     */
    this.lastMenuContainerInfo_ = null;

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();
    this.addWebUIListener(
        'crostini-container-info', (infos) => this.onContainerInfo_(infos));
    this.browserProxy_.requestContainerInfo();
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

    this.browserProxy_.requestCrostiniExportImportOperationStatus();
    this.browserProxy_.requestCrostiniInstallerStatus();
  }

  /**
   * @param {!Array<!ContainerInfo>} containerInfos
   */
  onContainerInfo_(containerInfos) {
    this.set('allContainers_', containerInfos);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onCreateClick_(event) {
    this.showCreateContainerDialog_ = true;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onCreateContainerDialogClose_(event) {
    this.showCreateContainerDialog_ = false;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onContainerMenuClick_(event) {
    const id =
        /** @type {GuestId} */ (event.currentTarget['dataContainerId']);
    this.lastMenuContainerInfo_ = this.allContainers_.find(
        e => e.id.vm_name === id.vm_name &&
            e.id.container_name === id.container_name);
    this.getContainerMenu_().showAt(/** @type {!HTMLElement} */ (event.target));
  }

  /**
   * @param {!Event} event
   * @private
   */
  onDeleteContainerClick_(event) {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.deleteContainer(this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onStopContainerClick_(event) {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.stopContainer(this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onExportContainerClick_(event) {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.exportCrostiniContainer(
          this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onImportContainerClick_(event) {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.importCrostiniContainer(
          this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onContainerColorChange_(event) {
    const containerId =
        /** @type {GuestId} */ (event.currentTarget['dataContainerId']);

    this.browserProxy_.setContainerBadgeColor(
        containerId, hexColorToSkColor(event.target.value));
  }

  /**
   * @param {!ContainerInfo} info
   * @private
   */
  shouldDisableDeleteContainer_(info) {
    return info && info.id.vm_name === DEFAULT_CROSTINI_VM &&
        info.id.container_name === DEFAULT_CROSTINI_CONTAINER;
  }

  /**
   * @param {!ContainerInfo} info
   * @private
   */
  shouldDisableStopContainer_(info) {
    return !info || !info.ipv4;
  }

  /**
   * @return {!CrActionMenuElement}
   * @private
   */
  getContainerMenu_() {
    return /** @type {!CrActionMenuElement} */ (this.$.containerMenu.get());
  }

  /**
   * @private
   */
  closeContainerMenu_() {
    const menu = this.getContainerMenu_();
    assert(menu.open && this.lastMenuContainerInfo_);
    menu.close();
    this.lastMenuContainerInfo_ = null;
  }

  /**
   * @param {!Boolean} installerShowing
   * @param {!Boolean} exportImportInProgress
   * @private
   */
  isEnabledButtons_(installerShowing, exportImportInProgress) {
    return !(installerShowing || exportImportInProgress);
  }
}

customElements.define(ExtraContainersElement.is, ExtraContainersElement);
