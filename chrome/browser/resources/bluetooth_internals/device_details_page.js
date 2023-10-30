// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DeviceDetailsPage which displays all of the details of a
 * device. The page is generated and managed dynamically in bluetooth_internals.
 * served from chrome://bluetooth-internals/.
 */

import './service_list.js';
import './object_fieldset.js';

import {$} from 'chrome://resources/js/util.js';

import {DeviceRemote} from './device.mojom-webui.js';
import {connectToDevice} from './device_broker.js';
import {ConnectionStatus} from './device_collection.js';
import {formatManufacturerDataMap, formatServiceUuids} from './device_utils.js';
import {ObjectFieldSetElement} from './object_fieldset.js';
import {Page} from './page.js';
import {showSnackbar, SnackbarType} from './snackbar.js';

/**
 * Property names that will be displayed in the ObjectFieldSet which contains
 * the DeviceInfo object.
 */
const PROPERTY_NAMES = {
  name: 'Name',
  address: 'Address',
  isGattConnected: 'GATT Connected',
  'rssi.value': 'Latest RSSI',
  serviceUuids: 'Services',
  manufacturerDataMap: 'Manufacturer Data',
};

/**
 * Page that displays all of the details of a device. This page is generated
 * and managed dynamically in bluetooth_internals. The page contains two
 * sections: Status and Services. The Status section displays information from
 * the DeviceInfo object and the Services section contains a ServiceList
 * compononent that lists all of the active services on the device.
 */
export class DeviceDetailsPage extends Page {
  /**
   * @param {string} id
   * @param {!DeviceInfo} deviceInfo
   */
  constructor(id, deviceInfo) {
    super(id, deviceInfo.nameForDisplay, id);

    /** @type {!DeviceInfo} */
    this.deviceInfo = deviceInfo;

    /** @type {?Array<ServiceInfo>} */
    this.services = null;

    /** @private {?DeviceRemote} */
    this.device_ = null;

    /** @private {!ObjectFieldSetElement} */
    this.deviceFieldSet_ = document.createElement('object-field-set');
    this.deviceFieldSet_.toggleAttribute('show-all', true);
    this.deviceFieldSet_.dataset.nameMap = JSON.stringify(PROPERTY_NAMES);

    /** @private {!ServiceList} */
    this.serviceList_ = document.createElement('service-list');

    /** @private {!ConnectionStatus} */
    this.status_ = ConnectionStatus.DISCONNECTED;

    /** @private {?Element} */
    this.connectBtn_ = null;

    this.pageDiv.appendChild(document.importNode(
        $('device-details-template').content, true /* deep */));

    this.pageDiv.querySelector('.device-details')
        .appendChild(this.deviceFieldSet_);
    this.pageDiv.querySelector('.services').appendChild(this.serviceList_);

    this.pageDiv.querySelector('.forget').addEventListener('click', function() {
      this.disconnect();
      this.pageDiv.dispatchEvent(new CustomEvent('forgetpressed', {
        detail: {
          address: this.deviceInfo.address,
        },
      }));
    }.bind(this));

    this.connectBtn_ = this.pageDiv.querySelector('.disconnect');
    this.connectBtn_.addEventListener('click', function() {
      this.device_ !== null ? this.disconnect() : this.connect();
    }.bind(this));

    this.redraw();
  }

  /** Creates a connection to the Bluetooth device. */
  connect() {
    if (this.status_ !== ConnectionStatus.DISCONNECTED) {
      return;
    }

    this.updateConnectionStatus_(ConnectionStatus.CONNECTING);

    connectToDevice(this.deviceInfo.address)
        .then(function(device) {
          this.device_ = device;

          this.updateConnectionStatus_(ConnectionStatus.CONNECTED);

          // Fetch services asynchronously.
          return this.device_.getServices();
        }.bind(this))
        .then(function(response) {
          this.services = response.services;
          this.serviceList_.load(this.deviceInfo.address);
          this.redraw();
          this.fireDeviceInfoChanged_();
        }.bind(this))
        .catch(function(error) {
          // If a connection error occurs while fetching the services, the
          // DeviceRemote reference must be removed.
          if (this.device_) {
            this.device_.disconnect();
            this.device_ = null;
          }

          showSnackbar(
              this.deviceInfo.nameForDisplay + ': ' + error.message,
              SnackbarType.ERROR, 'Retry', this.connect.bind(this));

          this.updateConnectionStatus_(ConnectionStatus.DISCONNECTED);
        }.bind(this));
  }

  /** Disconnects the page from the Bluetooth device. */
  disconnect() {
    if (!this.device_) {
      return;
    }

    this.device_.disconnect();
    this.device_ = null;
    this.updateConnectionStatus_(ConnectionStatus.DISCONNECTED);
  }

  /** Redraws the contents of the page with the current |deviceInfo|. */
  redraw() {
    const isConnected = this.deviceInfo.isGattConnected;

    // Update status if connection has changed.
    if (isConnected) {
      this.connect();
    } else {
      this.disconnect();
    }

    const connectedText = isConnected ? 'Connected' : 'Not Connected';

    const rssi = this.deviceInfo.rssi || {};

    let rssiValue = 'Unknown';
    if (rssi.value != null && rssi.value <= 0) {
      rssiValue = rssi.value;
    }

    const serviceUuidsText = formatServiceUuids(this.deviceInfo.serviceUuids);

    const manufacturerDataMapText =
        formatManufacturerDataMap(this.deviceInfo.manufacturerDataMap);

    const deviceViewObj = {
      name: this.deviceInfo.nameForDisplay,
      address: this.deviceInfo.address,
      isGattConnected: connectedText,
      'rssi.value': rssiValue,
      serviceUuids: serviceUuidsText,
      manufacturerDataMap: manufacturerDataMapText,
    };

    this.deviceFieldSet_.dataset.value = JSON.stringify(deviceViewObj);
  }

  /**
   * Sets the page's device info and forces a redraw.
   * @param {!DeviceInfo} info
   */
  setDeviceInfo(info) {
    this.deviceInfo = info;
    this.redraw();
  }

  /**
   * Fires an 'infochanged' event with the current |deviceInfo|
   * @private
   */
  fireDeviceInfoChanged_() {
    this.pageDiv.dispatchEvent(new CustomEvent('infochanged', {
      bubbles: true,
      detail: {
        info: this.deviceInfo,
      },
    }));
  }

  /**
   * Updates the current connection status. Caches the latest status, updates
   * the connection button message, and fires a 'connectionchanged' event when
   * finished.
   * @param {!ConnectionStatus} status
   * @private
   */
  updateConnectionStatus_(status) {
    if (this.status_ === status) {
      return;
    }

    this.status_ = status;
    if (status === ConnectionStatus.DISCONNECTED) {
      this.connectBtn_.textContent = 'Connect';
      this.connectBtn_.disabled = false;
    } else if (status === ConnectionStatus.CONNECTING) {
      this.connectBtn_.textContent = 'Connecting';
      this.connectBtn_.disabled = true;
    } else {
      this.connectBtn_.textContent = 'Disconnect';
      this.connectBtn_.disabled = false;
    }

    this.pageDiv.dispatchEvent(new CustomEvent('connectionchanged', {
      bubbles: true,
      detail: {
        address: this.deviceInfo.address,
        status: status,
      },
    }));
  }
}
