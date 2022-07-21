// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-add-port-dialog' is a component enabling a
 * user to start forwarding a different port by filling in the appropriate
 * fields and clicking add.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import '../../settings_shared.css.js';
import '../guest_os/guest_os_container_select.js';

import {html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContainerInfo, GuestId} from '../guest_os/guest_os_browser_proxy.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CONTAINER_ID, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';

/** @polymer */
class CrostiniPortForwardingAddPortDialog extends PolymerElement {
  static get is() {
    return 'settings-crostini-add-port-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private {?number}
       */
      inputPortNumber_: {
        type: Number,
        value: null,
      },

      /**
       * @private {string}
       */
      inputPortLabel_: {
        type: String,
        value: '',
      },

      /**
       * @private {number}
       */
      inputProtocolIndex_: {
        type: Number,
        value: 0,  // Default: TCP
      },

      /**
       * @private {!PortState}
       */
      portState_: {
        type: String,
        value: PortState.VALID,
      },

      /**
       * @type {!GuestId}
       */
      containerId_: {
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
       * List of ports that are already stored in the settings.
       * @type {!Array<!CrostiniPortSetting>}
       */
      allPorts: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * List of containers that are already stored in the settings.
       * @type {!Array<!ContainerInfo>}
       */
      allContainers: {
        type: Array,
        value: [],
      },
    };
  }

  static get observers() {
    return [
      'onPortStateChanged_(portState_)',
    ];
  }

  constructor() {
    super();

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
    microTask.run(() => {
      this.$.portNumberInput.focus();
    });
  }

  resetInputs_() {
    this.inputPortLabel_ = '';
    this.inputPortNumber_ = null;
    this.inputProtocolIndex_ = 0;
    this.portState_ = PortState.VALID;
  }

  /**
   * @return {!CrInputElement} input for the port number.
   */
  get portNumberInput() {
    return /** @type{!CrInputElement} */ (this.$.portNumberInput);
  }

  /**
   * @return {!CrInputElement} input for the optional port label.
   */
  get portLabelInput() {
    return /** @type{!CrInputElement} */ (this.$.portLabelInput);
  }

  /**
   * @param {string} input The port input to verify.
   * @return {?boolean} if the input string is a valid port number.
   */
  isValidPortNumber(input) {
    const numberRegex = /^[0-9]+$/;
    return input.match(numberRegex) && Number(input) >= MIN_VALID_PORT_NUMBER &&
        Number(input) <= MAX_VALID_PORT_NUMBER;
  }

  /**
   * @return {!PortState}
   * @private
   */
  computePortState_() {
    if (!this.isValidPortNumber(this.$.portNumberInput.value)) {
      return PortState.INVALID;
    }
    if (this.allPorts.find(
            portSetting => portSetting.port_number ===
                    Number(this.$.portNumberInput.value) &&
                portSetting.protocol_type === this.inputProtocolIndex_)) {
      return PortState.DUPLICATE;
    }
    return PortState.VALID;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onSelectProtocol_(e) {
    this.inputProtocolIndex_ = e.target.selectedIndex;
    this.portState_ = this.computePortState_();
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
    this.resetInputs_();
  }

  /** @private */
  onAddTap_() {
    this.portState_ = this.computePortState_();
    if (this.portState_ !== PortState.VALID) {
      return;
    }
    const portNumber = +this.$.portNumberInput.value;
    const portLabel = this.$.portLabelInput.value;
    this.browserProxy_
        .addCrostiniPortForward(
            this.containerId_, portNumber,
            /** @type {!CrostiniPortProtocol} */ (this.inputProtocolIndex_),
            portLabel)
        .then(result => {
          // TODO(crbug.com/848127): Error handling for result
          this.$.dialog.close();
        });
    this.resetInputs_();
  }

  /** @private */
  onBlur_() {
    this.portState_ = this.computePortState_();
  }

  /** @private */
  onPortStateChanged_() {
    if (this.portState_ === PortState.VALID) {
      this.$.portNumberInput.invalid = false;
      this.$.continue.disabled = false;
      return;
    }
    this.$.portNumberInput.invalid = true;
    this.$.continue.disabled = true;
  }

  /**
   * @param {!Array<!ContainerInfo>} allContainers
   * @return boolean
   * @private
   */
  showContainerSelect_(allContainers) {
    return allContainers.length > 1;
  }
}

customElements.define(
    CrostiniPortForwardingAddPortDialog.is,
    CrostiniPortForwardingAddPortDialog);
