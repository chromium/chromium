// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element to show a list of discovered Bluetooth devices and initiate
 * pairing to a device.
 */
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import './bluetooth_icon.js';

import {FocusRowMixin} from '//resources/ash/common/cr_elements/focus_row_mixin.js';
import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from '//resources/js/assert.js';
import {mojoString16ToString} from '//resources/js/mojo_type_util.js';
import {BluetoothDeviceProperties, DeviceType} from '//resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_pairing_device_item.html.js';
import {DeviceItemState} from './bluetooth_types.js';

const SettingsBluetoothPairingDeviceItemElementBase = FocusRowMixin(I18nMixin(PolymerElement));


export interface SettingsBluetoothPairingDeviceItemElement {
  $: {
    container: HTMLDivElement,
    secondaryLabel: HTMLDivElement,
    textRow: HTMLDivElement,
  };
}

export class SettingsBluetoothPairingDeviceItemElement extends
    SettingsBluetoothPairingDeviceItemElementBase {
  static get is() {
    return 'bluetooth-pairing-device-item'as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      device: Object,

      deviceItemState: {
        type: Object,
        value: DeviceItemState.DEFAULT,
      },

      /** The index of this item in its parent list, used for its a11y label. */
      itemIndex: Number,

      /**
       * The total number of elements in this item's parent list, used for its
       * a11y label.
       */
      listSize: Number,

      secondaryLabel_: {
        type: String,
        computed: 'computeSecondaryLabel_(deviceItemState)',
      },

      pairingFailed_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computePairingFailed_(deviceItemState)',
      },
    };
  }

  device: BluetoothDeviceProperties;
  deviceItemState: DeviceItemState;
  itemIndex: number;
  listSize: number;
  private secondaryLabel_: string;
  private pairingFailed_: boolean;

  override focus(): void {
    // Prevent scroll stops iron list from trying to bring this element to view,
    // if it is the |lastFocused| element and scrolled out of view. This can
    // happen if this element is tabbed to or selected and then scrolled out of
    // view.
    // TODO(b/210743107) Add a test for this.
    this.$.container.focus({preventScroll: true});
  }

  private computePairingFailed_(): boolean {
    return this.deviceItemState === DeviceItemState.FAILED;
  }

  private getDeviceName_(): string {
    if (!this.device) {
      return '';
    }
    return mojoString16ToString(this.device.publicName);
  }

  private onSelected_(event: Event): void {
    this.dispatchPairDeviceEvent_();
    event.stopPropagation();
  }

  private onKeydown_(event: KeyboardEvent): void {
    if (event.key !== 'Enter' && event.key !== ' ') {
      return;
    }

    this.dispatchPairDeviceEvent_();
    event.stopPropagation();
  }

  private computeSecondaryLabel_(): string {
    switch (this.deviceItemState) {
      case DeviceItemState.FAILED:
        return this.i18n('bluetoothPairingFailed');
      case DeviceItemState.PAIRING:
        return this.i18n('bluetoothPairing');
      case DeviceItemState.DEFAULT:
        return '';
      default:
        return '';
    }
  }

  private dispatchPairDeviceEvent_(): void {
    this.dispatchEvent(new CustomEvent('pair-device', {
      bubbles: true,
      composed: true,
      detail: {device: this.device},
    }));
  }

  private getAriaLabel_(): string {
    if (!this.device) {
      return '';
    }

    return this.i18n(
               'bluetoothA11yDeviceName', this.itemIndex + 1, this.listSize,
               this.getDeviceName_()) +
        ' ' + this.i18n(this.getA11yDeviceTypeTextName_());
  }

  private getA11yDeviceTypeTextName_(): string  {
    switch (this.device.deviceType) {
      case DeviceType.kUnknown:
        return 'bluetoothA11yDeviceTypeUnknown';
      case DeviceType.kComputer:
        return 'bluetoothA11yDeviceTypeComputer';
      case DeviceType.kPhone:
        return 'bluetoothA11yDeviceTypePhone';
      case DeviceType.kHeadset:
        return 'bluetoothA11yDeviceTypeHeadset';
      case DeviceType.kVideoCamera:
        return 'bluetoothA11yDeviceTypeVideoCamera';
      case DeviceType.kGameController:
        return 'bluetoothA11yDeviceTypeGameController';
      case DeviceType.kKeyboard:
        return 'bluetoothA11yDeviceTypeKeyboard';
      case DeviceType.kKeyboardMouseCombo:
        return 'bluetoothA11yDeviceTypeKeyboardMouseCombo';
      case DeviceType.kMouse:
        return 'bluetoothA11yDeviceTypeMouse';
      case DeviceType.kTablet:
        return 'bluetoothA11yDeviceTypeTablet';
      default:
        return '';
    }
  }

  private getSecondaryAriaLabel_(): string|undefined {
    const deviceName = this.getDeviceName_();
    switch (this.deviceItemState) {
      case DeviceItemState.FAILED:
        return this.i18n(
            'bluetoothPairingDeviceItemSecondaryErrorA11YLabel', deviceName);
      case DeviceItemState.PAIRING:
        return this.i18n(
            'bluetoothPairingDeviceItemSecondaryPairingA11YLabel', deviceName);
      case DeviceItemState.DEFAULT:
        return '';
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothPairingDeviceItemElement.is]: SettingsBluetoothPairingDeviceItemElement;
  }
}

customElements.define(
    SettingsBluetoothPairingDeviceItemElement.is,
    SettingsBluetoothPairingDeviceItemElement);
