// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element for displaying Bluetooth pairing dialog.
 */
import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_ui.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {BluetoothUiSurface, recordBluetoothUiSurfaceMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_bluetooth_pairing_dialog.html.js';

export interface SettingsBluetoothPairingDialogElement {
  $: {dialog: CrDialogElement};
}

export class SettingsBluetoothPairingDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-bluetooth-pairing-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    recordBluetoothUiSurfaceMetrics(BluetoothUiSurface.SETTINGS_PAIRING_DIALOG);
  }

  private closeDialog_(e: Event): void {
    this.$.dialog.close();
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothPairingDialogElement.is]:
        SettingsBluetoothPairingDialogElement;
  }
}

customElements.define(
    SettingsBluetoothPairingDialogElement.is,
    SettingsBluetoothPairingDialogElement);
