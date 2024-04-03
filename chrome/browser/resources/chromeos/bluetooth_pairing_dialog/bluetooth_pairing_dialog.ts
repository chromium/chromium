// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_ui.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import './strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_pairing_dialog.html.js';

/**
 * @fileoverview
 * 'bluetooth-pairing-dialog' is used to host a
 * <bluetooth-pairing-ui> element to manage bluetooth pairing. The dialog
 * arguments are provided in the chrome 'dialogArguments' variable.
 */

class BluetoothPairingDialogElement extends PolymerElement {
  static get is() {
    return 'bluetooth-pairing-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The address, when set, of the specific device that will be attempted
       * to be paired with by the pairing dialog. If null, no specific device
       * will be paired with and the user will be allowed to select a device to
       * pair with.
       */
      deviceAddress_: {
        type: String,
        value: null,
      },

      /**
       * Flag indicating whether links should be displayed or not. In some
       * cases, such as the user being in OOBE or the login screen, links will
       * not work and should not be displayed.
       */
      shouldOmitLinks_: {
        type: Boolean,
        value: false,
      },

    };
  }

  private deviceAddress_: string;
  private shouldOmitLinks_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    ColorChangeUpdater.forDocument().start();

    const dialogArgs = chrome.getVariableValue('dialogArguments');
    if (!dialogArgs) {
      return;
    }

    const parsedDialogArgs = JSON.parse(dialogArgs);
    if (!parsedDialogArgs) {
      return;
    }

    this.deviceAddress_ = parsedDialogArgs.address;
    this.shouldOmitLinks_ = !!parsedDialogArgs.shouldOmitLinks;
  }

  private closeDialog_(): void {
    chrome.send('dialogClose');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [BluetoothPairingDialogElement.is]: BluetoothPairingDialogElement;
  }
}

customElements.define(
    BluetoothPairingDialogElement.is, BluetoothPairingDialogElement);
