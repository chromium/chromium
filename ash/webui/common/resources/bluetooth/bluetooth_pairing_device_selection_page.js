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
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/cr_components/localized_link/localized_link.js';

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from '//resources/ash/common/cr_scrollable_behavior.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {getTemplate} from './bluetooth_pairing_device_selection_page.html.js';
import {ButtonBarState, ButtonState, DeviceItemState} from './bluetooth_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingDeviceSelectionPageElementBase =
    mixinBehaviors([CrScrollableBehavior, I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothPairingDeviceSelectionPageElement extends
    SettingsBluetoothPairingDeviceSelectionPageElementBase {
  static get is() {
    return 'bluetooth-pairing-device-selection-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * @type {!Array<!BluetoothDeviceProperties>}
       */
      devices: {
        type: Array,
        value: [],
        observer: 'onDevicesChanged_',
      },

      /**
       * Id of a device who's pairing attempt failed.
       * @type {string}
       */
      failedPairingDeviceId: {
        type: String,
        value: '',
      },

      /**
       * @type {?BluetoothDeviceProperties}
       */
      devicePendingPairing: {
        type: Object,
        value: null,
        observer: 'onDevicePendingPairingChanged_',
      },

      /** @type {boolean} */
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

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       * @private
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowBehavior to track if the list has been blurred.
       * @private
       */
      listBlurred_: Boolean,
    };
  }

  constructor() {
    super();

    /**
     * The last device that was selected for pairing.
     * @private {?BluetoothDeviceProperties}
     */
    this.lastSelectedDevice_ = null;
  }

  /**
   * Attempts to focus the item corresponding to |lastSelectedDevice_|.
   */
  attemptFocusLastSelectedItem() {
    if (!this.lastSelectedDevice_) {
      return;
    }

    const index = this.devices.findIndex(
        device => device.id === this.lastSelectedDevice_.id);
    if (index < 0) {
      return;
    }

    afterNextRender(this, function() {
      const items =
          this.shadowRoot.querySelectorAll('bluetooth-pairing-device-item');
      if (index >= items.length) {
        return;
      }

      items[index].focus();
    });
  }

  /** @private */
  onDevicesChanged_() {
    // CrScrollableBehaviorInterface method required for list items to be
    // properly rendered when devices updates. This is because iron-list size
    // is not fixed, if this is not called iron-list container would not be
    // properly sized.
    this.updateScrollableContents();
  }

  /** @private */
  onDevicePendingPairingChanged_() {
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

  /**
   * @private
   * @return {boolean}
   */
  shouldShowDeviceList_() {
    return this.isBluetoothEnabled && this.devices && this.devices.length > 0;
  }

  /**
   * @private
   * @return {string}
   */
  getDeviceListTitle_() {
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

  /**
   * @param {?BluetoothDeviceProperties} device
   * @return {!DeviceItemState}
   * @private
   */
  getDeviceItemState_(device) {
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

customElements.define(
    SettingsBluetoothPairingDeviceSelectionPageElement.is,
    SettingsBluetoothPairingDeviceSelectionPageElement);
