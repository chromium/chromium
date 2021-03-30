// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-add-port-dialog' is a component enabling a
 * user to start forwarding a different port by filling in the appropriate
 * fields and clicking add.
 */

Polymer({
  is: 'settings-crostini-add-port-dialog',

  properties: {
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
     * List of ports that are already stored in the settings.
     * @type {!Array<!CrostiniPortSetting>}
     */
    allPorts: {
      type: Array,
      value() {
        return [];
      },
    },
  },

  observers: [
    'onPortStateChanged_(portState_)',
  ],

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
    this.async(() => {
      this.$.portNumberInput.focus();
    }, 1);
  },

  resetInputs_: function() {
    this.inputPortLabel_ = '';
    this.inputPortNumber_ = null;
    this.inputProtocolIndex_ = 0;
    this.portState_ = PortState.VALID;
  },

  /**
   * @return {!CrInputElement} input for the port number.
   */
  get portNumberInput() {
    return /** @type{!CrInputElement} */ (this.$.portNumberInput);
  },

  /**
   * @return {!CrInputElement} input for the optional port label.
   */
  get portLabelInput() {
    return /** @type{!CrInputElement} */ (this.$.portLabelInput);
  },

  /**
   * @param {string} input The port input to verify.
   * @return {?boolean} if the input string is a valid port number.
   */
  isValidPortNumber: function(input) {
    const numberRegex = /^[0-9]+$/;
    return input.match(numberRegex) && Number(input) >= MIN_VALID_PORT_NUMBER &&
        Number(input) <= MAX_VALID_PORT_NUMBER;
  },

  /**
   * @return {!PortState}
   * @private
   */
  computePortState_: function() {
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
  },

  /**
   * @param {!Event} e
   * @private
   */
  onSelectProtocol_: function(e) {
    this.inputProtocolIndex_ = e.target.selectedIndex;
    this.portState_ = this.computePortState_();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
    this.resetInputs_();
  },

  /** @private */
  onAddTap_: function() {
    this.portState_ = this.computePortState_();
    if (this.portState_ !== PortState.VALID) {
      return;
    }
    const portNumber = +this.$.portNumberInput.value;
    const portLabel = this.$.portLabelInput.value;
    settings.CrostiniBrowserProxyImpl.getInstance()
        .addCrostiniPortForward(
            DEFAULT_CROSTINI_VM, DEFAULT_CROSTINI_CONTAINER, portNumber,
            /** @type {!CrostiniPortProtocol} */ (this.inputProtocolIndex_),
            portLabel)
        .then(result => {
          // TODO(crbug.com/848127): Error handling for result
          this.$.dialog.close();
        });
    this.resetInputs_();
  },

  /** @private */
  onBlur_: function() {
    this.portState_ = this.computePortState_();
  },

  /** @private */
  onPortStateChanged_: function() {
    if (this.portState_ === PortState.VALID) {
      this.$.portNumberInput.invalid = false;
      this.$.continue.disabled = false;
      return;
    }
    this.$.portNumberInput.invalid = true;
    this.$.continue.disabled = true;
  }
});
