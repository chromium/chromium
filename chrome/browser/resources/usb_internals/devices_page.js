// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DevicesPage, served from
 *     chrome://usb-internals/.
 */

import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {Tab, TabPanel} from 'chrome://resources/js/cr/ui/tabs.m.js';
import {Tree, TreeItem} from 'chrome://resources/js/cr/ui/tree.m.js';
import {queryRequiredElement} from 'chrome://resources/js/util.m.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

import {DescriptorPanel, renderClassCodeWithDescription} from './descriptor_panel.js';

import {UsbAlternateInterfaceInfo, UsbConfigurationInfo, UsbDeviceInfo, UsbDeviceRemote, UsbEndpointInfo, UsbInterfaceInfo, UsbTransferDirection, UsbTransferType} from './usb_device.mojom-webui.js';
import {UsbDeviceManagerRemote} from './usb_manager.mojom-webui.js';

/**
 * Page that contains a tab header and a tab panel displaying devices table.
 */
export class DevicesPage {
  /**
   * @param {!UsbDeviceManagerRemote} usbManager
   * @param {!ShadowRoot} root
   */
  constructor(usbManager, root) {
    /** @private {!UsbDeviceManagerRemote} */
    this.usbManager_ = usbManager;
    this.root = root;
    this.renderDeviceList_();
  }

  /**
   * Sets the device collection for the page's device table.
   * @private
   */
  async renderDeviceList_() {
    const response = await this.usbManager_.getDevices(null);

    /** @type {!Array<!UsbDeviceInfo>} */
    const devices = response.results;

    const tableBody = this.root.querySelector('#device-list');
    tableBody.innerHTML = trustedTypes.emptyHTML;

    const rowTemplate = this.root.querySelector('#device-row');

    for (const device of devices) {
      /** @type {DocumentFragment|Node} */
      const clone = document.importNode(rowTemplate.content, true);

      const td = clone.querySelectorAll('td');

      td[0].textContent = device.busNumber;
      td[1].textContent = device.portNumber;
      td[2].textContent = toHex(device.vendorId);
      td[3].textContent = toHex(device.productId);
      if (device.manufacturerName) {
        td[4].textContent = decodeString16(device.manufacturerName);
      }
      if (device.productName) {
        td[5].textContent = decodeString16(device.productName);
      }
      if (device.serialNumber) {
        td[6].textContent = decodeString16(device.serialNumber);
      }

      const inspectButton = clone.querySelector('button');
      inspectButton.addEventListener('click', () => {
        this.switchToTab_(device);
      });

      tableBody.appendChild(clone);
    }
    // window.deviceListCompleteFn() provides a hook for the test suite to
    // perform test actions after the devices list is loaded.
    window.deviceListCompleteFn();
  }

  /**
   * Switches to the device's tab, creating one if necessary.
   * @param {!UsbDeviceInfo} device
   * @private
   */
  switchToTab_(device) {
    const tabId = device.guid;

    if (null == this.root.getElementById(tabId)) {
      const devicePage = new DevicePage(this.usbManager_, device, this.root);
    }
    this.root.getElementById(tabId).selected = true;
  }
}

/**
 * Page that contains a tree view displaying devices detail and can get
 * descriptors.
 */
class DevicePage {
  /**
   * @param {!UsbDeviceManagerRemote} usbManager
   * @param {!UsbDeviceInfo} device
   * @param {!ShadowRoot} root
   */
  constructor(usbManager, device, root) {
    this.usbManager_ = usbManager;
    this.root = root;
    this.renderTab_(device);
  }

  /**
   * Renders a tab to display a tree view showing device's detail information.
   * @param {!UsbDeviceInfo} device
   * @private
   */
  renderTab_(device) {
    const tabs = queryRequiredElement('tabs', this.root);

    const tabTemplate = queryRequiredElement('#tab-template', this.root);
    /** @type {DocumentFragment|Node} */
    const tabClone = document.importNode(tabTemplate.content, true);

    const tab = tabClone.querySelector('tab');
    if (device.productName) {
      tab.textContent = decodeString16(device.productName);
    } else {
      const vendorId = toHex(device.vendorId).slice(2);
      const productId = toHex(device.productId).slice(2);
      tab.textContent = `${vendorId}:${productId}`;
    }
    tab.id = device.guid;

    tabs.appendChild(tabClone);
    decorate(tab, Tab);

    const tabPanels = queryRequiredElement('tabpanels', this.root);
    const tabPanelTemplate =
        queryRequiredElement('#device-tabpanel-template', this.root);
    /** @type {DocumentFragment|Node} */
    const tabPanelClone = document.importNode(tabPanelTemplate.content, true);

    /**
     * Root of the WebContents tree of current device.
     */
    const treeViewRoot = assertInstanceof(
        tabPanelClone.querySelector('.tree-view'), HTMLElement);
    decorate(treeViewRoot, Tree);
    treeViewRoot.detail = {payload: {}, children: {}};
    // Clear the tree first before populating it with the new content.
    treeViewRoot.innerText = '';
    renderDeviceTree(device, treeViewRoot);

    const tabPanel =
        assertInstanceof(tabPanelClone.querySelector('tabpanel'), HTMLElement);
    this.initializeDescriptorPanels_(tabPanel, device.guid);

    tabPanels.appendChild(tabPanelClone);
    decorate(tabPanel, TabPanel);
  }

