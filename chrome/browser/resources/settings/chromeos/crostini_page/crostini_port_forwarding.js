// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-port-forwarding' is the settings port forwarding subpage for
 * Crostini.
 */
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './crostini_port_forwarding_add_port_dialog.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_section.js';
import '../../settings_page_styles.css.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContainerInfo, GuestId} from '../guest_os/guest_os_browser_proxy.js';
import {containerLabel, equalContainerId} from '../guest_os/guest_os_container_select.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniPortActiveSetting, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CONTAINER_ID, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {PrefsBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const CrostiniPortForwardingBase =
    mixinBehaviors([PrefsBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
class CrostiniPortForwardingElement extends CrostiniPortForwardingBase {
  static get is() {
    return 'settings-crostini-port-forwarding';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
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
        notify: true,
        value() {
          return [];
        },
      },

      /**
       * The known ContainerIds for display in the UI.
       * @private {!Array<!ContainerInfo>}
       */
      allContainers_: {
        type: Array,
        notify: true,
        value() {
          return [];
        },
      },
    };
  }

  static get observers() {
    return [
      'onCrostiniPortsChanged_(prefs.crostini.port_forwarding.ports.value)',
    ];
  }

  constructor() {
    super();
    /**
     * List of ports are currently being forwarded.
     * @private {!Array<?CrostiniPortActiveSetting>}
     */
    this.activePorts_ = [];

    /**
     * Tracks the last port that was selected for removal.
     * @private {?CrostiniPortActiveSetting}
     */
    this.lastMenuOpenedPort_ = null;

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.addWebUIListener(
        'crostini-port-forwarder-active-ports-changed',
        (ports) => this.onCrostiniPortsActiveStateChanged_(ports));
    this.addWebUIListener(
        'crostini-container-info', (infos) => this.onContainerInfo_(infos));
    this.browserProxy_.getCrostiniActivePorts().then(
        (ports) => this.onCrostiniPortsActiveStateChanged_(ports));
    this.browserProxy_.requestContainerInfo();
  }

  /**
   * @param {!CrostiniPortProtocol} protocol
   * @private
   */
  getProtocolName(protocol) {
    return Object.keys(CrostiniPortProtocol)
        .find(k => CrostiniPortProtocol[k] === protocol);
  }

  /**
   * @param {!Array<!ContainerInfo>} containerInfos
   */
  onContainerInfo_(containerInfos) {
    this.set('allContainers_', containerInfos);
  }

  /**
   * @param {!Array<!CrostiniPortSetting>} ports List of ports.
   * @private
   */
  onCrostiniPortsChanged_(ports) {
    this.splice('allPorts_', 0, this.allPorts_.length);
    for (const port of ports) {
      port.is_active = this.activePorts_.some(
          activePort => activePort.port_number === port.port_number &&
              activePort.protocol_type === port.protocol_type);
      port.container_id = port.container_id || {
        vm_name: port['vm_name'] || DEFAULT_CROSTINI_VM,
        container_name: port['container_name'] || DEFAULT_CROSTINI_CONTAINER,
      };
      this.push('allPorts_', port);
    }
    this.notifyPath('allContainers_');
  }

  /**
   * @param {!Array<!CrostiniPortActiveSetting>} ports List of ports that are
   *     active.
   * @private
   */
  onCrostiniPortsActiveStateChanged_(ports) {
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
  }

  /**
   * @param {!Event} event
   * @private
   */
  onAddPortClick_(event) {
    this.showAddPortDialog_ = true;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onAddPortDialogClose_(event) {
    this.showAddPortDialog_ = false;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onShowRemoveAllPortsMenuClick_(event) {
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeAllPortsMenu.get());
    menu.showAt(/** @type {!HTMLElement} */ (event.target));
  }

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveSinglePortClick_(event) {
    const dataSet = /** @type {{portNumber: string, protocolType: string}} */
        (event.currentTarget.dataset);
    const containerId = /** @type {GuestId} */
        (event.currentTarget['dataContainerId']);
    const protocolType = /** @type {!CrostiniPortProtocol} */
        (Number(dataSet.protocolType));

    this.browserProxy_
        .removeCrostiniPortForward(
            containerId, Number(dataSet.portNumber), protocolType)
        .then(result => {
          // TODO(crbug.com/848127): Error handling for result
          recordSettingChange();
        });
  }

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveAllPortsClick_(event) {
    const menu = /** @type {!CrActionMenuElement} */
        (this.$.removeAllPortsMenu.get());
    assert(menu.open);
    for (const container of this.allContainers_) {
      this.browserProxy_.removeAllCrostiniPortForwards(container.id);
    }
    recordSettingChange();
    menu.close();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onPortActivationChange_(event) {
    const dataSet = /** @type {{portNumber: string, protocolType: string}} */
        (event.currentTarget.dataset);
    const id = /** @type {GuestId} */
        (event.currentTarget['dataContainerId']);
    const portNumber = Number(dataSet.portNumber);
    const protocolType = /** @type {!CrostiniPortProtocol} */
        (Number(dataSet.protocolType));
    if (event.target.checked) {
      event.target.checked = false;
      this.browserProxy_
          .activateCrostiniPortForward(id, portNumber, protocolType)
          .then(result => {
            if (!result) {
              this.$.errorToast.show();
            }
            // TODO(crbug.com/848127): Elaborate on error handling for result
          });
    } else {
      this.browserProxy_
          .deactivateCrostiniPortForward(id, portNumber, protocolType)
          .then(
              result => {
                  // TODO(crbug.com/848127): Error handling for result
              });
    }
  }

  /**
   * @param {!Array<!CrostiniPortSetting>} allPorts
   * @param {!GuestId} id
   * @return boolean
   * @private
   */
  showContainerId_(allPorts, id) {
    return allPorts.some(port => equalContainerId(port.container_id, id)) &&
        allPorts.some(
            port => !equalContainerId(port.container_id, DEFAULT_CONTAINER_ID));
  }

  /**
   * @param {!GuestId} id
   * @return string
   * @private
   */
  containerLabel_(id) {
    return this.showContainerId_(this.allPorts_, id) ?
        containerLabel(id, DEFAULT_CROSTINI_VM) :
        '';
  }

  /**
   * @param {!Array<!CrostiniPortSetting>} allPorts
   * @param {!GuestId} id
   * @return boolean
   * @private
   */
  hasContainerPorts_(allPorts, id) {
    return allPorts.some(port => equalContainerId(port.container_id, id));
  }

  /**
   * @param {!GuestId} id
   * @return function
   * @private
   */
  byContainerId_(id) {
    return port => equalContainerId(port.container_id, id);
  }
}

customElements.define(
    CrostiniPortForwardingElement.is, CrostiniPortForwardingElement);
