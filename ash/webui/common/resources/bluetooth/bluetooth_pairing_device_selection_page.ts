// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element to show a list of discovered Bluetooth devices and initiate
 * pairing to a device.
 */
import './bluetooth_base_page.js';
import './bluetooth_pairing_device_item.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/ash/common/cr_elements/localized_link/localized_link.js';

import {CrScrollableMixin} from '//resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {BluetoothDeviceProperties} from '//resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_pairing_device_selection_page.html.js';
import {ButtonBarState, ButtonState, DeviceItemState} from './bluetooth_types.js';

const SettingsBluetoothPairingDeviceSelectionPageElementBase =
    I18nMixin(CrScrollableMixin(PolymerElement));

export class SettingsBluetoothPairingDeviceSelectionPageElement extends
    SettingsBluetoothPairingDeviceSelectionPageElementBase {
  static get is() {
    return 'bluetooth-pairing-device-selection-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      devices: {
        type: Array,
        value: [],
        observer: 'onDevicesChanged_',
      },

      /**
       * Id of a device who's pairing attempt failed.
       */
      failedPairingDeviceId: {
        type: String,
        value: '',
      },

      devicePendingPairing: {
        type: Object,
        value: null,
        observer: 'onDevicePendingPairingChanged_',
      },

      isBluetoothEnabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Flag indicating whether links should be displayed or not. In some
       * cases, such as the user being in OOBE or the login screen, links will
       * not work and should not be displayed.
       */
      shouldOmitLinks: {
        type: Boolean,
        value: false,
      },

      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowBehavior to track if the list has been blurred.
       */
      listBlurred_: Boolean,
    };
  }

  devices: BluetoothDeviceProperties[];
  failedPairingDeviceId: string;
  devicePendingPairing: BluetoothDeviceProperties|null;
  isBluetoothEnabled: boolean;
  shouldOmitLinks: boolean;
  private buttonBarState_: ButtonBarState;
  private lastFocused_?: HTMLElement;
  private listBlurred_: boolean;
  private lastSelectedDevice_: BluetoothDeviceProperties | null;

  constructor() {
    super();

    /**
     * The last device that was selected for pairing
     */
    this.lastSelectedDevice_ = null;
  }

  /**
   * Attempts to focus the item corresponding to |lastSelectedDevice_|.
   */
  attemptFocusLastSelectedItem(): void {
    if (!this.lastSelectedDevice_) {
      return;
    }

    const index = this.devices.findIndex(
        device => device.id === this.lastSelectedDevice_!.id);
    if (index < 0) {
      return;
    }

    afterNextRender(this, ()=> {
      const items =
          this.shadowRoot!.querySelectorAll('bluetooth-pairing-device-item');
      if (index >= items.length) {
        return;
      }

      items[index].focus();
    });
  }

  private onDevicesChanged_(): void {
    // CrScrollableBehaviorInterface method required for list items to be
    // properly rendered when devices updates. This is because iron-list size
    // is not fixed, if this is not called iron-list container would not be
    // properly sized.
    this.updateScrollableContents();
  }

  private onDevicePendingPairingChanged_(): void {
    // If |devicePendingPairing_| has changed to a defined value, it was the
    // last selected device. |devicePendingPairing_| gets reset to null whenever
    // we move back to this page after a pairing attempt fails or cancels. In
    // this case, do not reset |lastSelectedDevice_| because we want to hold
    // onto the device that was last attempted to be paired with.
    if (!this.devicePendingPairing) {
      return;
    }

    this.lastSelectedDevice_ = this.devicePendingPairing;
  }

  private shouldShowDeviceList_(): boolean {
    return this.isBluetoothEnabled && this.devices && this.devices.length > 0;
  }

  private getDeviceListTitle_(): string {
    if (!this.isBluetoothEnabled) {
      return this.i18n('bluetoothDisabled');
    }

    // Case where device list becomes empty (device is turned off)
    // and then device pairing fails.
    // Note: |devices| always updates before pairing result
    // returns.
    if (!this.devicePendingPairing && !this.shouldShowDeviceList_()) {
      return this.i18n('bluetoothNoAvailableDevices');
    }

    // When pairing succeeds there is a brief moement where |devices| is empty
    // (device is removed from discovered list) but because of b/216522777 we
    // still want to show available devices header, we check for
    // |devicePendingPairing| which will have a value since it is only reset
    // after pairing fails.
    if (this.shouldShowDeviceList_() || this.devicePendingPairing) {
      return this.i18n('bluetoothAvailableDevices');
    }

    return this.i18n('bluetoothNoAvailableDevices');
  }

  private getDeviceItemState_(device: BluetoothDeviceProperties): DeviceItemState {
    if (!device) {
      return DeviceItemState.DEFAULT;
    }

    if (device.id === this.failedPairingDeviceId) {
      return DeviceItemState.FAILED;
    }

    if (this.devicePendingPairing &&
        device.id === this.devicePendingPairing.id) {
      return DeviceItemState.PAIRING;
    }

    return DeviceItemState.DEFAULT;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothPairingDeviceSelectionPageElement.is]:
    SettingsBluetoothPairingDeviceSelectionPageElement;
  }
}

customElements.define(
    SettingsBluetoothPairingDeviceSelectionPageElement.is,
    SettingsBluetoothPairingDeviceSelectionPageElement);
