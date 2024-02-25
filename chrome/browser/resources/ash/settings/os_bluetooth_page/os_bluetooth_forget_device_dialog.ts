// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Forget dialog is used to forget a Bluetooth device.
 */
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {getDeviceNameUnsafe} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_bluetooth_forget_device_dialog.html.js';

interface SettingsBluetoothForgetDeviceDialogElement {
  $: {dialog: CrDialogElement};
}

const SettingsBluetoothForgetDeviceDialogElementBase =
    I18nMixin(PolymerElement);

class SettingsBluetoothForgetDeviceDialogElement extends
    SettingsBluetoothForgetDeviceDialogElementBase {
  static get is() {
    return 'os-settings-bluetooth-forget-device-dialog' as const;
  }
  static get template() {
    return getTemplate();
  }
  static get properties() {
    return {
      device_: {
        type: Object,
      },
    };
  }

  private device_: PairedBluetoothDeviceProperties;

  private getForgetDeviceDialogBodyText_(): string {
    return loadTimeData.getStringF(
        'bluetoothDevicesDialogLabel', getDeviceNameUnsafe(this.device_),
        loadTimeData.getString('primaryUserEmail'));
  }

  private onForgetClick_(event: Event): void {
    const fireEvent = new CustomEvent(
        'forget-bluetooth-device', {bubbles: true, composed: true});
    this.dispatchEvent(fireEvent);
    this.$.dialog.close();
    event.stopPropagation();
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothForgetDeviceDialogElement.is]:
        SettingsBluetoothForgetDeviceDialogElement;
  }
}

customElements.define(
    SettingsBluetoothForgetDeviceDialogElement.is,
    SettingsBluetoothForgetDeviceDialogElement);
