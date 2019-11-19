// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for usb_internals.html, served from chrome://usb-internals/.
 */
cr.define('usb_internals', function() {
  class UsbInternals {
    constructor() {}

    async initializeViews() {
      // window.setupFn() provides a hook for the test suite to perform setup
      // actions after the page is loaded but before any script is run.
      await window.setupFn();

      const pageHandler = mojom.UsbInternalsPageHandler.getRemote();

      // Connection to the UsbInternalsPageHandler instance running in the
      // browser process.
      /** @type {device.mojom.UsbDeviceManagerRemote} */
      const usbManager = new device.mojom.UsbDeviceManagerRemote;
      await pageHandler.bindUsbDeviceManagerInterface(
          usbManager.$.bindNewPipeAndPassReceiver());

      /** @private {devices_page.DevicesPage} */
      this.devicesPage_ = new devices_page.DevicesPage(usbManager);

      /** @private {device.mojom.UsbDeviceManagerTestRemote} */
      this.usbManagerTest_ = new device.mojom.UsbDeviceManagerTestRemote;
      await pageHandler.bindTestInterface(
          this.usbManagerTest_.$.bindNewPipeAndPassReceiver());

      $('add-test-device-form').addEventListener('submit', (event) => {
        this.addTestDevice(event);
      });
      this.refreshTestDeviceList();

      cr.ui.decorate('tabbox', cr.ui.TabBox);
    }

    async refreshTestDeviceList() {
      const response = await this.usbManagerTest_.getTestDevices();

      const tableBody = $('test-device-list');
      tableBody.innerHTML = '';

      const rowTemplate = document.querySelector('#test-device-row');
      const td = rowTemplate.content.querySelectorAll('td');

      for (const device of response.devices) {
        td[0].textContent = device.name;
        td[1].textContent = device.serialNumber;
        td[2].textContent = device.landingPage.url;

        const clone = document.importNode(rowTemplate.content, true);

        const removeButton = clone.querySelector('button');
        removeButton.addEventListener('click', async () => {
          await this.usbManagerTest_.removeDeviceForTesting(device.guid);
          this.refreshTestDeviceList();
        });

        tableBody.appendChild(clone);
      }
    }

    async addTestDevice(event) {
      event.preventDefault();

      const response = await this.usbManagerTest_.addDeviceForTesting(
          $('test-device-name').value, $('test-device-serial').value,
          $('test-device-landing-page').value);
      if (response.success) {
        this.refreshTestDeviceList();
      }

      $('add-test-device-result').textContent = response.message;
      $('add-test-device-result').className =
          response.success ? 'action-success' : 'action-failure';
    }
  }

  return {
    UsbInternals,
  };
});

window.setupFn = window.setupFn || function() {
  return Promise.resolve();
};

document.addEventListener('DOMContentLoaded', () => {
  const usbInternalsPage = new usb_internals.UsbInternals();
  usbInternalsPage.initializeViews();
});
