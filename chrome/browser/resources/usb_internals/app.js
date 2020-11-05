// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for usb_internals.html, served from chrome://usb-internals/.
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {TabBox} from 'chrome://resources/js/cr/ui/tabs.m.js';

import {DevicesPage} from './devices_page.js';
import {UsbInternalsPageHandler} from './usb_internals.mojom-webui.js';
import {UsbDeviceManagerRemote} from './usb_manager.mojom-webui.js';
import {UsbDeviceManagerTestRemote} from './usb_manager_test.mojom-webui.js';

window.setupFn = window.setupFn || function() {
  return Promise.resolve();
};

class UsbInternalsAppElement extends HTMLElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    this.attachShadow({mode: 'open'});
    const template = document.createElement('template');
    template.innerHTML = this.constructor.template || '';
    this.shadowRoot.appendChild(template.content.cloneNode(true));
  }

  /**
   * @param {string} query
   * @return {?Element}
   */
  $(query) {
    return this.shadowRoot.querySelector(query);
  }

  async connectedCallback() {
    // window.setupFn() provides a hook for the test suite to perform setup
    // actions after the page is loaded but before any script is run.
    await window.setupFn();

    const pageHandler = UsbInternalsPageHandler.getRemote();

    // Connection to the UsbInternalsPageHandler instance running in the
    // browser process.
    /** @type {UsbDeviceManagerRemote} */
    const usbManager = new UsbDeviceManagerRemote;
    await pageHandler.bindUsbDeviceManagerInterface(
        usbManager.$.bindNewPipeAndPassReceiver());

    /** @private {!DevicesPage} */
    this.devicesPage_ = new DevicesPage(usbManager, assert(this.shadowRoot));

    /** @private {UsbDeviceManagerTestRemote} */
    this.usbManagerTest_ = new UsbDeviceManagerTestRemote;
    await pageHandler.bindTestInterface(
        this.usbManagerTest_.$.bindNewPipeAndPassReceiver());

    this.$('#add-test-device-form').addEventListener('submit', (event) => {
      this.addTestDevice(event);
    });
    this.refreshTestDeviceList();

    decorate(assert(this.$('tabbox')), TabBox);
  }

  async refreshTestDeviceList() {
    const response = await this.usbManagerTest_.getTestDevices();

    const tableBody = this.$('#test-device-list');
    tableBody.innerHTML = trustedTypes.emptyHTML;

    const rowTemplate = this.$('#test-device-row');
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
        this.$('#test-device-name').value, this.$('#test-device-serial').value,
        this.$('#test-device-landing-page').value);
    if (response.success) {
      this.refreshTestDeviceList();
    }

    this.$('#add-test-device-result').textContent = response.message;
    this.$('#add-test-device-result').className =
        response.success ? 'action-success' : 'action-failure';
  }
}
customElements.define('usb-internals-app', UsbInternalsAppElement);
