// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Root UI element for Bluetooth pairing dialog.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import './bluetooth_pairing_device_selection_page.js';
import './bluetooth_pairing_enter_code_page.js';
import './bluetooth_pairing_request_code_page.js';
import './bluetooth_pairing_confirm_code_page.js';
import './bluetooth_spinner_page.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './bluetooth_pairing_ui.html.js';
import {BluetoothDeviceProperties, BluetoothDiscoveryDelegateInterface, BluetoothDiscoveryDelegateReceiver, BluetoothSystemState, DevicePairingDelegateInterface, DevicePairingDelegateReceiver, DevicePairingHandlerInterface, KeyEnteredHandlerInterface, KeyEnteredHandlerPendingReceiver, KeyEnteredHandlerReceiver, PairingResult, SystemPropertiesObserverInterface, SystemPropertiesObserverReceiver} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {assert, assertNotReached} from '//resources/ash/common/assert.js';

import {PairingAuthType} from './bluetooth_types.js';
import {mojoString16ToString} from './bluetooth_utils.js';
import {getBluetoothConfig} from './cros_bluetooth_config.js';

/** @implements {KeyEnteredHandlerInterface} */
class KeyEnteredHandler {
  /**
   * @param {!SettingsBluetoothPairingUiElement} page
   * @param {!KeyEnteredHandlerPendingReceiver}
   *     keyEnteredHandlerReceiver
   */
  constructor(page, keyEnteredHandlerReceiver) {
    /** @private {!SettingsBluetoothPairingUiElement} */
    this.page_ = page;

    /** @private {!KeyEnteredHandlerReceiver} */
    this.keyEnteredHandlerReceiver_ = new KeyEnteredHandlerReceiver(this);
    this.keyEnteredHandlerReceiver_.$.bindHandle(
        keyEnteredHandlerReceiver.handle);
  }

  /** @override */
  handleKeyEntered(numKeysEntered) {
    this.page_.handleKeyEntered(numKeysEntered);
  }

  close() {
    this.keyEnteredHandlerReceiver_.$.close();
  }
}

/** @enum {string} */
const BluetoothPairingSubpageId = {
  DEVICE_SELECTION_PAGE: 'deviceSelectionPage',
  DEVICE_ENTER_CODE_PAGE: 'deviceEnterCodePage',
  DEVICE_REQUEST_CODE_PAGE: 'deviceRequestCodePage',
  DEVICE_CONFIRM_CODE_PAGE: 'deviceConfirmCodePage',
  SPINNER_PAGE: 'spinnerPage',
};

/**
 * @typedef {{
 *  resolve: ?function(string),
 *  reject: ?function(),
 * }}
 */
let RequestCodeCallback;

/**
 * @typedef {{
 *  resolve: ?function(),
 *  reject: ?function(),
 * }}
 */
let ConfirmCodeCallback;

/**
 * @implements {SystemPropertiesObserverInterface}
 * @implements {BluetoothDiscoveryDelegateInterface}
 * @implements {DevicePairingDelegateInterface}
 * @implements {KeyEnteredHandlerInterface}
 * @polymer
 */
