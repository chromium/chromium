// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-add-port-dialog' is a component enabling a
 * user to start forwarding a different port by filling in the appropriate
 * fields and clicking add.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import '../settings_shared.css.js';
import '../guest_os/guest_os_container_select.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast} from '../assert_extras.js';
import {ContainerInfo, GuestId} from '../guest_os/guest_os_browser_proxy.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CROSTINI_GUEST_ID, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_port_forwarding_add_port_dialog.html.js';

interface CrostiniPortForwardingAddPortDialog {
  $: {
    continue: CrButtonElement,
    dialog: CrDialogElement,
    portLabelInput: CrInputElement,
    portNumberInput: CrInputElement,
  };
}

class CrostiniPortForwardingAddPortDialog extends PolymerElement {
  static get is() {
    return 'settings-crostini-add-port-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      inputPortNumber_: {
        type: Number,
        value: null,
      },

      inputPortLabel_: {
        type: String,
        value: '',
      },

      inputProtocolIndex_: {
        type: Number,
        value: 0,  // Default: TCP
      },

      portState_: {
        type: String,
        value: PortState.VALID,
      },

      containerId_: {
        type: Object,
        value() {
          return DEFAULT_CROSTINI_GUEST_ID;
        },
      },

      defaultVmName_: {
        type: String,
        value: DEFAULT_CROSTINI_VM,
      },

      /**
       * List of ports that are already stored in the settings.
       */
      allPorts: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * List of containers that are already stored in the settings.
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

  allContainers: ContainerInfo[];
  allPorts: CrostiniPortSetting[];
  private browserProxy_: CrostiniBrowserProxy;
  private containerId_: GuestId;
  private defaultVmName_: string;
  private inputPortLabel_: string;
  private inputPortNumber_: number|null;
  private inputProtocolIndex_: number;
  private portState_: string;

  constructor() {
    super();

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.$.dialog.showModal();
    microTask.run(() => {
      this.$.portNumberInput.focus();
    });
  }

  private resetInputs_(): void {
    this.inputPortLabel_ = '';
    this.inputPortNumber_ = null;
    this.inputProtocolIndex_ = 0;
    this.portState_ = PortState.VALID;
  }

  get portNumberInput(): CrInputElement {
    return this.$.portNumberInput;
  }

  get portLabelInput(): CrInputElement {
    return this.$.portLabelInput;
  }

  /**
   * @param input The port input to verify.
   * @return if the input string is a valid port number.
   */
  isValidPortNumber(input: string): boolean {
    const numberRegex = /^[0-9]+$/;
    return Boolean(input.match(numberRegex)) &&
        Number(input) >= MIN_VALID_PORT_NUMBER &&
        Number(input) <= MAX_VALID_PORT_NUMBER;
  }

  private computePortState_(): string {
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

  private onSelectProtocol_(e: Event): void {
    this.inputProtocolIndex_ = cast(e.target, HTMLSelectElement).selectedIndex;
    this.portState_ = this.computePortState_();
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
    this.resetInputs_();
  }

  private onAddClick_(): void {
    this.portState_ = this.computePortState_();
    if (this.portState_ !== PortState.VALID) {
      return;
    }
    const portNumber = +this.$.portNumberInput.value;
    const portLabel = this.$.portLabelInput.value;
    this.browserProxy_
        .addCrostiniPortForward(
            this.containerId_, portNumber,
            this.inputProtocolIndex_ as CrostiniPortProtocol, portLabel)
        .then((_result) => {
          // TODO(crbug.com/41391957): Error handling for result
          this.$.dialog.close();
        });
    this.resetInputs_();
  }

  private onBlur_(): void {
    this.portState_ = this.computePortState_();
  }

  private onPortStateChanged_(): void {
    if (this.portState_ === PortState.VALID) {
      this.$.portNumberInput.invalid = false;
      this.$.continue.disabled = false;
      return;
    }
    this.$.portNumberInput.invalid = true;
    this.$.continue.disabled = true;
  }

  private showContainerSelect_(allContainers: ContainerInfo[]): boolean {
    return allContainers.length > 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-add-port-dialog': CrostiniPortForwardingAddPortDialog;
  }
}

customElements.define(
    CrostiniPortForwardingAddPortDialog.is,
    CrostiniPortForwardingAddPortDialog);
