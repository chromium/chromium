// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DevicesPage and DevicesView, served from
 *     chrome://bluetooth-internals/.
 */
import './device_table.js';

import {DeviceCollection} from './device_collection.js';
import {Page} from './page.js';

/**
 * Enum of scan status for the devices page.
 * @enum {number}
 */
export const ScanStatus = {
  OFF: 0,
  STARTING: 1,
  ON: 2,
  STOPPING: 3,
};


/**
 * Page that contains a header and a DevicesView.
 */
export class DevicesPage extends Page {
  constructor() {
    super('devices', 'Devices', 'devices');

    this.deviceTable = document.createElement('device-table');
    this.pageDiv.appendChild(this.deviceTable);
    this.scanBtn_ = this.pageDiv.querySelector('#scan-btn');
    this.scanBtn_.addEventListener('click', event => {
      this.pageDiv.dispatchEvent(new CustomEvent('scanpressed'));
    });
  }

  /**
   * Sets the device collection for the page's device table.
   * @param {!DeviceCollection} devices
   */
  setDevices(devices) {
    this.deviceTable.setDevices(devices);
  }

  /**
   * Updates the inspect status of the given |deviceInfo| in the device table.
   * @param {!DeviceInfo} deviceInfo
   * @param {boolean} isInspecting
   */
  setInspecting(deviceInfo, isInspecting) {
    this.deviceTable.setInspecting(deviceInfo, isInspecting);
  }

  /**
   * If Bluetooth is currently powered off do not show start discovery button.
   * @param {boolean} powered
   */
  updatedScanButtonVisibility(powered) {
    this.scanBtn_.hidden = !powered;
  }

  setScanStatus(status) {
    switch (status) {
      case ScanStatus.OFF:
        this.scanBtn_.disabled = false;
        this.scanBtn_.textContent = 'Start Scan';
        break;
      case ScanStatus.STARTING:
        this.scanBtn_.disabled = true;
        this.scanBtn_.textContent = 'Starting...';
        break;
      case ScanStatus.ON:
        this.scanBtn_.disabled = false;
        this.scanBtn_.textContent = 'Stop Scan';
        break;
      case ScanStatus.STOPPING:
        this.scanBtn_.disabled = true;
        this.scanBtn_.textContent = 'Stopping...';
        break;
    }
  }
}
