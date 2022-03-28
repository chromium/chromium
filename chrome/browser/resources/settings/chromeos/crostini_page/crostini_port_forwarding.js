// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-port-forwarding' is the settings port forwarding subpage for
 * Crostini.
 */
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './crostini_port_forwarding_add_port_dialog.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {PrefsBehavior} from '../prefs_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-crostini-port-forwarding',

  behaviors: [PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether Crostini is running.
     * @private {boolean}
     */
    crostiniRunning_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showAddPortDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * The forwarded ports for display in the UI.
     * @private {!Array<!CrostiniPortSetting>}
     */
    allPorts_: {
      type: Array,
      value() {
        return [];
      },
    },
  },

  /**
   * List of ports are currently being forwarded.
   * @private {!Array<?CrostiniPortActiveSetting>}
   */
  activePorts_: new Array(),

  /**
   * Tracks the last port that was selected for removal.
   * @private {?CrostiniPortActiveSetting}
   */
  lastMenuOpenedPort_: null,

  /** @override */
  attached() {
    this.addWebUIListener(
        'crostini-port-forwarder-active-ports-changed',
        (ports) => this.onCrostiniPortsActiveStateChanged_(ports));
    this.addWebUIListener(
        'crostini-status-changed',
        (isRunning) => this.onCrostiniIsRunningStateChanged_(isRunning));
    CrostiniBrowserProxyImpl.getInstance().getCrostiniActivePorts().then(
        (ports) => this.onCrostiniPortsActiveStateChanged_(ports));
    CrostiniBrowserProxyImpl.getInstance().checkCrostiniIsRunning().then(
        (isRunning) => this.onCrostiniIsRunningStateChanged_(isRunning));
  },

  observers:
      ['onCrostiniPortsChanged_(prefs.crostini.port_forwarding.ports.value)'],

  /**
   * @param {!CrostiniPortProtocol} protocol
   * @private
   */
  getProtocolName(protocol) {
    return Object.keys(CrostiniPortProtocol)
        .find(k => CrostiniPortProtocol[k] === protocol);
  },

  /**
   * @param {boolean} isRunning boolean indicating if Crostini is running.
   * @private
   */
  onCrostiniIsRunningStateChanged_: function(isRunning) {
    this.crostiniRunning_ = isRunning;
  },

  /**
   * @param {!Array<!CrostiniPortSetting>} ports List of ports.
   * @private
   */
  onCrostiniPortsChanged_: function(ports) {
    this.splice('allPorts_', 0, this.allPorts_.length);
    for (const port of ports) {
      port.is_active = this.activePorts_.some(
          activePort => activePort.port_number === port.port_number &&
              activePort.protocol_type === port.protocol_type);
      this.push('allPorts_', port);
    }
  },

  /**
   * @param {!Array<!CrostiniPortActiveSetting>} ports List of ports that are
   *     active.
   * @private
   */
  onCrostiniPortsActiveStateChanged_: function(ports) {
    this.activePorts_ = ports;
    for (let i = 0; i < this.allPorts_.length; i++) {
      this.set(
          `allPorts_.${i}.${'is_active'}`,
          this.activePorts_.some(
              activePort =>
                  activePort.port_number === this.allPorts_[i].port_number &&
                  activePort.protocol_type ===
                      this.allPorts_[i].protocol_type));
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAddPortClick_: function(event) {
    this.showAddPortDialog_ = true;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAddPortDialogClose_: function(event) {
    this.showAddPortDialog_ = false;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onShowRemoveAllPortsMenuClick_: function(event) {
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeAllPortsMenu.get());
    menu.showAt(/** @type {!HTMLElement} */ (event.target));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onShowRemoveSinglePortMenuClick_: function(event) {
    const dataSet = /** @type {{portNumber: string, protocolType: string}} */
        (event.currentTarget.dataset);
    this.lastMenuOpenedPort_ = {
      port_number: Number(dataSet.portNumber),
      protocol_type: /** @type {!CrostiniPortProtocol} */
          (Number(dataSet.protocolType))
    };
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeSinglePortMenu.get());
    menu.showAt(/** @type {!HTMLElement} */ (event.target));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveSinglePortClick_: function(event) {
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeSinglePortMenu.get());
    assert(
        menu.open && this.lastMenuOpenedPort_.port_number != null &&
        this.lastMenuOpenedPort_.protocol_type != null);
    CrostiniBrowserProxyImpl.getInstance()
        .removeCrostiniPortForward(
            DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER,
            this.lastMenuOpenedPort_.port_number,
            this.lastMenuOpenedPort_.protocol_type)
        .then(result => {
          // TODO(crbug.com/848127): Error handling for result
          recordSettingChange();
          menu.close();
        });
    this.lastMenuOpenedPort_ = null;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveAllPortsClick_: function(event) {
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeAllPortsMenu.get());
    assert(menu.open);
    CrostiniBrowserProxyImpl.getInstance().removeAllCrostiniPortForwards(
        DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER);
    recordSettingChange();
    menu.close();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onPortActivationChange_: function(event) {
    const dataSet = /** @type {{portNumber: string, protocolType: string}} */
        (event.currentTarget.dataset);
    const portNumber = Number(dataSet.portNumber);
    const protocolType = /** @type {!CrostiniPortProtocol} */
        (Number(dataSet.protocolType));
    if (event.target.checked) {
      event.target.checked = false;
      CrostiniBrowserProxyImpl.getInstance()
          .activateCrostiniPortForward(
              DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER, portNumber,
              protocolType)
          .then(result => {
            if (!result) {
              this.$.errorToast.show();
            }
            // TODO(crbug.com/848127): Elaborate on error handling for result
          });
    } else {
      CrostiniBrowserProxyImpl.getInstance()
          .deactivateCrostiniPortForward(
              DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER, portNumber,
              protocolType)
          .then(
              result => {
                  // TODO(crbug.com/848127): Error handling for result
              });
    }
  },
});