export class SettingsBluetoothPairingUiElement extends PolymerElement {
  static get is() {
    return 'bluetooth-pairing-ui';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The address, when set, of the specific device that will be attempted to
       * be paired with by the pairing dialog. If null, no specific device will
       * be paired with and the user will be allowed to select a device to pair
       * with. This is set when the dialog is opened if the purpose of the
       * dialog is to pair with a specific device.
       * @type {?string}
       */
      pairingDeviceAddress: {
        type: String,
        value: null,
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

      /**
       * Id of the currently selected Bluetooth pairing subpage.
       * @private {!BluetoothPairingSubpageId}
       */
      selectedPageId_: {
        type: String,
        value: BluetoothPairingSubpageId.DEVICE_SELECTION_PAGE,
        observer: 'onSelectedPageIdChanged_',
      },

      /**
       * @private {Array<!BluetoothDeviceProperties>}
       */
      discoveredDevices_: {
        type: Array,
        value: [],
      },

      /**
       * This can be null if no pairing attempt was started or a pairing attempt
       * was cancelled by user.
       * @private {?BluetoothDeviceProperties}
       */
      devicePendingPairing_: {
        type: Object,
        value: null,
      },

      /** @private {?PairingAuthType} */
      pairingAuthType_: {
        type: Object,
        value: null,
      },

      /** @private {string} */
      pairingCode_: {
        type: String,
        value: '',
      },

      /** @private {number} */
      numKeysEntered_: {
        type: Number,
        value: 0,
      },

      /**
       * Id of a device who's pairing attempt failed.
       * @private {string}
       */
      lastFailedPairingDeviceId_: {
        type: String,
        value: '',
      },

      /** @private {boolean} */
      isBluetoothEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used to access |BluetoothPairingSubpageId| type in HTML.
       * @private {!BluetoothPairingSubpageId}
       */
      SubpageId: {
        type: Object,
        value: BluetoothPairingSubpageId,
      },
    };
  }

  constructor() {
    super();
    /**
     * @private {!SystemPropertiesObserverReceiver}
     */
    this.systemPropertiesObserverReceiver_ =
        new SystemPropertiesObserverReceiver(
            /**
             * @type {!SystemPropertiesObserverInterface}
             */
            (this));

    /**
     * @private {!BluetoothDiscoveryDelegateReceiver}
     */
    this.bluetoothDiscoveryDelegateReceiver_ =
        new BluetoothDiscoveryDelegateReceiver(this);

    /**
     * @private {?DevicePairingHandlerInterface}
     */
    this.devicePairingHandler_;

    /**
     * The device to be paired with after the current pairDevice_() request has
     * finished.
     * @private {?BluetoothDeviceProperties}
     */
    this.queuedDevicePendingPairing_;

    /**
     * The Mojo receiver of the current ongoing pairing. If null indicates no
     * pairing is occurring.
     * @private {?DevicePairingDelegateReceiver}
     */
    this.pairingDelegateReceiver_ = null;

    /** @private {?RequestCodeCallback} */
    this.requestCodeCallback_ = null;

    /** @private {?KeyEnteredHandler} */
    this.keyEnteredReceiver_ = null;

    /** @private {?ConfirmCodeCallback} */
    this.confirmCodeCallback_ = null;

    /** @private {?function()} */
    this.onBluetoothDiscoveryStartedCallbackForTest_ = null;

    /** @private {?function()} */
    this.handlePairDeviceResultCallbackForTest_ = null;
  }

  ready() {
    super.ready();

    // If there's a specific device to pair with, immediately go to the spinner
    // page.
    if (this.pairingDeviceAddress) {
      this.selectedPageId_ = BluetoothPairingSubpageId.SPINNER_PAGE;
    }
  }

