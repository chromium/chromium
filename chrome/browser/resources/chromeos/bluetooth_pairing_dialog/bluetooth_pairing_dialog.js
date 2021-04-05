// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_dialog.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_page_host_style_css.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'bluetooth-dialog-host' is used to host a <bluetooth-dialog> element to
 * manage bluetooth pairing. The device properties are provided in the
 * chrome 'dialogArguments' variable. When created (attached) the dialog
 * connects to the specified device and passes the results to the
 * bluetooth-dialog element to display.
 */

Polymer({
  is: 'bluetooth-pairing-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Current Pairing device.
     * @type {!chrome.bluetooth.Device|undefined}
     * @private
     */
    pairingDevice_: Object,
  },

  /** @override */
  attached() {
    let dialogArgs = chrome.getVariableValue('dialogArguments');
    if (!dialogArgs) {
      // This situation currently only occurs if the user navigates to the debug
      // chrome://bluetooth-pairing.
      console.warn('No arguments were provided to the dialog.');
      this.$.deviceDialog.open();
      return;
    }

    let parsedDialogArgs = JSON.parse(dialogArgs);
    this.connect_(parsedDialogArgs.address);
  },

  /**
   * @param {!string} address The address of the pairing device.
   * @private
   */
  connect_(address) {
    this.$.deviceDialog.open();

    chrome.bluetooth.getDevice(address, device => {
      this.pairingDevice_ = device;
      chrome.bluetoothPrivate.connect(address, result => {
        var dialog = this.$.deviceDialog;
        dialog.endConnectionAttempt(
            this.pairingDevice_, true /* wasPairing */,
            chrome.runtime.lastError, result);
      });
    });
  },

  /** @private */
  onDialogClose_() {
    chrome.send('dialogClose');
  },
});
