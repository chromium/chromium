// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DevicesPage and DevicesView, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('devices_page', function() {
  /**
   * Enum of scan status for the devices page.
   * @enum {number}
   */
  const ScanStatus = {
    OFF: 0,
    STARTING: 1,
    ON: 2,
    STOPPING: 3,
  };


  /**
   * Page that contains a header and a DevicesView.
   */
  class DevicesPage extends cr.ui.pageManager.Page {
    constructor() {
      super('devices', 'Devices', 'devices');

      this.deviceTable = new device_table.DeviceTable();
      this.pageDiv.appendChild(this.deviceTable);
      this.scanBtn_ = this.pageDiv.querySelector('#scan-btn');
      this.scanBtn_.addEventListener('click', event => {
        this.pageDiv.dispatchEvent(new CustomEvent('scanpressed'));
      });
    }

    /**
     * Sets the device collection for the page's device table.
     * @param {!device_collection.DeviceCollection} devices
     */
    setDevices(devices) {
      this.deviceTable.setDevices(devices);
    }

    /**
     * Updates the inspect status of the given |deviceInfo| in the device table.
     * @param {!bluetooth.mojom.DeviceInfo} deviceInfo
     * @param {boolean} isInspecting
     */
    setInspecting(deviceInfo, isInspecting) {
      this.deviceTable.setInspecting(deviceInfo, isInspecting);
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

  return {
    DevicesPage: DevicesPage,
    ScanStatus: ScanStatus,
  };
});