  /**
   * Initializes all the descriptor panels.
   * @param {!HTMLElement} tabPanel
   * @param {string} guid
   * @private
   */
  async initializeDescriptorPanels_(tabPanel, guid) {
    const usbDevice = new UsbDeviceRemote;
    await this.usbManager_.getDevice(
        guid, /*blocked_interface_classes=*/[],
        usbDevice.$.bindNewPipeAndPassReceiver(), /*device_client=*/ null);

    const deviceDescriptorPanel =
        initialInspectorPanel(tabPanel, 'device-descriptor', usbDevice, guid);

    const configurationDescriptorPanel = initialInspectorPanel(
        tabPanel, 'configuration-descriptor', usbDevice, guid);

    const stringDescriptorPanel =
        initialInspectorPanel(tabPanel, 'string-descriptor', usbDevice, guid);
    deviceDescriptorPanel.setStringDescriptorPanel(stringDescriptorPanel);
    configurationDescriptorPanel.setStringDescriptorPanel(
        stringDescriptorPanel);

    initialInspectorPanel(tabPanel, 'bos-descriptor', usbDevice, guid);

    initialInspectorPanel(tabPanel, 'testing-tool', usbDevice, guid);

    // window.deviceTabInitializedFn() provides a hook for the test suite to
    // perform test actions after the device tab query descriptors actions are
    // initialized.
    window.deviceTabInitializedFn();
  }
}

/**
 * Renders a tree to display the device's detail information.
 * @param {!UsbDeviceInfo} device
 * @param {!Tree} root
 */
function renderDeviceTree(device, root) {
  root.add(customTreeItem(`USB Version: ${device.usbVersionMajor}.${
      device.usbVersionMinor}.${device.usbVersionSubminor}`));

  root.add(customTreeItem(
      `Class Code: ${renderClassCodeWithDescription(device.classCode)}`));

  root.add(customTreeItem(`Subclass Code: ${device.subclassCode}`));

  root.add(customTreeItem(`Protocol Code: ${device.protocolCode}`));

  root.add(customTreeItem(`Port Number: ${device.portNumber}`));

  root.add(customTreeItem(`Vendor Id: ${toHex(device.vendorId)}`));

  root.add(customTreeItem(`Product Id: ${toHex(device.productId)}`));

  root.add(customTreeItem(`Device Version: ${device.deviceVersionMajor}.${
      device.deviceVersionMinor}.${device.deviceVersionSubminor}`));

  if (device.manufacturerName) {
    root.add(customTreeItem(
        `Manufacturer Name: ${decodeString16(device.manufacturerName)}`));
  }

  if (device.productName) {
    root.add(
        customTreeItem(`Product Name: ${decodeString16(device.productName)}`));
  }

  if (device.serialNumber) {
    root.add(customTreeItem(
        `Serial Number: ${decodeString16(device.serialNumber)}`));
  }

  if (device.webusbLandingPage) {
    const urlItem =
        customTreeItem(`WebUSB Landing Page: ${device.webusbLandingPage.url}`);
    root.add(urlItem);

    urlItem.querySelector('.tree-label')
        .addEventListener(
            'click', () => window.open(device.webusbLandingPage.url, '_blank'));
  }

  root.add(
      customTreeItem(`Active Configuration: ${device.activeConfiguration}`));

  const configurationsArray = device.configurations;
  renderConfigurationTreeItem(configurationsArray, root);
}

/**
 * Renders a tree item to display the device's configuration information.
 * @param {!Array<!UsbConfigurationInfo>} configurationsArray
 * @param {!Tree} root
 */
function renderConfigurationTreeItem(configurationsArray, root) {
  for (const configuration of configurationsArray) {
    const configurationItem =
        customTreeItem(`Configuration ${configuration.configurationValue}`);

    if (configuration.configurationName) {
      configurationItem.add(customTreeItem(`Configuration Name: ${
          decodeString16(configuration.configurationName)}`));
    }

    const interfacesArray = configuration.interfaces;
    renderInterfacesTreeItem(interfacesArray, configurationItem);

    root.add(configurationItem);
  }
}

/**
 * Renders a tree item to display the device's interface information.
 * @param {!Array<!UsbInterfaceInfo>} interfacesArray
 * @param {!TreeItem} root
 */
function renderInterfacesTreeItem(interfacesArray, root) {
  for (const currentInterface of interfacesArray) {
    const interfaceItem =
        customTreeItem(`Interface ${currentInterface.interfaceNumber}`);

    const alternatesArray = currentInterface.alternates;
    renderAlternatesTreeItem(alternatesArray, interfaceItem);

    root.add(interfaceItem);
  }
}

