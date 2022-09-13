// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_dialog.js';
import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_ui.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import './strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {I18nBehavior} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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

    /** @private */
    isBluetoothRevampEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableBluetoothRevamp');
      },
    },

    /**
     * The address, when set, of the specific device that will be attempted to
     * be paired with by the pairing dialog. If null, no specific device will be
     * paired with and the user will be allowed to select a device to pair with.
     * This is set by the dialog arguments when |isBluetoothRevampEnabled_| is
     * true.
     * @private {?string}
     */
    deviceAddress_: {
      type: String,
      value: null,
    },

    /**
     * Flag indicating whether links should be displayed or not. In some
     * cases, such as the user being in OOBE or the login screen, links will
     * not work and should not be displayed.
     * This is set by the dialog arguments when |isBluetoothRevampEnabled_| is
     * true.
     */
    shouldOmitLinks_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  attached() {
    const dialogArgs = chrome.getVariableValue('dialogArguments');

    if (this.isBluetoothRevampEnabled_) {
      if (!dialogArgs) {
        return;
      }

      const parsedDialogArgs = JSON.parse(dialogArgs);
      if (!parsedDialogArgs) {
        return;
      }

      this.deviceAddress_ = parsedDialogArgs.address;
      this.shouldOmitLinks_ = !!parsedDialogArgs.shouldOmitLinks;
      return;
    }

    if (!dialogArgs) {
      // This situation currently only occurs if the user navigates to the debug
      // chrome://bluetooth-pairing.
      console.warn('No arguments were provided to the dialog.');

      // Wait for next render or deviceDialog has not been created yet.
      afterNextRender(this, () => this.$$('#deviceDialog').open());
      return;
    }

    const parsedDialogArgs = JSON.parse(dialogArgs);

    // Wait for next render or deviceDialog has not been created yet.
    afterNextRender(this, () => this.connect_(parsedDialogArgs.address));
  },

  /**
   * @param {!string} address The address of the pairing device.
   * @private
   */
  connect_(address) {
    this.$$('#deviceDialog').open();

    chrome.bluetooth.getDevice(address, device => {
      this.pairingDevice_ = device;
      chrome.bluetoothPrivate.connect(address, result => {
        var dialog = this.$$('#deviceDialog');
        dialog.endConnectionAttempt(
            this.pairingDevice_, true /* wasPairing */,
            chrome.runtime.lastError, result);
      });
    });
  },

  /** @private */
  closeDialog_() {
    chrome.send('dialogClose');
  },
});