  connectedCallback() {
    super.connectedCallback();

    getBluetoothConfig().observeSystemProperties(
        this.systemPropertiesObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    if (this.systemPropertiesObserverReceiver_) {
      this.systemPropertiesObserverReceiver_.$.close();
    }
    if (this.bluetoothDiscoveryDelegateReceiver_) {
      this.bluetoothDiscoveryDelegateReceiver_.$.close();
    }
    if (this.pairingDelegateReceiver_) {
      this.pairingDelegateReceiver_.$.close();
    }
    if (this.keyEnteredReceiver_) {
      this.keyEnteredReceiver_.close();
    }
  }

  /** @override */
  onPropertiesUpdated(properties) {
    const wasBluetoothEnabled = this.isBluetoothEnabled_;
    this.isBluetoothEnabled_ =
        properties.systemState === BluetoothSystemState.kEnabled;

    if (!wasBluetoothEnabled && this.isBluetoothEnabled_) {
      // If Bluetooth enables after being disabled, initialize the UI state.
      this.lastFailedPairingDeviceId_ = '';

      // Start discovery.
      getBluetoothConfig().startDiscovery(
          this.bluetoothDiscoveryDelegateReceiver_.$
              .bindNewPipeAndPassRemote());
    }
  }

  /** @override */
  onDiscoveredDevicesListChanged(discoveredDevices) {
    this.discoveredDevices_ = discoveredDevices;

    this.updateLastFailedPairingDeviceId_(discoveredDevices);

    // Check if this dialog needs to pair to a specific device.
    if (!this.pairingDeviceAddress) {
      return;
    }

    // Check if a pairing is already occurring.
    if (this.pairingDelegateReceiver_) {
      return;
    }

    // If |this.pairingDeviceAddress| exists and no ongoing pairing is
    // occurring, search for the device with address |this.pairingDeviceAddress|
    // and attempt to pair with it.
    this.attemptPairDeviceByAddress_();
  }

  /**
   * @param {Array<!BluetoothDeviceProperties>}
   *     devices
   * @private
   */
  updateLastFailedPairingDeviceId_(devices) {
    if (devices.some(device => device.id === this.lastFailedPairingDeviceId_)) {
      return;
    }

    this.lastFailedPairingDeviceId_ = '';
  }

  /** @override */
  onBluetoothDiscoveryStarted(handler) {
    this.devicePairingHandler_ = handler;

    // Inform tests that onBluetoothDiscoveryStarted() has been called. This is
    // to ensure tests don't progress until |devicePairingHandler_| has been
    // set.
    if (this.onBluetoothDiscoveryStartedCallbackForTest_) {
      this.onBluetoothDiscoveryStartedCallbackForTest_();
    }
  }

  /** @override */
  onBluetoothDiscoveryStopped() {
    // Discovery will stop if Bluetooth disables. Reset the UI back to the
    // selection page.
    this.bluetoothDiscoveryDelegateReceiver_.$.close();
    this.selectedPageId_ = BluetoothPairingSubpageId.DEVICE_SELECTION_PAGE;
    this.devicePairingHandler_ = null;
  }

  /**
   * Returns a promise that will be resolved the next time
   * onBluetoothDiscoveryStarted() is called.
   * @return {Promise}
   */
  waitForOnBluetoothDiscoveryStartedForTest() {
    return new Promise((resolve) => {
      this.onBluetoothDiscoveryStartedCallbackForTest_ = resolve;
    });
  }

  /**
   * Returns a promise that will be resolved the next time
   * handlePairDeviceResult_() is called.
   * @return {Promise}
   */
  waitForHandlePairDeviceResultForTest() {
    return new Promise((resolve) => {
      this.handlePairDeviceResultCallbackForTest_ = resolve;
    });
  }

  /**
   * @param {!CustomEvent<!{device:
   *     BluetoothDeviceProperties}>} event
   * @private
   */
  onPairDevice_(event) {
    if (!event.detail.device) {
      return;
    }
    // If a pairing operation is currently underway, close it and queue
    // the current device to be paired after pairDevice_() promise is
    // returned.
    if (this.pairingDelegateReceiver_) {
      this.queuedDevicePendingPairing_ = event.detail.device;
      this.pairingDelegateReceiver_.$.close();
      return;
    }
    this.pairDevice_(event.detail.device);
  }

  /**
   * Searches for the device with address |this.pairingDeviceAddress| in
   * |this.discoveredDevices| and attempts to pair with it.
   * @private
   */
  attemptPairDeviceByAddress_() {
    assert(this.pairingDeviceAddress);
    assert(!this.pairingDelegateReceiver_);

    if (!this.devicePairingHandler_) {
      console.error('Attempted pairing with no device pairing handler.');
      return;
    }

    this.devicePairingHandler_.fetchDevice(this.pairingDeviceAddress)
        .then(result => {
          if (!result.device) {
            console.warn(
                'Attempted pairing with a device that was not found, address: ' +
                this.pairingDeviceAddress);
            return;
          }

          this.pairDevice_(result.device);
        });
  }

  /**
   * @param {!BluetoothDeviceProperties} device
   * @private
   */
  pairDevice_(device) {
    assert(
        this.devicePairingHandler_, 'devicePairingHandler_ has not been set.');

    this.pairingDelegateReceiver_ = new DevicePairingDelegateReceiver(this);

    this.devicePendingPairing_ = device;
    assert(this.devicePendingPairing_);

    this.lastFailedPairingDeviceId_ = '';

    this.devicePairingHandler_
        .pairDevice(
            this.devicePendingPairing_.id,
            this.pairingDelegateReceiver_.$.bindNewPipeAndPassRemote())
        .then(result => {
          this.handlePairDeviceResult_(result.result);
        })
        .catch(() => {
          // Pairing failed due to external issues, such as Mojo pipe
          // disconnecting from Bluetooth disabling.
          this.handlePairDeviceResult_(PairingResult.kNonAuthFailure);
        });
  }

  /**
   * @param {!PairingResult} result
   * @private
   */
  handlePairDeviceResult_(result) {
    if (this.pairingDelegateReceiver_) {
      this.pairingDelegateReceiver_.$.close();
    }
    this.pairingAuthType_ = null;

    if (this.keyEnteredReceiver_) {
      this.keyEnteredReceiver_.close();
      this.keyEnteredReceiver_ = null;
    }

    this.pairingDelegateReceiver_ = null;

    if (result === PairingResult.kSuccess) {
      this.closeDialog_();
      return;
    }

    // If |pairingDeviceAddress| is defined, this was a device-specific pairing
    // request that has failed. Clear |pairingDeviceAddress| so that we don't
    // automatically attempt to re-pair with the same device again.
    this.pairingDeviceAddress = null;

    this.selectedPageId_ = BluetoothPairingSubpageId.DEVICE_SELECTION_PAGE;
    if (this.devicePendingPairing_) {
      this.lastFailedPairingDeviceId_ = this.devicePendingPairing_.id;
    }

    this.devicePendingPairing_ = null;

    if (this.queuedDevicePendingPairing_ && this.devicePairingHandler_) {
      this.pairDevice_(this.queuedDevicePendingPairing_);
    }

    this.queuedDevicePendingPairing_ = null;

    // Inform tests that handlePairDeviceResult_() has been called. This is
    // to ensure tests don't progress until the correct state has been
    // set.
    if (this.handlePairDeviceResultCallbackForTest_) {
      this.handlePairDeviceResultCallbackForTest_();
    }
  }

  /** @override */
  requestPinCode() {
    return this.requestCode_(PairingAuthType.REQUEST_PIN_CODE);
  }

  /** @override */
  requestPasskey() {
    return this.requestCode_(PairingAuthType.REQUEST_PASSKEY);
  }

  /**
   * @param {!PairingAuthType} authType
   * @return {!Promise<{pinCode: !string}> | !Promise<{passkey: !string}>}
   * @private
   */
  requestCode_(authType) {
    this.pairingAuthType_ = authType;
    this.selectedPageId_ = BluetoothPairingSubpageId.DEVICE_REQUEST_CODE_PAGE;
    this.requestCodeCallback_ = {
      reject: null,
      resolve: null,
    };

    const promise = new Promise((resolve, reject) => {
      this.requestCodeCallback_.resolve = (code) => {
        if (authType === PairingAuthType.REQUEST_PIN_CODE) {
          resolve({'pinCode': code});
          return;
        }

        if (authType === PairingAuthType.REQUEST_PASSKEY) {
          resolve({'passkey': code});
          return;
        }

        assertNotReached();
      };
      this.requestCodeCallback_.reject = reject;
    });

    return promise;
  }

  /**
   * @param {!CustomEvent<!{code: string}>} event
   * @private
   */
  onRequestCodeEntered_(event) {
    this.selectedPageId_ = BluetoothPairingSubpageId.SPINNER_PAGE;
    event.stopPropagation();
    assert(this.pairingAuthType_);
    assert(this.requestCodeCallback_.resolve);
    this.requestCodeCallback_.resolve(event.detail.code);
  }

  /** @override */
  displayPinCode(pinCode, handler) {
    this.displayCode_(handler, pinCode);
  }

  /** @override */
  displayPasskey(passkey, handler) {
    this.displayCode_(handler, passkey);
  }

  /**s
   * @param {!KeyEnteredHandlerPendingReceiver}
   *     handler
   * @param {string} code
   * @private
   */
  displayCode_(handler, code) {
    this.pairingCode_ = code;
    this.selectedPageId_ = BluetoothPairingSubpageId.DEVICE_ENTER_CODE_PAGE;
    this.keyEnteredReceiver_ = new KeyEnteredHandler(this, handler);
  }

  /**
   * @param {number} numKeysEntered
   */
  handleKeyEntered(numKeysEntered) {
    this.numKeysEntered_ = numKeysEntered;
  }

  /** @override */
  confirmPasskey(passkey) {
    this.pairingAuthType_ = PairingAuthType.CONFIRM_PASSKEY;
    this.selectedPageId_ = BluetoothPairingSubpageId.DEVICE_CONFIRM_CODE_PAGE;
    this.pairingCode_ = passkey;

    this.confirmCodeCallback_ = {
      resolve: null,
      reject: null,
    };

    return new Promise((resolve, reject) => {
      this.confirmCodeCallback_.resolve = () => {
        resolve({'confirmed': true});
      };
      this.confirmCodeCallback_.reject = reject;
    });
  }

  /**
   * @param {!Event} event
   * @private
   */
  onConfirmCode_(event) {
    this.selectedPageId_ = BluetoothPairingSubpageId.SPINNER_PAGE;
    event.stopPropagation();
    assert(this.pairingAuthType_);
    assert(this.confirmCodeCallback_);
    this.confirmCodeCallback_.resolve();
  }

  /** @override */
  authorizePairing() {
    // TODO(crbug.com/1010321): Implement this function.
  }

  /**
   * @param {!BluetoothPairingSubpageId} subpageId
   * @return {boolean}
   * @private
   */
  shouldShowSubpage_(subpageId) {
    return this.selectedPageId_ === subpageId;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onCancelClick_(event) {
    event.stopPropagation();
    this.devicePendingPairing_ = null;
    if (this.pairingDelegateReceiver_) {
      this.pairingDelegateReceiver_.$.close();
      this.finishPendingCallbacksForTest_();
      return;
    }

    // If there is no receiver, this means pairing was not initiated and we
    // we are currently in DEVICE_SELECTION_PAGE or something went wrong and
    // |pairingDelegateReceiver_| was not instantiated. (b/218368694)
    this.closeDialog_();
  }

  /** @private */
  closeDialog_() {
    this.dispatchEvent(new CustomEvent('finished', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @private */
  onSelectedPageIdChanged_() {
    // If the current page changes to the device selection page, focus the item
    // corresponding to the last device attempted to be paired with.
    if (this.selectedPageId_ !==
        BluetoothPairingSubpageId.DEVICE_SELECTION_PAGE) {
      return;
    }

    const deviceSelectionPage =
        this.shadowRoot.querySelector('#deviceSelectionPage');
    if (!deviceSelectionPage) {
      return;
    }

    deviceSelectionPage.attemptFocusLastSelectedItem();
  }

  /** @private */
  finishPendingCallbacksForTest_() {
    if (this.requestCodeCallback_) {
      // |requestCodeCallback_| promise is held by FakeDevicePairingHandler
      // in test. This does not get resolved for the test case where user
      // cancels request while in request code page. Calling reject is
      // necessary here to make sure the promise is resolved.
      this.requestCodeCallback_.reject();
    }

    if (this.confirmCodeCallback_) {
      // |confirmCodeCallback_| promise is held by FakeDevicePairingHandler
      // in test. This does not get resolved for the test case where user
      // cancels request while in request code page. Calling reject is
      // necessary here to make sure the promise is resolved.
      this.confirmCodeCallback_.reject();
    }
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceName_() {
    if (!this.devicePendingPairing_) {
      return '';
    }
    return mojoString16ToString(this.devicePendingPairing_.publicName);
  }
}

customElements.define(
    SettingsBluetoothPairingUiElement.is, SettingsBluetoothPairingUiElement);