/**
 * Renders a tree item to display the device's alternate interfaces
 * information.
 * @param {!Array<!UsbAlternateInterfaceInfo>} alternatesArray
 * @param {!TreeItem} root
 */
function renderAlternatesTreeItem(alternatesArray, root) {
  for (const alternate of alternatesArray) {
    const alternateItem =
        customTreeItem(`Alternate ${alternate.alternateSetting}`);

    alternateItem.add(customTreeItem(
        `Class Code: ${renderClassCodeWithDescription(alternate.classCode)}`));

    alternateItem.add(
        customTreeItem(`Subclass Code: ${alternate.subclassCode}`));

    alternateItem.add(
        customTreeItem(`Protocol Code: ${alternate.protocolCode}`));

    if (alternate.interfaceName) {
      alternateItem.add(customTreeItem(
          `Interface Name: ${decodeString16(alternate.interfaceName)}`));
    }

    const endpointsArray = alternate.endpoints;
    renderEndpointsTreeItem(endpointsArray, alternateItem);

    root.add(alternateItem);
  }
}

/**
 * Renders a tree item to display the device's endpoints information.
 * @param {!Array<!UsbEndpointInfo>} endpointsArray
 * @param {!TreeItem} root
 */
function renderEndpointsTreeItem(endpointsArray, root) {
  for (const endpoint of endpointsArray) {
    let itemLabel = 'Endpoint ';

    itemLabel += endpoint.endpointNumber;

    switch (endpoint.direction) {
      case UsbTransferDirection.INBOUND:
        itemLabel += ' (INBOUND)';
        break;
      case UsbTransferDirection.OUTBOUND:
        itemLabel += ' (OUTBOUND)';
        break;
    }

    const endpointItem = customTreeItem(itemLabel);

    let usbTransferType = '';
    switch (endpoint.type) {
      case UsbTransferType.CONTROL:
        usbTransferType = 'CONTROL';
        break;
      case UsbTransferType.ISOCHRONOUS:
        usbTransferType = 'ISOCHRONOUS';
        break;
      case UsbTransferType.BULK:
        usbTransferType = 'BULK';
        break;
      case UsbTransferType.INTERRUPT:
        usbTransferType = 'INTERRUPT';
        break;
    }

    endpointItem.add(customTreeItem(`USB Transfer Type: ${usbTransferType}`));

    endpointItem.add(customTreeItem(`Packet Size: ${endpoint.packetSize}`));

    root.add(endpointItem);
  }
}

/**
 * Initialize a descriptor panel.
 * @param {!HTMLElement} tabPanel
 * @param {string} panelType
 * @param {!UsbDeviceRemote} usbDevice
 * @param {string} guid
 * @return {!DescriptorPanel}
 */
function initialInspectorPanel(tabPanel, panelType, usbDevice, guid) {
  const button = queryRequiredElement(`.${panelType}-button`, tabPanel);
  const displayElement = queryRequiredElement(`.${panelType}-panel`, tabPanel);
  const descriptorPanel = new DescriptorPanel(
      usbDevice, /** @type {!HTMLElement} */ (displayElement));
  switch (panelType) {
    case 'string-descriptor':
      descriptorPanel.initialStringDescriptorPanel(guid);
      break;
    case 'testing-tool':
      descriptorPanel.initialTestingToolPanel();
      break;
  }

  button.addEventListener('click', async () => {
    displayElement.hidden = !displayElement.hidden;
    // Clear the panel before rendering new data.
    descriptorPanel.clearView();

    if (!displayElement.hidden) {
      switch (panelType) {
        case 'device-descriptor':
          await descriptorPanel.getDeviceDescriptor();
          break;
        case 'configuration-descriptor':
          await descriptorPanel.getConfigurationDescriptor();
          break;
        case 'string-descriptor':
          await descriptorPanel.getAllLanguageCodes();
          break;
        case 'bos-descriptor':
          await descriptorPanel.getBosDescriptor();
          break;
      }
    }
  });
  return descriptorPanel;
}

/**
 * Parses utf16 coded string.
 * @param {!String16} arr
 * @return {string}
 */
function decodeString16(arr) {
  return arr.data.map(ch => String.fromCodePoint(ch)).join('');
}

/**
 * Parses the decimal number to hex format.
 * @param {number} num
 * @return {string}
 */
function toHex(num) {
  return `0x${num.toString(16).padStart(4, '0').toUpperCase()}`;
}

/**
 * Renders a customized TreeItem with the given content and class name.
 * @param {string} itemLabel
 * @return {!TreeItem}
 * @private
 */
function customTreeItem(itemLabel) {
  return new TreeItem({
    label: itemLabel,
    icon: '',
  });
}

window.deviceListCompleteFn = window.deviceListCompleteFn || function() {};

window.deviceTabInitializedFn = window.deviceTabInitializedFn || function() {};
