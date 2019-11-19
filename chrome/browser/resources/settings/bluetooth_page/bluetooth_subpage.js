// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Maximum number of bluetooth devices shown in bluetooth subpage.
 * @type {number}
 */
const MAX_NUMBER_DEVICE_SHOWN = 50;

/**
 * @fileoverview
 * 'settings-bluetooth-subpage' is the settings subpage for managing bluetooth
 *  properties and devices.
 */

Polymer({
  is: 'settings-bluetooth-subpage',

  behaviors: [
    I18nBehavior,
    CrScrollableBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Reflects the bluetooth-page property. */
    bluetoothToggleState: {
      type: Boolean,
      notify: true,
    },

    /** Reflects the bluetooth-page property. */
    stateChangeInProgress: Boolean,

    /**
     * The bluetooth adapter state, cached by bluetooth-page.
     * @type {!chrome.bluetooth.AdapterState|undefined}
     */
    adapterState: Object,

    /** Informs bluetooth-page whether to show the spinner in the header. */
    showSpinner_: {
      type: Boolean,
      notify: true,
      computed: 'computeShowSpinner_(adapterState.*, dialogShown_)',
    },

    /**
     * The ordered list of bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     * @private
     */
    deviceList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The ordered list of paired or connecting bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    pairedDeviceList_: {
      type: Array,
      value: /** @return {Array} */ function() {
        return [];
      },
    },

    /**
     * Reflects the iron-list selecteditem property.
     * @type {!chrome.bluetooth.Device}
     * @private
     */
    selectedPairedItem_: {
      type: Object,
      observer: 'selectedPairedItemChanged_',
    },

    /**
     * The ordered list of unpaired bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    unpairedDeviceList_: {
      type: Array,
      value: /** @return {Array} */ function() {
        return [];
      },
    },

    /**
     * Reflects the iron-list selecteditem property.
     * @type {!chrome.bluetooth.Device}
     */
    selectedUnpairedItem_: {
      type: Object,
      observer: 'selectedUnpairedItemChanged_',
    },

    /**
     * Whether or not the dialog is shown.
     * @private
     */
    dialogShown_: {
      type: Boolean,
      value: false,
    },

    /**
     * Current Pairing device.
     * @type {!chrome.bluetooth.Device|undefined}
     * @private
     */
    pairingDevice_: Object,

    /**
     * Interface for bluetooth calls. Set in bluetooth-page.
     * @type {Bluetooth}
     * @private
     */
    bluetooth: {
      type: Object,
      value: chrome.bluetooth,
    },

    /**
     * Interface for bluetoothPrivate calls. Set in bluetooth-page.
     * @type {BluetoothPrivate}
     * @private
     */
    bluetoothPrivate: {
      type: Object,
      value: chrome.bluetoothPrivate,
    },

    /**
     * Update frequency of the bluetooth list.
     * @type {number}
     */
    listUpdateFrequencyMs: {
      type: Number,
      value: 1000,
    },

    /**
     * The time in milliseconds at which discovery was started attempt (when the
     * page was opened with Bluetooth on, or when Bluetooth turned on while the
     * page was active).
     * @private {?number}
     */
    discoveryStartTimestampMs_: {
      type: Number,
      value: null,
    },

  },

  observers: [
    'adapterStateChanged_(adapterState.*)',
    'deviceListChanged_(deviceList_.*)',
    'listUpdateFrequencyMsChanged_(listUpdateFrequencyMs)',
  ],

  /**
   * Timer ID for bluetooth list update.
   * @type {number|undefined}
   * @private
   */
  updateTimerId_: undefined,

  /** @override */
  detached: function() {
    if (this.updateTimerId_ !== undefined) {
      window.clearInterval(this.updateTimerId_);
      this.updateTimerId_ = undefined;
      this.deviceList_ = [];
    }
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    this.updateDiscovery_();
    this.startOrStopRefreshingDeviceList_();
  },

  /** @private */
  computeShowSpinner_: function() {
    return !this.dialogShown_ && this.get('adapterState.discovering');
  },

  /** @private */
  adapterStateChanged_: function() {
    this.updateDiscovery_();
    this.startOrStopRefreshingDeviceList_();
  },

  /** @private */
  deviceListChanged_: function() {
    this.saveScroll(this.$.pairedDevices);
    this.saveScroll(this.$.unpairedDevices);

    this.pairedDeviceList_ = this.getUpdatedDeviceList_(
      this.pairedDeviceList_,
      this.deviceList_.filter(d => d.paired || d.connecting));
    this.unpairedDeviceList_ = this.getUpdatedDeviceList_(
      this.unpairedDeviceList_,
      this.deviceList_.filter(d => !(d.paired || d.connecting)));

    this.$.pairedDevices.fire('iron-resize');
    this.$.unpairedDevices.fire('iron-resize');
    this.updateScrollableContents();
    this.restoreScroll(this.$.unpairedDevices);
    this.restoreScroll(this.$.pairedDevices);
  },

  /**
   * Returns a copy of |oldDeviceList| but:
   *   - Using the corresponding device objects in |newDeviceList|
   *   - Removing devices not in |newDeviceList|
   *   - Adding device not in |oldDeviceList| but in |newDeviceList| to the
   *     end of the list.
   *
   * @param {!Array<!chrome.bluetooth.Device>} oldDeviceList
   * @param {!Array<!chrome.bluetooth.Device>} newDeviceList
   * @return {!Array<!chrome.bluetooth.Device>}
   * @private
   */
  getUpdatedDeviceList_: function(oldDeviceList, newDeviceList) {
    const newDeviceMap = new Map(newDeviceList.map(d => [d.address, d]));
    const updatedDeviceList = [];

    // Add elements of |oldDeviceList| that are in |newDeviceList| to
    // |updatedDeviceList|.
    for (const oldDevice of oldDeviceList) {
      const newDevice = newDeviceMap.get(oldDevice.address);
      if (newDevice === undefined) {
        continue;
      }
      updatedDeviceList.push(newDevice);
      newDeviceMap.delete(newDevice.address);
    }

    // Add all elements of |newDeviceList| that are not in |oldDeviceList| to
    // |updatedDeviceList|.
    for (const newDevice of newDeviceMap.values()) {
      updatedDeviceList.push(newDevice);
    }

    return updatedDeviceList;
  },

  /** @private */
  selectedPairedItemChanged_: function() {
    if (this.selectedPairedItem_) {
      this.connectDevice_(this.selectedPairedItem_);
    }
  },

  /** @private */
  selectedUnpairedItemChanged_: function() {
    if (this.selectedUnpairedItem_) {
      this.connectDevice_(this.selectedUnpairedItem_);
    }
  },

  /** @private */
  updateDiscovery_: function() {
    if (!this.adapterState || !this.adapterState.powered) {
      return;
    }
    if (settings.getCurrentRoute() == settings.routes.BLUETOOTH_DEVICES) {
      this.startDiscovery_();
    } else {
      this.stopDiscovery_();
    }
  },

  /** @private */
  startDiscovery_: function() {
    if (!this.adapterState || this.adapterState.discovering) {
      return;
    }

    this.bluetooth.startDiscovery(function() {
      const lastError = chrome.runtime.lastError;
      if (lastError) {
        if (lastError.message == 'Starting discovery failed') {
          return;
        }  // May happen if also started elsewhere, ignore.
        console.error('startDiscovery Error: ' + lastError.message);
      }
    });
  },

  /** @private */
  stopDiscovery_: function() {
    if (!this.get('adapterState.discovering')) {
      return;
    }

    this.bluetooth.stopDiscovery(function() {
      const lastError = chrome.runtime.lastError;
      if (lastError) {
        if (lastError.message == 'Failed to stop discovery') {
          return;
        }  // May happen if also stopped elsewhere, ignore.
        console.error('stopDiscovery Error: ' + lastError.message);
      }
    });
  },

  /**
   * @param {!CustomEvent<!{
   *     action: string, device:
   *     !chrome.bluetooth.Device
   * }>} e
   * @private
   */
  onDeviceEvent_: function(e) {
    const action = e.detail.action;
    const device = e.detail.device;
    if (action == 'connect') {
      this.connectDevice_(device);
    } else if (action == 'disconnect') {
      this.disconnectDevice_(device);
    } else if (action == 'remove') {
      this.forgetDevice_(device);
    } else {
      console.error('Unexected action: ' + action);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_: function(event) {
    if (this.isToggleEnabled_()) {
      this.bluetoothToggleState = !this.bluetoothToggleState;
    }
    event.stopPropagation();
  },

  /**
   * @param {boolean} enabled
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOnOffString_: function(enabled, onstr, offstr) {
    // If these strings are changed to convey more information other than "On"
    // and "Off" in the future, revisit the a11y implementation to ensure no
    // meaningful information is skipped.
    return enabled ? onstr : offstr;
  },

  /**
   * @return {boolean}
   * @private
   */
  isToggleEnabled_: function() {
    return this.adapterState !== undefined && this.adapterState.available &&
        !this.stateChangeInProgress;
  },

  /**
   * @param {boolean} bluetoothToggleState
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean}
   * @private
   */
  showDevices_: function(bluetoothToggleState, deviceList) {
    return bluetoothToggleState && deviceList.length > 0;
  },

  /**
   * @param {boolean} bluetoothToggleState
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean}
   * @private
   */
  showNoDevices_: function(bluetoothToggleState, deviceList) {
    return bluetoothToggleState && deviceList.length == 0;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  connectDevice_: function(device) {
    if (device.connecting || device.connected) {
      return;
    }

    // If the device is not paired, show the pairing dialog before connecting.
    // TODO(crbug.com/966170): Need to check if the device is pairable as well.
    const isPaired = device.paired;
    if (!isPaired) {
      this.pairingDevice_ = device;
      this.openDialog_();
    }

    if (isPaired !== undefined && device.transport !== undefined) {
      this.recordDeviceSelectionDuration_(isPaired, device.transport);
    }

    const address = device.address;
    this.bluetoothPrivate.connect(address, result => {
      if (isPaired) {
        this.recordUserInitiatedReconnectionAttemptResult_(result);
      }

      // If |pairingDevice_| has changed, ignore the connect result.
      if (this.pairingDevice_ && address != this.pairingDevice_.address) {
        return;
      }

      // Let the dialog handle any errors, otherwise close the dialog.
      const dialog = this.$.deviceDialog;
      if (dialog.endConnectionAttempt(
              device, !isPaired /* wasPairing */, chrome.runtime.lastError,
              result)) {
        this.openDialog_();
      } else if (
          result != chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS) {
        this.$.deviceDialog.close();
      }
    });
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  disconnectDevice_: function(device) {
    this.bluetoothPrivate.disconnectAll(device.address, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error disconnecting: ' + device.address +
            chrome.runtime.lastError.message);
      }
    });
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  forgetDevice_: function(device) {
    this.bluetoothPrivate.forgetDevice(device.address, () => {
      if (chrome.runtime.lastError) {
        console.error(
            'Error forgetting: ' + device.name + ': ' +
            chrome.runtime.lastError.message);
      }
    });
  },

  /** @private */
  openDialog_: function() {
    if (this.dialogShown_) {
      return;
    }
    // Call flush so that the dialog gets sized correctly before it is opened.
    Polymer.dom.flush();
    this.$.deviceDialog.open();
    this.dialogShown_ = true;
  },

  /** @private */
  onDialogClose_: function() {
    this.dialogShown_ = false;
    this.pairingDevice_ = undefined;
    // The list is dynamic so focus the first item.
    const device = this.$$('#unpairedContainer bluetooth-device-list-item');
    if (device) {
      device.focus();
    }
  },

  /**
   * Requests bluetooth device list from Chrome. Update deviceList_ once the
   * results are returned from chrome.
   * @private
   */
  refreshBluetoothList_: function() {
    const filter = {
      filterType: chrome.bluetooth.FilterType.KNOWN,
      limit: MAX_NUMBER_DEVICE_SHOWN
    };
    this.bluetooth.getDevices(filter, devices => {
      this.deviceList_ = devices;
    });
  },

  /** @private */
  startOrStopRefreshingDeviceList_: function() {
    if (this.adapterState && this.adapterState.powered) {
      if (this.updateTimerId_ !== undefined) {
        return;
      }

      this.refreshBluetoothList_();
      this.updateTimerId_ =
        window.setInterval(this.refreshBluetoothList_.bind(this),
                           this.listUpdateFrequencyMs);
      this.discoveryStartTimestampMs_ = Date.now();
      return;
    }
    window.clearInterval(this.updateTimerId_);
    this.updateTimerId_ = undefined;
    this.discoveryStartTimestampMs_ = null;
    this.deviceList_ = [];
  },

  /**
   * Restarts the timer when the frequency changes, which happens
   * during tests.
   */
  listUpdateFrequencyMsChanged_: function() {
    if (this.updateTimerId_ === undefined) {
      return;
    }

    window.clearInterval(this.updateTimerId_);
    this.updateTimerId_ = undefined;

    this.startOrStopRefreshingDeviceList_();
  },

  /**
   * Record metrics for user-initiated attempts to reconnect to an already
   * paired device.
   * @param {!chrome.bluetoothPrivate.ConnectResultType} result The connection
   *     result.
   * @private
   */
  recordUserInitiatedReconnectionAttemptResult_: function(result) {
    let success;
    if (chrome.runtime.lastError) {
      success = false;
    } else {
      switch (result) {
        case chrome.bluetoothPrivate.ConnectResultType.SUCCESS:
          success = true;
          break;
        case chrome.bluetoothPrivate.ConnectResultType.AUTH_CANCELED:
        case chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS:
          // Don't record metrics until connection has ended, and don't record
          // cancellations.
          return;
        default:
          success = false;
          break;
      }
    }

    chrome.bluetoothPrivate.recordReconnection(success);
  },

  /**
   * Record metrics for how long it took between when discovery started on the
   * Settings page, and the user selected the device they wanted to connect to.
   * @param {!boolean} wasPaired If the selected device was already
   *     paired.
   * @param {!chrome.bluetooth.Transport} transport The transport type
   *     of the device.
   * @private
   */
  recordDeviceSelectionDuration_: function(wasPaired, transport) {
    if (!this.discoveryStartTimestampMs_) {
      // It's not necessarily an error that |discoveryStartTimestampMs_| isn't
      // present; it's intentionally cleared after the first device selection
      // (see further on in this method). Recording subsequent device selections
      // after the first would provide inflated durations that don't truly
      // reflect how long it took for the user to find the device they're
      // looking for.
      return;
    }

    chrome.bluetoothPrivate.recordDeviceSelection(
        Date.now() - this.discoveryStartTimestampMs_, wasPaired, transport);

    this.discoveryStartTimestampMs_ = null;
  },
});
