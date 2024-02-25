// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for usb_internals.html, served from chrome://usb-internals/.
 */

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree.js';

import {assert} from 'chrome://resources/js/assert.js';

import {getTemplate} from './app.html.js';
import {DevicesPage} from './devices_page.js';
import {UsbInternalsPageHandler} from './usb_internals.mojom-webui.js';
import {UsbDeviceManagerRemote} from './usb_manager.mojom-webui.js';
import {UsbDeviceManagerTestRemote} from './usb_manager_test.mojom-webui.js';

let setupFn: () => Promise<void> = () => {
  return Promise.resolve();
};

export function setSetupFn(newSetupFn: () => Promise<void>) {
  setupFn = newSetupFn;
}

export class UsbInternalsAppElement extends HTMLElement {
  private usbManagerTest_: UsbDeviceManagerTestRemote|null = null;

  static get template() {
    return getTemplate();
  }

  constructor() {
    super();

    this.attachShadow({mode: 'open'});
    const template = document.createElement('template');
    template.innerHTML =
        UsbInternalsAppElement.template || window.trustedTypes!.emptyHTML;
    this.shadowRoot!.appendChild(template.content.cloneNode(true));
  }

  $<T extends Element>(query: string): T {
    const element = this.shadowRoot!.querySelector<T>(query);
    assert(element);
    return element;
  }

  async connectedCallback() {
    // setupFn() provides a hook for the test suite to perform setup
    // actions after the page is loaded but before any script is run.
    await setupFn();

    const pageHandler = UsbInternalsPageHandler.getRemote();

    // Connection to the UsbInternalsPageHandler instance running in the
    // browser process.
    const usbManager = new UsbDeviceManagerRemote();
    await pageHandler.bindUsbDeviceManagerInterface(
        usbManager.$.bindNewPipeAndPassReceiver());

    new DevicesPage(usbManager, this.shadowRoot!);

    this.usbManagerTest_ = new UsbDeviceManagerTestRemote();
    await pageHandler.bindTestInterface(
        this.usbManagerTest_.$.bindNewPipeAndPassReceiver());

    this.$<HTMLElement>('#add-test-device-form')
        .addEventListener('submit', (event: Event) => {
          this.addTestDevice(event);
        });
    this.refreshTestDeviceList();

    const tabbox = this.$<HTMLElement>('cr-tab-box');
    tabbox.hidden = false;
  }

  async refreshTestDeviceList() {
    assert(this.usbManagerTest_);
    const response = await this.usbManagerTest_.getTestDevices();

    const tableBody = this.$<HTMLElement>('#test-device-list');
    tableBody.innerHTML = window.trustedTypes!.emptyHTML;

    const rowTemplate = this.$<HTMLTemplateElement>('#test-device-row');
    const td = rowTemplate.content.querySelectorAll('td');

    for (const device of response.devices) {
      td[0]!.textContent = device.name;
      td[1]!.textContent = device.serialNumber;
      td[2]!.textContent = device.landingPage.url;

      const clone = document.importNode(rowTemplate.content, true);

      const removeButton = clone.querySelector('button');
      assert(removeButton);
      removeButton.addEventListener('click', async () => {
        assert(this.usbManagerTest_);
        await this.usbManagerTest_.removeDeviceForTesting(device.guid);
        this.refreshTestDeviceList();
      });

      tableBody.appendChild(clone);
    }
  }

  async addTestDevice(event: Event) {
    event.preventDefault();

    assert(this.usbManagerTest_);
    const response = await this.usbManagerTest_.addDeviceForTesting(
        this.$<HTMLInputElement>('#test-device-name').value,
        this.$<HTMLInputElement>('#test-device-serial').value,
        this.$<HTMLInputElement>('#test-device-landing-page').value);
    if (response.success) {
      this.refreshTestDeviceList();
    }

    this.$<HTMLElement>('#add-test-device-result').textContent =
        response.message;
    this.$<HTMLElement>('#add-test-device-result').className =
        response.success ? 'action-success' : 'action-failure';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'usb-internals-app': UsbInternalsAppElement;
  }
}

customElements.define('usb-internals-app', UsbInternalsAppElement);
