// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DescriptorPanel UI, served from
 *     chrome://usb-internals/.
 */

cr.define('descriptor_panel', function() {
  const INPUT_TYPE_DECIMAL_WITH_DROPDOWN = 0;
  const INPUT_TYPE_HEX_BYTE = 1;

  // Standard USB requests and descriptor types:
  const GET_DESCRIPTOR_REQUEST = 0x06;

  const CONTROL_TRANSFER_DIRECTION_HOST_TO_DEVICE = 0;
  const CONTROL_TRANSFER_DIRECTION_DEVICE_TO_HOST = 1;

  const DEVICE_DESCRIPTOR_TYPE = 0x01;
  const CONFIGURATION_DESCRIPTOR_TYPE = 0x02;
  const STRING_DESCRIPTOR_TYPE = 0x03;
  const INTERFACE_DESCRIPTOR_TYPE = 0x04;
  const ENDPOINT_DESCRIPTOR_TYPE = 0x05;
  const BOS_DESCRIPTOR_TYPE = 0x0F;
  const DEVICE_CAPABILITY_DESCRIPTOR_TYPE = 0x10;

  const DEVICE_CAPABILITY_DESCRIPTOR_TYPE_PLATFORM_TYPE = 0x05;

  const DEVICE_DESCRIPTOR_LENGTH = 18;
  const CONFIGURATION_DESCRIPTOR_LENGTH = 9;
  const MAX_STRING_DESCRIPTOR_LENGTH = 0xFF;
  const INTERFACE_DESCRIPTOR_LENGTH = 9;
  const ENDPOINT_DESCRIPTOR_LENGTH = 7;
  const BOS_DESCRIPTOR_HEADER_LENGTH = 5;
  const PLATFORM_DESCRIPTOR_HEADER_LENGTH = 20;
  const MAX_URL_DESCRIPTOR_LENGTH = 0xFF;

  const CONTROL_TRANSFER_TIMEOUT_MS = 2000;  // 2 seconds

  const STANDARD_DESCRIPTOR_LENGTH_OFFSET = 0;
  const STANDARD_DESCRIPTOR_TYPE_OFFSET = 1;
  const CONFIGURATION_DESCRIPTOR_TOTAL_LENGTH_OFFSET = 2;
  const CONFIGURATION_DESCRIPTOR_NUM_INTERFACES_OFFSET = 4;
  const INTERFACE_DESCRIPTOR_NUM_ENDPOINTS_OFFSET = 4;
  const BOS_DESCRIPTOR_TOTAL_LENGTH_OFFSET = 2;
  const BOS_DESCRIPTOR_NUM_DEVICE_CAPABILITIES_OFFSET = 4;
  const BOS_DESCRIPTOR_DEVICE_CAPABILITY_TYPE_OFFSET = 2;

  // Language codes are defined in:
  // https://docs.microsoft.com/en-us/windows/desktop/intl/language-identifier-constants-and-strings
  const LANGUAGE_CODE_EN_US = 0x0409;

  // Windows headers defined in:
  // https://docs.microsoft.com/en-us/windows/desktop/winprog/using-the-windows-headers
  const WIN_81_HEADER = 0x06030000;

  // These constants are defined by the WebUSB specification:
  // http://wicg.github.io/webusb/
  const WEB_USB_DESCRIPTOR_LENGTH = 24;

  const GET_URL_REQUEST = 0x02;

  const WEB_USB_VENDOR_CODE_OFFSET = 22;
  const WEB_USB_URL_DESCRIPTOR_INDEX_OFFSET = 23;

  const WEB_USB_CAPABILITY_UUID = [
    // Little-endian encoding of {3408b638-09a9-47a0-8bfd-a0768815b665}.
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47, 0x8B, 0xFD, 0xA0, 0x76,
    0x88, 0x15, 0xB6, 0x65
  ];

  // These constants are defined by Microsoft OS 2.0 Descriptors Specification
  // (July, 2018).
  const MS_OS_20_DESCRIPTOR_SET_INFORMATION_LENGTH = 8;

  const MS_OS_20_DESCRIPTOR_INDEX = 0x07;
  const MS_OS_20_SET_ALT_ENUMERATION = 0x08;

  const MS_OS_20_SET_TOTAL_LENGTH_OFFSET = 4;
  const MS_OS_20_VENDOR_CODE_ITEM_OFFSET = 6;
  const MS_OS_20_ALT_ENUM_CODE_ITEM_OFFSET = 7;
  const MS_OS_20_DESCRIPTOR_LENGTH_OFFSET = 0;
  const MS_OS_20_DESCRIPTOR_TYPE_OFFSET = 2;
  const MS_OS_20_REGISTRY_PROPERTY_DESCRIPTOR_PROPERTY_DATA_TYPE_OFFSET = 4;
  const MS_OS_20_REGISTRY_PROPERTY_DESCRIPTOR_NAME_LENGTH_OFFSET = 6;

  const MS_OS_20_SET_HEADER_DESCRIPTOR = 0x00;
  const MS_OS_20_SUBSET_HEADER_CONFIGURATION = 0x01;
  const MS_OS_20_SUBSET_HEADER_FUNCTION = 0x02;
  const MS_OS_20_FEATURE_COMPATIBLE_ID = 0x03;
  const MS_OS_20_FEATURE_REG_PROPERTY = 0x04;
  const MS_OS_20_FEATURE_MIN_RESUME_TIME = 0x05;
  const MS_OS_20_FEATURE_MODEL_ID = 0x06;
  const MS_OS_20_FEATURE_CCGP_DEVICE = 0x07;
  const MS_OS_20_FEATURE_VENDOR_REVISION = 0x08;

  const MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_SZ = 0x01;
  const MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_EXPAND_SZ = 0x02;
  const MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_BINARY = 0x03;
  const MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_DWORD_LITTLE_ENDIAN = 0x04;
  const MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_DWORD_BIG_ENDIAN = 0x05;
  const MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_LINK = 0x06;
  const MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_MULTI_SZ = 0x07;

  const MS_OS_20_PLATFORM_CAPABILITY_UUID = [
    // Little-endian encoding of {D8DD60DF-4589-4CC7-9CD2-659D9E648A9F}.
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, 0x9C, 0xD2, 0x65, 0x9D,
    0x9E, 0x64, 0x8A, 0x9F
  ];

  class DescriptorPanel {
    /**
     * @param {!device.mojom.UsbDeviceInterface} usbDeviceProxy
     * @param {!HTMLElement} rootElement
     */
    constructor(usbDeviceProxy, rootElement) {
      /** @type {!device.mojom.UsbDeviceInterface} */
      this.usbDeviceProxy_ = usbDeviceProxy;

      /** @type {!HTMLElement} */
      this.rootElement_ = rootElement;
    }

    /**
     * Adds the reference of the string descriptor panel of the device for
     * string descriptor functionality.
     * @param {!descriptor_panel.DescriptorPanel} stringDescriptorPanel
     */
    setStringDescriptorPanel(stringDescriptorPanel) {
      /** @type {!descriptor_panel.DescriptorPanel} */
      this.stringDescriptorPanel_ = stringDescriptorPanel;
    }

    /**
     * Clears the data first before populating it with the new content.
     */
    clearView() {
      this.rootElement_.querySelectorAll('descriptorpanel')
          .forEach(el => el.remove());
      this.rootElement_.querySelectorAll('error').forEach(el => el.remove());
      this.rootElement_.querySelectorAll('descriptorpaneltitle')
          .forEach(el => el.remove());
    }

    /**
     * Adds a button for getting string descriptor to the string descriptor
     * index item, and adds an autocomplete value to the index input area in
     * the string descriptor panel.
     * @param {!Uint8Array} rawData
     * @param {number} offset The offset of the string descriptor index field.
     * @param {!cr.ui.TreeItem} item
     * @param {string} fieldLabel
     * @private
     */
    renderIndexItem_(rawData, offset, item, fieldLabel) {
      const index = rawData[offset];
      if (index > 0) {
        if (!this.stringDescriptorPanel_.stringDescriptorIndexes.has(index)) {
          const optionElement = document.createElement('option');
          optionElement.label = index;
          optionElement.value = index;
          this.stringDescriptorPanel_.indexesListElement.appendChild(
              optionElement);

          this.stringDescriptorPanel_.stringDescriptorIndexes.add(index);
        }

        const buttonTemplate = queryRequiredElement('#raw-data-tree-button');
        const button = document.importNode(buttonTemplate.content, true)
                           .querySelector('button');
        item.labelElement.appendChild(button);
        button.addEventListener('click', (event) => {
          event.stopPropagation();
          // Clear the previous string descriptors.
          item.querySelector('.tree-children').textContent = '';
          this.stringDescriptorPanel_.clearView();
          this.stringDescriptorPanel_.getStringDescriptorForAllLanguages_(
              index, item);
        });
      } else if (index < 0) {
        // Delete the ': ' in fieldLabel.
        const fieldName = fieldLabel.slice(0, -2);
        showError(
            `Invalid String Descriptor occurs in field ${
                fieldName} of this descriptor.`,
            this.rootElement_);
      }
    }

    /**
     * Adds a button for getting URL descriptor.
     * @param {!Uint8Array} rawData
     * @param {number} offset The offset of the URL descriptor index.
     * @param {!cr.ui.TreeItem} item
     * @param {string} fieldLabel Not used in this function, but used to match
     *     other extraTreeItemFormatter.
     * @private
     */
    renderUrlDescriptorIndexItem_(rawData, offset, item, fieldLabel) {
      const index = rawData[offset];
      if (index > 0) {
        const buttonTemplate = queryRequiredElement('#raw-data-tree-button');
        const button = document.importNode(buttonTemplate.content, true)
                           .querySelector('button');
        item.labelElement.appendChild(button);
        button.addEventListener('click', (event) => {
          event.stopPropagation();
          // Clear the previous URL descriptors.
          item.querySelector('.tree-children').textContent = '';
          this.getUrlDescriptor_(
              rawData, offset - WEB_USB_URL_DESCRIPTOR_INDEX_OFFSET, item);
        });
      }
    }

    /**
     * Adds a button for getting the Microsoft OS 2.0 vendor-specific descriptor
     * to the Microsoft OS 2.0 descriptor set information vendor-specific code
     * item.
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the Microsoft OS 2.0
     *     descriptor set information.
     * @param {!cr.ui.TreeItem} item
     * @param {string} fieldLabel Not used in this function, but used to match
     *     other extraTreeItemFormatter.
     * @private
     */
    async renderMsOs20DescriptorVendorSpecific_(
        rawData, offset, item, fieldLabel) {
      // Use the vendor specified code and the length of Microsoft OS 2.0
      // descriptor Set that contained in Microsoft OS 2.0 descriptor Set Info
      // to get Microsoft OS 2.0 Descriptor Set.
      // This is defined by Microsoft OS 2.0 Descriptors Specification (July,
      // 2018).
      const vendorCode = rawData[offset + MS_OS_20_VENDOR_CODE_ITEM_OFFSET];
      const data = new DataView(rawData.buffer, offset);
      const msOs20DescriptorSetLength =
          data.getUint16(MS_OS_20_SET_TOTAL_LENGTH_OFFSET, true);

      const buttonTemplate = queryRequiredElement('#raw-data-tree-button');
      const button = document.importNode(buttonTemplate.content, true)
                         .querySelector('button');
      item.labelElement.appendChild(button);
      button.addEventListener('click', async (event) => {
        event.stopPropagation();
        // Clear all the descriptor display elements except the first one, which
        // displays the original BOS descriptor.
        Array.from(this.rootElement_.querySelectorAll('descriptorpanel'))
            .slice(1)
            .forEach(el => el.remove());
        this.rootElement_.querySelectorAll('descriptorpaneltitle')
            .forEach(el => el.remove());
        const msOs20RawData = await this.getMsOs20DescriptorSet_(
            vendorCode, msOs20DescriptorSetLength);
        this.renderMsOs20DescriptorSet_(msOs20RawData);
      });
    }

    /**
     * Adds a button for sending a Microsoft OS 2.0 descriptor set alternate
     * enumeration command to the USB device.
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the Microsoft OS 2.0
     *     descriptor set information.
     * @param {!cr.ui.TreeItem} item
     * @param {string} fieldLabel Not used in this function, but used to match
     *     other extraTreeItemFormatter.
     * @private
     */
    async renderMsOs20DescriptorSetAltEnum_(rawData, offset, item, fieldLabel) {
      // Use the vendor specified code, alternate enumeration code to send a
      // Microsoft OS 2.0 set alternate enumeration command.
      // This is defined by Microsoft OS 2.0 Descriptors Specification (July,
      // 2018).
      const altEnumCode = rawData[offset + MS_OS_20_ALT_ENUM_CODE_ITEM_OFFSET];
      if (altEnumCode !== 0) {
        const vendorCode = rawData[offset + MS_OS_20_VENDOR_CODE_ITEM_OFFSET];

        const buttonTemplate = queryRequiredElement('#raw-data-tree-button');
        const button = document.importNode(buttonTemplate.content, true)
                           .querySelector('button');
        item.labelElement.appendChild(button);
        button.addEventListener('click', async (event) => {
          event.stopPropagation();
          await this.sendMsOs20DescriptorSetAltEnumCommand_(
              vendorCode, altEnumCode);
        });
      }
    }

    /**
     * Changes the display text in tree item for the Microsoft OS 2.0 registry
     * property descriptor.
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the registry Property
     *     descriptor.
     * @param {!cr.ui.TreeItem} item
     * @param {string} fieldLabel Not used in this function, but used to match
     *     other extraTreeItemFormatter.
     * @param {number} featureRegistryPropertyDataType
     * @private
     */
    renderFeatureRegistryPropertyDataItem_(
        rawData, offset, item, fieldLabel, featureRegistryPropertyDataType,
        length) {
      let data;
      switch (featureRegistryPropertyDataType) {
        case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_BINARY:
          break;
        case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_DWORD_LITTLE_ENDIAN:
          data = new DataView(rawData.buffer, offset);
          item.label += data.getUint32(0, true);
          break;
        case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_DWORD_BIG_ENDIAN:
          data = new DataView(rawData.buffer, offset);
          item.label += data.getUint32(0, false);
          break;
        case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_SZ:
        case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_EXPAND_SZ:
        case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_LINK:
        case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_MULTI_SZ:
          item.label +=
              decodeUtf16Array(rawData.slice(offset, offset + length), true);
          break;
        case 0:
        default:
          item.label += `Illegal Data Type. (${
              featureRegistryPropertyDataType} should be reserved.)`;
          break;
      }
    }

    /**
     * Renders a view to display standard descriptor hex data in both tree view
     * and raw form view.
     * @param {!Uint8Array} data
     * @param {number=} languageCode
     * @param {cr.ui.TreeItem=} treeItem
     * @private
     */
    async renderStandardDescriptor_(
        data, languageCode = 0, treeItem = undefined) {
      const displayElement = addNewDescriptorDisplayElement(this.rootElement_);
      /** @type {!cr.ui.Tree} */
      const rawDataTreeRoot = displayElement.rawDataTreeRoot;
      /** @type {!HTMLElement} */
      const rawDataByteElement = displayElement.rawDataByteElement;

      renderRawDataBytes(rawDataByteElement, data);

      let offset = 0;
      let indexInterface = 0;
      let indexEndpoint = 0;
      let indexUnknown = 0;
      let indexDevCapability = 0;

      let expectNumInterfaces = 0;
      let expectNumEndpoints = 0;
      let expectNumDevCapabilities = 0;

      let lastInterfaceItem;
      // Continue parsing while there are still unparsed standard descriptor.
      // Stop if accessing the descriptor type would cause us to read past the
      // end of the buffer.
      while (offset + STANDARD_DESCRIPTOR_TYPE_OFFSET < data.length) {
        const length = data[offset + STANDARD_DESCRIPTOR_LENGTH_OFFSET];
        const descriptorType = data[offset + STANDARD_DESCRIPTOR_TYPE_OFFSET];
        switch (descriptorType) {
          case DEVICE_DESCRIPTOR_TYPE:
            this.renderDeviceDescriptor_(
                rawDataTreeRoot, rawDataByteElement, data, offset);
            break;
          case CONFIGURATION_DESCRIPTOR_TYPE:
            if (CONFIGURATION_DESCRIPTOR_NUM_INTERFACES_OFFSET < length) {
              expectNumInterfaces =
                  data[offset + CONFIGURATION_DESCRIPTOR_NUM_INTERFACES_OFFSET];
            }
            this.renderConfigurationDescriptor_(
                rawDataTreeRoot, rawDataByteElement, data, offset);
            break;
          case STRING_DESCRIPTOR_TYPE:
            this.renderStringDescriptorForLanguageCode_(
                rawDataTreeRoot, rawDataByteElement, data, offset, languageCode,
                treeItem);
            break;
          case INTERFACE_DESCRIPTOR_TYPE:
            if (INTERFACE_DESCRIPTOR_NUM_ENDPOINTS_OFFSET < length) {
              expectNumEndpoints +=
                  data[offset + INTERFACE_DESCRIPTOR_NUM_ENDPOINTS_OFFSET];
            }
            lastInterfaceItem = this.renderInterfaceDescriptor_(
                rawDataTreeRoot, rawDataByteElement, data, offset,
                indexInterface);
            indexInterface++;
            break;
          case ENDPOINT_DESCRIPTOR_TYPE:
            const treeRoot = lastInterfaceItem || rawDataTreeRoot;
            this.renderEndpointDescriptor_(
                treeRoot, rawDataByteElement, data, offset, indexEndpoint);
            indexEndpoint++;
            break;
          case BOS_DESCRIPTOR_TYPE:
            this.renderBosDescriptor_(
                rawDataTreeRoot, rawDataByteElement, data, offset);
            expectNumDevCapabilities =
                data[BOS_DESCRIPTOR_NUM_DEVICE_CAPABILITIES_OFFSET];
            break;
          case DEVICE_CAPABILITY_DESCRIPTOR_TYPE:
            await this.renderDeviceCapabilityDescriptor_(
                rawDataTreeRoot, rawDataByteElement, data, offset,
                indexDevCapability);
            indexDevCapability++;
            break;
          default:
            this.renderUnknownDescriptor_(
                rawDataTreeRoot, rawDataByteElement, data, offset,
                indexUnknown);
            indexUnknown++;
            break;
        }
        offset += length;
      }

      if (expectNumInterfaces != indexInterface) {
        showError(
            `Expected to find ${expectNumInterfaces} interface descriptors ` +
                `but only encountered ${indexInterface}.`,
            this.rootElement_);
      }

      if (expectNumEndpoints != indexEndpoint) {
        showError(
            `Expected to find ${expectNumEndpoints} interface descriptors ` +
                `but only encountered ${indexEndpoint}.`,
            this.rootElement_);
      }

      if (expectNumDevCapabilities != indexDevCapability) {
        showError(
            `Expected to find ${expectNumDevCapabilities} ` +
                `device capability descriptors but only encountered ${
                    indexDevCapability}.`,
            this.rootElement_);
      }
      addMappingAction(rawDataTreeRoot, rawDataByteElement);
    }

    /**
     * Gets device descriptor of current device, and display it.
     */
    async getDeviceDescriptor() {
      /** @type {!device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {
        type: device.mojom.UsbControlTransferType.STANDARD,
        recipient: device.mojom.UsbControlTransferRecipient.DEVICE,
        request: GET_DESCRIPTOR_REQUEST,
        value: DEVICE_DESCRIPTOR_TYPE << 8,
        index: 0,
      };

      try {
        await this.usbDeviceProxy_.open();
        const response = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, DEVICE_DESCRIPTOR_LENGTH,
            CONTROL_TRANSFER_TIMEOUT_MS);
        checkTransferSuccess(
            response.status, 'Failed to read the device descriptor.',
            this.rootElement_);
        this.renderStandardDescriptor_(new Uint8Array(response.data));
      } catch (e) {
        showError(e.message, this.rootElement_);
      } finally {
        await this.usbDeviceProxy_.close();
      }
    }

    /**
     * Renders a view to display device descriptor hex data in both tree view
     * and raw form.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the device descriptor.
     * @private
     */
    async renderDeviceDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset) {
      const fields = [
        {
          label: `Length (should be ${DEVICE_DESCRIPTOR_LENGTH}): `,
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type (should be 0x01): ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'USB Version: ',
          size: 2,
          formatter: formatUsbVersion,
        },
        {
          label: 'Class Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Subclass Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Protocol Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Control Pipe Maximum Packet Size: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Vendor ID: ',
          size: 2,
          formatter: formatTwoBytesToHex,
        },
        {
          label: 'Product ID: ',
          size: 2,
          formatter: formatTwoBytesToHex,
        },
        {
          label: 'Device Version: ',
          size: 2,
          formatter: formatUsbVersion,
        },
        {
          label: 'Manufacturer String Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
        {
          label: 'Product String Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
        {
          label: 'Serial Number Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
        {
          label: 'Number of Configurations: ',
          size: 1,
          formatter: formatByte,
        },
      ];

      renderRawDataTree(
          rawDataTreeRoot, rawDataByteElement, fields, rawData, offset,
          this.rootElement_);

      // window.deviceDescriptorCompleteFn() provides a hook for the test suite
      // to perform test actions after the device descriptor is rendered.
      window.deviceDescriptorCompleteFn();
    }

    /**
     * Gets configuration descriptor of current device, and display it.
     */
    async getConfigurationDescriptor() {
      /** @type {!device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {
        type: device.mojom.UsbControlTransferType.STANDARD,
        recipient: device.mojom.UsbControlTransferRecipient.DEVICE,
        request: GET_DESCRIPTOR_REQUEST,
        value: CONFIGURATION_DESCRIPTOR_TYPE << 8,
        index: 0,
      };

      try {
        await this.usbDeviceProxy_.open();
        let response = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, CONFIGURATION_DESCRIPTOR_LENGTH,
            CONTROL_TRANSFER_TIMEOUT_MS);
        checkTransferSuccess(
            response.status,
            'Failed to read the device configuration descriptor to determine ' +
                'the total descriptor length.',
            this.rootElement_);
        const dataView = new DataView(new Uint8Array(response.data).buffer);
        const length = dataView.getUint16(
            CONFIGURATION_DESCRIPTOR_TOTAL_LENGTH_OFFSET, true);
        // Re-gets the data using the full length.
        response = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, length, CONTROL_TRANSFER_TIMEOUT_MS);
        checkTransferSuccess(
            response.status,
            'Failed to read the complete configuration descriptor.',
            this.rootElement_);
        this.renderStandardDescriptor_(new Uint8Array(response.data));
      } catch (e) {
        showError(e.message, this.rootElement_);
      } finally {
        await this.usbDeviceProxy_.close();
      }
    }

    /**
     * Renders a view to display configuration descriptor hex data in both tree
     * view and raw form.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the configuration descriptor.
     * @private
     */
    async renderConfigurationDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset) {
      const fields = [
        {
          label: `Length (should be ${CONFIGURATION_DESCRIPTOR_LENGTH}): `,
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type (should be 0x02): ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Total Length: ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Number of Interfaces: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Configuration Value: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Configuration String Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
        {
          label: 'Attribute Bitmap: ',
          size: 1,
          formatter: formatBitmap,
        },
        {
          label: 'Max Power (2mA increments): ',
          size: 1,
          formatter: formatByte,
        },
      ];

      renderRawDataTree(
          rawDataTreeRoot, rawDataByteElement, fields, rawData, offset,
          this.rootElement_);
    }

    /**
     * Renders a tree item to display interface descriptor at index
     * indexInterface.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the interface
     *     descriptor.
     * @param {number} indexInterface
     * @return {!cr.ui.TreeItem}
     * @private
     */
    renderInterfaceDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset, indexInterface) {
      const parentClassName = `descriptor-interface-${indexInterface}`;
      const interfaceItem =
          customTreeItem(`Interface ${indexInterface}`, parentClassName);
      rawDataTreeRoot.add(interfaceItem);

      const fields = [
        {
          label: `Length (should be ${INTERFACE_DESCRIPTOR_LENGTH}): `,
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type (should be 0x04): ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Interface Number: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Alternate String: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Number of Endpoint: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Class Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Subclass Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Protocol Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface String Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
      ];

      renderRawDataTree(
          interfaceItem, rawDataByteElement, fields, rawData, offset,
          this.rootElement_, parentClassName);

      return interfaceItem;
    }

    /**
     * Renders a tree item to display endpoint descriptor at index
     * indexEndpoint.
     * @param {!cr.ui.Tree|!cr.ui.TreeItem} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the endpoint
     *     descriptor.
     * @param {number} indexEndpoint
     * @private
     */
    renderEndpointDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset, indexEndpoint) {
      const parentClassName = `descriptor-endpoint-${indexEndpoint}`;
      const endpointItem =
          customTreeItem(`Endpoint ${indexEndpoint}`, parentClassName);
      rawDataTreeRoot.add(endpointItem);

      const fields = [
        {
          label: `Length (should be ${ENDPOINT_DESCRIPTOR_LENGTH}): `,
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type (should be 0x05): ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'EndPoint Address: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Attribute Bitmap: ',
          size: 1,
          formatter: formatBitmap,
        },
        {
          label: 'Max Packet Size: ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Interval: ',
          size: 1,
          formatter: formatByte,
        },
      ];

      renderRawDataTree(
          endpointItem, rawDataByteElement, fields, rawData, offset,
          this.rootElement_, parentClassName);
    }

    /**
     * Renders a tree item to display length and type of unknown descriptor at
     * index indexUnknown.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the this descriptor.
     * @param {number} indexUnknown
     * @private
     */
    renderUnknownDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexUnknown) {
      const length =
          rawData[originalOffset + STANDARD_DESCRIPTOR_LENGTH_OFFSET];
      const parentClassName = `descriptor-unknown-${indexUnknown}`;
      const unknownItem =
          customTreeItem(`Unknown Descriptor ${indexUnknown}`, parentClassName);
      rawDataTreeRoot.add(unknownItem);

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
      ];

      let offset = renderRawDataTree(
          unknownItem, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      const rawDataByteElements = rawDataByteElement.querySelectorAll('span');

      for (; offset < originalOffset + length; offset++) {
        rawDataByteElements[offset].classList.add(`field-offset-${offset}`);
        rawDataByteElements[offset].classList.add(parentClassName);
      }
    }

    /**
     * Gets all the supported language codes of this device, and adds them as
     * autocompletions for the language code input area in the string descriptor
     * panel.
     * @return {!Promise<!Array<number>>}
     */
    async getAllLanguageCodes() {
      /** @type {!device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {
        type: device.mojom.UsbControlTransferType.STANDARD,
        recipient: device.mojom.UsbControlTransferRecipient.DEVICE,
        request: GET_DESCRIPTOR_REQUEST,
        value: STRING_DESCRIPTOR_TYPE << 8,
        index: 0,
      };

      let response;
      try {
        await this.usbDeviceProxy_.open();

        response = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, MAX_STRING_DESCRIPTOR_LENGTH,
            CONTROL_TRANSFER_TIMEOUT_MS);

        checkTransferSuccess(
            response.status,
            'Failed to read the device string descriptor to determine ' +
                'all supported languages.',
            this.rootElement_);
      } catch (e) {
        showError(e.message, this.rootElement_);
        // Stop rendering autocomplete datalist if failed to read the string
        // descriptor.
        return [];
      } finally {
        await this.usbDeviceProxy_.close();
      }

      const responseData = new Uint8Array(response.data);
      this.languageCodesListElement_.innerText = '';

      const optionAllElement = document.createElement('option');
      optionAllElement.value = 'All';
      this.languageCodesListElement_.appendChild(optionAllElement);

      const languageCodesList = [];
      // First two bytes are length and descriptor type(0x03);
      for (let i = 2; i < responseData.length; i += 2) {
        const languageCode = parseShort(responseData, i);

        const optionElement = document.createElement('option');
        optionElement.label = parseLanguageCode(languageCode);
        optionElement.value = `0x${toHex(languageCode, 4)}`;

        this.languageCodesListElement_.appendChild(optionElement);

        languageCodesList.push(languageCode);
      }
      return languageCodesList;
    }

    /**
     * Gets the string descriptor for the current device with the given index
     * and language code, and display it.
     * @param {number} index
     * @param {number} languageCode
     * @param {!cr.ui.TreeItem=} treeItem
     * @private
     */
    async getStringDescriptorForLanguageCode_(
        index, languageCode, treeItem = undefined) {
      /** @type {!device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {
        type: device.mojom.UsbControlTransferType.STANDARD,
        recipient: device.mojom.UsbControlTransferRecipient.DEVICE,
        request: GET_DESCRIPTOR_REQUEST,
        index: languageCode,
        value: (STRING_DESCRIPTOR_TYPE << 8) | index,
      };

      try {
        await this.usbDeviceProxy_.open();
        const response = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, MAX_STRING_DESCRIPTOR_LENGTH,
            CONTROL_TRANSFER_TIMEOUT_MS);
        checkTransferSuccess(
            response.status,
            `Failed to read the device string descriptor of index: ${
                index}, language: ${parseLanguageCode(languageCode)}.`,
            this.rootElement_);

        this.indexInput_.value = index;
        this.renderStandardDescriptor_(
            new Uint8Array(response.data), languageCode, treeItem);
      } catch (e) {
        showError(e.message, this.rootElement_);
      } finally {
        await this.usbDeviceProxy_.close();
      }
    }

    /**
     * Renders string descriptor of current device with given index and language
     * code.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the string descriptor.
     * @param {number=} languageCode
     * @param {cr.ui.TreeItem=} treeItem
     * @private
     */
    renderStringDescriptorForLanguageCode_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset, languageCode = 0,
        treeItem = undefined) {
      this.rootElement_.hidden = false;

      const languageStr = parseLanguageCode(languageCode);

      const fields = [
        {
          'label': 'Length: ',
          'size': 1,
          'formatter': formatByte,
        },
        {
          'label': 'Descriptor Type (should be 0x03): ',
          'size': 1,
          'formatter': formatDescriptorType,
        },
      ];

      // The first two elements are length and descriptor type.
      for (let i = 2; i < rawData.length; i += 2) {
        const field = {
          'label': '',
          'size': 2,
          'formatter': formatLetter,
        };
        fields.push(field);
      }

      // The first two elements of rawData are length and descriptor type.
      const stringDescriptor = decodeUtf16Array(rawData.slice(2), true);
      const parentClassName = `descriptor-string-language-${languageStr}`;
      const stringDescriptorItem = customTreeItem(
          `${languageStr}: ${stringDescriptor}`, parentClassName);
      rawDataTreeRoot.add(stringDescriptorItem);
      if (treeItem) {
        treeItem.add(customTreeItem(`${languageStr}: ${stringDescriptor}`));
        treeItem.expanded = true;
      }

      renderRawDataTree(
          stringDescriptorItem, rawDataByteElement, fields, rawData, offset,
          this.rootElement_, parentClassName);
    }

    /**
     * Gets string descriptor in all supported languages of current device with
     * given index.
     * @param {number} index
     * @param {cr.ui.TreeItem=} treeItem
     * @private
     */
    async getStringDescriptorForAllLanguages_(index, treeItem = undefined) {
      this.rootElement_.hidden = false;

      this.indexInput_.value = index;

      /** @type {!Array<number>} */
      const languageCodesList = await this.getAllLanguageCodes();

      for (const languageCode of languageCodesList) {
        await this.getStringDescriptorForLanguageCode_(
            index, languageCode, assert(treeItem));
      }
    }

    /**
     * Initializes the string descriptor panel for autocomplete functionality.
     * @param {string} tabId
     */
    initialStringDescriptorPanel(tabId) {
      // Binds the input area and datalist use each tab's unique id.
      this.rootElement_.querySelectorAll('input').forEach(
          el => el.setAttribute('list', `${el.getAttribute('list')}-${tabId}`));
      this.rootElement_.querySelectorAll('datalist')
          .forEach(el => el.id = `${el.id}-${tabId}`);

      /** @type {!HTMLElement} */
      const button = queryRequiredElement('button', this.rootElement_);
      /** @type {!HTMLElement} */
      this.indexInput_ =
          queryRequiredElement('#index-input', this.rootElement_);
      /** @type {!HTMLElement} */
      const languageCodeInput =
          queryRequiredElement('#language-code-input', this.rootElement_);

      button.addEventListener('click', async () => {
        this.clearView();
        const index = Number.parseInt(this.indexInput_.value, 10);
        if (this.checkParamValid_(index, 'Index', 1, 255)) {
          if (languageCodeInput.value === 'All') {
            await this.getStringDescriptorForAllLanguages_(index);
          } else {
            const languageCode = Number.parseInt(languageCodeInput.value, 10);
            if (this.checkParamValid_(
                    languageCode, 'Language Code', 0, 65535)) {
              await this.getStringDescriptorForLanguageCode_(
                  index, languageCode);
            }
          }
        }
      });

      /** @type {!Set<number>} */
      this.stringDescriptorIndexes = new Set();
      /** @type {!HTMLElement} */
      this.indexesListElement =
          queryRequiredElement(`#indexes-${tabId}`, this.rootElement_);
      /** @type {!HTMLElement} */
      this.languageCodesListElement_ =
          queryRequiredElement(`#languages-${tabId}`, this.rootElement_);
    }

    /**
     * Gets the Binary device Object Store (BOS) descriptor of the current
     * device, which contains the WebUSB descriptor and Microsoft OS 2.0
     * descriptor, and display it.
     */
    async getBosDescriptor() {
      /** @type {!device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {
        type: device.mojom.UsbControlTransferType.STANDARD,
        recipient: device.mojom.UsbControlTransferRecipient.DEVICE,
        request: GET_DESCRIPTOR_REQUEST,
        value: BOS_DESCRIPTOR_TYPE << 8,
        index: 0,
      };

      try {
        await this.usbDeviceProxy_.open();
        let response = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, BOS_DESCRIPTOR_HEADER_LENGTH,
            CONTROL_TRANSFER_TIMEOUT_MS);
        checkTransferSuccess(
            response.status,
            'Failed to read the device BOS descriptor to determine ' +
                'the total descriptor length.',
            this.rootElement_);
        const dataView = new DataView(new Uint8Array(response.data).buffer);
        const length =
            dataView.getUint16(BOS_DESCRIPTOR_TOTAL_LENGTH_OFFSET, true);
        // Re-gets the data using the full length.
        response = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, length, CONTROL_TRANSFER_TIMEOUT_MS);
        checkTransferSuccess(
            response.status, 'Failed to read the complete BOS descriptor.',
            this.rootElement_);
        await this.renderStandardDescriptor_(new Uint8Array(response.data));
      } catch (e) {
        showError(e.message, this.rootElement_);
      } finally {
        await this.usbDeviceProxy_.close();
      }
    }

    /**
     * Renders a view to display Binary device Object Store (BOS) descriptor hex
     * data in both tree view and raw form.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the BOS descriptor.
     * @private
     */
    renderBosDescriptor_(rawDataTreeRoot, rawDataByteElement, rawData, offset) {
      const fields = [
        {
          'label': 'Length (should be 5): ',
          'size': 1,
          'formatter': formatByte,
        },
        {
          'label': 'Descriptor Type (should be 0x0F): ',
          'size': 1,
          'formatter': formatDescriptorType,
        },
        {
          'label': 'Total Length: ',
          'size': 2,
          'formatter': formatShort,
        },
        {
          'label': 'Number of Device Capability Descriptors: ',
          'size': 1,
          'formatter': formatByte,
        },
      ];

      renderRawDataTree(
          rawDataTreeRoot, rawDataByteElement, fields, rawData, offset,
          this.rootElement_);
    }

    /**
     * Renders a view to display device capability descriptor hex data in both
     * tree view and raw form.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the BOS descriptor.
     * @param {number} indexDevCapability
     * @private
     */
    async renderDeviceCapabilityDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset,
        indexDevCapability) {
      switch (rawData[offset + BOS_DESCRIPTOR_DEVICE_CAPABILITY_TYPE_OFFSET]) {
        case DEVICE_CAPABILITY_DESCRIPTOR_TYPE_PLATFORM_TYPE:
          if (isSameUuid(rawData, offset, WEB_USB_CAPABILITY_UUID)) {
            this.renderWebUsbPlatformDescriptor_(
                rawDataTreeRoot, rawDataByteElement, rawData, offset,
                indexDevCapability);
            break;
          } else if (isSameUuid(
                         rawData, offset, MS_OS_20_PLATFORM_CAPABILITY_UUID)) {
            this.renderMsOs20PlatformDescriptor_(
                rawDataTreeRoot, rawDataByteElement, rawData, offset,
                indexDevCapability);
            break;
          }
        default:
          this.renderUnknownBosDescriptor_(
              rawDataTreeRoot, rawDataByteElement, rawData, offset,
              indexDevCapability);
      }
    }

    /**
     * Renders a tree item to display WebUSB platform capability descriptor at
     * index indexWebUsb.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the WebUSB platform
     *     capability descriptor.
     * @param {number} indexWebUsb
     * @private
     */
    renderWebUsbPlatformDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset, indexWebUsb) {
      const parentClassName = `descriptor-webusb-${indexWebUsb}`;
      const webUsbItem = customTreeItem('WebUSB Descriptor', parentClassName);
      rawDataTreeRoot.add(webUsbItem);

      const fields = [
        {
          label: 'Length (should be 24): ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type (should be 0x10): ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Device Capability Descriptor Type (should be 0x05): ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Reserved (should be 0): ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Platform Capability UUID: ',
          size: 16,
          formatter: formatUuid,
        },
        {
          label: 'Protocol Version Supported (should be 1.0.0): ',
          size: 2,
          formatter: formatUsbVersion,
        },
        {
          label: 'Vendor Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Landing Page: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderUrlDescriptorIndexItem_.bind(this),
        },
      ];
      renderRawDataTree(
          webUsbItem, rawDataByteElement, fields, rawData, offset,
          this.rootElement_, parentClassName);
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 platform capability
     * descriptor at index indexMsOs20.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the Microsoft OS 2.0 platform
     *     capability descriptor.
     * @param {number} indexMsOs20
     * @private
     */
    renderMsOs20PlatformDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset, indexMsOs20) {
      const parentClassName = `descriptor-ms-os-20-${indexMsOs20}`;
      const msOs20Item =
          customTreeItem(`Microsoft OS 2.0 Descriptor`, parentClassName);
      rawDataTreeRoot.add(msOs20Item);

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type (should be 0x10): ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Device Capability Descriptor Type (should be 0x05): ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Reserved (should be 0): ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Platform Capability UUID: ',
          size: 16,
          formatter: formatUuid,
        },
      ];

      offset = renderRawDataTree(
          msOs20Item, rawDataByteElement, fields, rawData, offset,
          this.rootElement_, parentClassName);

      let indexMsOs20DescriptorSetInfo = 0;
      // Continue parsing while there are still unparsed Microsoft OS 2.0
      // descriptor set information structures. Stop if accessing the descriptor
      // set information structure would cause us to read past the end of the
      // buffer.
      while (offset < rawData.length) {
        offset = this.renderMsOs20DescriptorSetInfo_(
            msOs20Item, rawDataByteElement, rawData, offset,
            indexMsOs20DescriptorSetInfo, indexMsOs20);
        indexMsOs20DescriptorSetInfo++;
      }
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 descriptor set
     * information at index indexMsOs20DescriptorSetInfo.
     * @param {!cr.ui.Tree|!cr.ui.TreeItem} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} offset The start offset of the Microsoft OS 2.0
     *     set information structure.
     * @param {number} indexMsOs20DescriptorSetInfo
     * @param {number} indexMsOs20
     * @return {number}
     * @private
     */
    renderMsOs20DescriptorSetInfo_(
        rawDataTreeRoot, rawDataByteElement, rawData, offset,
        indexMsOs20DescriptorSetInfo, indexMsOs20) {
      const parentClassName =
          `descriptor-ms-os-20-set-info-${indexMsOs20DescriptorSetInfo}`;
      const msOs20SetInfoItem = customTreeItem(
          `Microsoft OS 2.0 Descriptor Set Information`, parentClassName);
      rawDataTreeRoot.add(msOs20SetInfoItem);

      const fields = [
        {
          label: 'Windows Version: ',
          size: 4,
          formatter: formatWindowsVersion,
        },
        {
          label: 'Total Length: ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Vendor Code: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: (rawData, offset, item, fieldLabel) =>
              this.renderMsOs20DescriptorVendorSpecific_(
                  rawData, offset - MS_OS_20_VENDOR_CODE_ITEM_OFFSET, item,
                  fieldLabel),
        },
        {
          label: 'Alternate Enumeration Code: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: (rawData, offset, item, fieldLabel) =>
              this.renderMsOs20DescriptorSetAltEnum_(
                  rawData, offset - MS_OS_20_ALT_ENUM_CODE_ITEM_OFFSET, item,
                  fieldLabel),
        },
      ];

      return renderRawDataTree(
          msOs20SetInfoItem, rawDataByteElement, fields, rawData, offset,
          this.rootElement_, parentClassName,
          `descriptor-ms-os-20-${indexMsOs20}`);
    }

    /**
     * Renders a tree item to display unknown device capability descriptor at
     * indexUnknownDevCapability
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the unknown device
     *     capability descriptor.
     * @param {number} indexUnknownDevCapability
     * @private
     */
    renderUnknownBosDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexUnknownDevCapability) {
      const length =
          rawData[originalOffset + STANDARD_DESCRIPTOR_LENGTH_OFFSET];

      const parentClassName =
          `descriptor-unknownbos-${indexUnknownDevCapability}`;
      const unknownBosItem =
          customTreeItem(`Unknown BOS Descriptor`, parentClassName);
      rawDataTreeRoot.add(unknownBosItem);

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Device Capability Descriptor Type: ',
          size: 1,
          formatter: formatByte,
        },
      ];

      let offset = renderRawDataTree(
          unknownBosItem, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      const rawDataByteElements = rawDataByteElement.querySelectorAll('span');

      for (; offset < originalOffset + length; offset++) {
        rawDataByteElements[offset].classList.add(`field-offset-${offset}`);
        rawDataByteElements[offset].classList.add(parentClassName);
      }
    }

    /**
     * Gets the URL Descriptor, renders a URL descriptor item and adds it to
     * the URL descriptor index item.
     * @param {!Uint8Array} rawData
     * @param {number} offset The offset of the WebUSB descriptor.
     * @param {!cr.ui.TreeItem} item The URL descriptor index item.
     * @private
     */
    async getUrlDescriptor_(rawData, offset, item) {
      // The second to last byte is the vendor code used to query URL
      // descriptor. Last byte is index of url descriptor. These are defined by
      // the WebUSB specification: http://wicg.github.io/webusb/
      const vendorCode = rawData[offset + WEB_USB_VENDOR_CODE_OFFSET];
      const urlIndex = rawData[offset + WEB_USB_URL_DESCRIPTOR_INDEX_OFFSET];

      /** @type {!device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {
        recipient: device.mojom.UsbControlTransferRecipient.DEVICE,
        // These constants are defined by the WebUSB specification:
        // http://wicg.github.io/webusb/
        type: device.mojom.UsbControlTransferType.VENDOR,
        request: vendorCode,
        value: urlIndex,
        index: GET_URL_REQUEST,
      };

      try {
        await this.usbDeviceProxy_.open();
        // Gets the URL descriptor.
        const urlResponse = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, MAX_URL_DESCRIPTOR_LENGTH,
            CONTROL_TRANSFER_TIMEOUT_MS);

        checkTransferSuccess(
            urlResponse.status, 'Failed to read the device URL descriptor.',
            this.rootElement_);

        let url;
        // URL Prefixes are defined by Chapter 4.3.1 of the WebUSB
        // specification: http://wicg.github.io/webusb/
        switch (urlResponse.data[2]) {
          case 0:
            url = 'http://';
            break;
          case 1:
            url = 'https://';
            break;
          case 255:
          default:
            url = '';
        }
        // The first three elements of urlResponse.data are length, descriptor
        // type and URL scheme prefix.
        url += decodeUtf8Array(new Uint8Array(urlResponse.data.slice(3)));

        const landingPageItem = customTreeItem(url, 'descriptor-url');
        landingPageItem.labelElement.addEventListener(
            'click', () => window.open(url, '_blank'));
        item.add(landingPageItem);
        item.expanded = true;
      } catch (e) {
        showError(e.message, this.rootElement_);
        // Stops parsing to string format URL if failed to read the URL
        // descriptor.
        return '';
      } finally {
        await this.usbDeviceProxy_.close();
      }
    }

    /**
     * Gets the Microsoft OS 2.0 Descriptor vendor-specific descriptor.
     * @param {number} vendorCode
     * @return {!Promise<!Uint8Array>}
     * @private
     */
    async getMsOs20DescriptorSet_(vendorCode, msOs20DescriptorSetLength) {
      /** @type {!device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {
        recipient: device.mojom.UsbControlTransferRecipient.DEVICE,
        // These constants are defined by Microsoft OS 2.0 Descriptors
        // Specification (July, 2018).
        type: device.mojom.UsbControlTransferType.VENDOR,
        request: vendorCode,
        value: 0,
        index: MS_OS_20_DESCRIPTOR_INDEX,
      };

      let response;
      try {
        await this.usbDeviceProxy_.open();
        // Gets the Microsoft OS 2.0 descriptor set.
        response = await this.usbDeviceProxy_.controlTransferIn(
            usbControlTransferParams, msOs20DescriptorSetLength,
            CONTROL_TRANSFER_TIMEOUT_MS);

        checkTransferSuccess(
            response.status,
            'Failed to read the Microsoft OS 2.0 descriptor set.',
            this.rootElement_);
      } catch (e) {
        showError(e.message, this.rootElement_);
        // Returns an empty array if failed to read the Microsoft OS 2.0
        // descriptor set.
        return new Uint8Array(0);
      } finally {
        await this.usbDeviceProxy_.close();
      }

      return new Uint8Array(response.data);
    }

    /**
     * Sends the Microsoft OS 2.0 Descriptor set alternate enumeration command.
     * @param {number} vendorCode
     * @param {number} altEnumCode
     * @private
     */
    async sendMsOs20DescriptorSetAltEnumCommand_(vendorCode, altEnumCode) {
      /** @type {!device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {
        recipient: device.mojom.UsbControlTransferRecipient.DEVICE,
        // These constants are defined by Microsoft OS 2.0 Descriptors
        // Specification (July, 2018).
        type: device.mojom.UsbControlTransferType.VENDOR,
        request: vendorCode,
        value: altEnumCode,
        index: MS_OS_20_SET_ALT_ENUMERATION,
      };

      try {
        await this.usbDeviceProxy_.open();
        // Sends the Microsoft OS 2.0 descriptor set alternate enumeration
        // command. It doesn't need extra bytes to send the device in the body
        // of the request.
        const response = await this.usbDeviceProxy_.controlTransferOut(
            usbControlTransferParams, [], CONTROL_TRANSFER_TIMEOUT_MS);

        checkTransferSuccess(
            response.status,
            'Failed to read the Microsoft OS 2.0 descriptor ' +
                'alternate enumeration set.',
            this.rootElement_);
      } catch (e) {
        showError(e.message, this.rootElement_);
      } finally {
        await this.usbDeviceProxy_.close();
      }
    }

    /**
     * Renders a view to display Microsoft OS 2.0 Descriptor Set hex data in
     * both tree view and raw form.
     * @param {!Uint8Array} msOs20RawData
     * @private
     */
    renderMsOs20DescriptorSet_(msOs20RawData) {
      const displayElement = addNewDescriptorDisplayElement(
          this.rootElement_, 'Microsoft OS 2.0 Descriptor Set');
      /** @type {!cr.ui.Tree} */
      const rawDataTreeRoot = displayElement.rawDataTreeRoot;
      /** @type {!HTMLElement} */
      const rawDataByteElement = displayElement.rawDataByteElement;
      renderRawDataBytes(rawDataByteElement, msOs20RawData);

      let msOs20DescriptorOffset = 0;
      let indexMsOs20Descriptor = 0;
      const data = new DataView(msOs20RawData.buffer);
      // Continue parsing while there are still unparsed Microsoft OS 2.0
      // Descriptor Set. Stop if accessing the descriptor type (two bytes)
      // would cause us to read past the end of the buffer.
      while (msOs20DescriptorOffset + MS_OS_20_DESCRIPTOR_TYPE_OFFSET + 1 <
             msOs20RawData.length) {
        const msOs20DescriptorType = data.getUint16(
            msOs20DescriptorOffset + MS_OS_20_DESCRIPTOR_TYPE_OFFSET, true);
        switch (msOs20DescriptorType) {
          case MS_OS_20_SET_HEADER_DESCRIPTOR:
            msOs20DescriptorOffset = this.renderMsOs20SetHeader_(
                rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                msOs20DescriptorOffset);
            break;
          case MS_OS_20_SUBSET_HEADER_CONFIGURATION:
            msOs20DescriptorOffset =
                this.renderMsOs20ConfigurationSubsetHeader_(
                    rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                    msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
            break;
          case MS_OS_20_SUBSET_HEADER_FUNCTION:
            msOs20DescriptorOffset = this.renderMsOs20FunctionSubsetHeader_(
                rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
            break;
          case MS_OS_20_FEATURE_COMPATIBLE_ID:
            msOs20DescriptorOffset = this.renderMsOs20FeatureCompatibleId_(
                rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
            break;
          case MS_OS_20_FEATURE_REG_PROPERTY:
            msOs20DescriptorOffset = this.renderMsOs20FeatureRegistryProperty_(
                rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
            break;
          case MS_OS_20_FEATURE_MIN_RESUME_TIME:
            msOs20DescriptorOffset = this.renderMsOs20FeatureMinResumeTime_(
                rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
            break;
          case MS_OS_20_FEATURE_MODEL_ID:
            msOs20DescriptorOffset = this.renderMsOs20FeatureModelId_(
                rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
            break;
          case MS_OS_20_FEATURE_CCGP_DEVICE:
            msOs20DescriptorOffset = this.renderMsOs20FeatureCcgpDevice_(
                rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
            break;
          case MS_OS_20_FEATURE_VENDOR_REVISION:
            msOs20DescriptorOffset = this.renderMsOs20FeatureVendorRevision_(
                rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
            break;
          default:
            msOs20DescriptorOffset =
                this.renderUnknownMsOs20DescriptorDescriptor_(
                    rawDataTreeRoot, rawDataByteElement, msOs20RawData,
                    msOs20DescriptorOffset, indexMsOs20Descriptor);
            indexMsOs20Descriptor++;
        }
      }
      addMappingAction(rawDataTreeRoot, rawDataByteElement);
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 descriptor set header.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     descriptor set header.
     * @return {number}
     * @private
     */
    renderMsOs20SetHeader_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset) {
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length (should be 10): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 0): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Windows Version: ',
          size: 4,
          formatter: formatWindowsVersion,
        },
        {
          label: 'Total Length: ',
          size: 2,
          formatter: formatShort,
        },
      ];

      const offset = renderRawDataTree(
          rawDataTreeRoot, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_);

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 Descriptor ' +
                'Set header.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 configuration subset
     * header.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     configuration subset header.
     * @param {number} indexMsOs20Descriptor
     * @return {number}
     * @private
     */
    renderMsOs20ConfigurationSubsetHeader_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexMsOs20Descriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexMsOs20Descriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Configuration Subset Header', parentClassName);
      rawDataTreeRoot.add(item);

      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length (should be 8): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 1): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Configuration Value: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Reserved (should be 0): ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Total Length: ',
          size: 2,
          formatter: formatShort,
        },
      ];

      const offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'Configuration Subset Header.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 function subset header.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     function subset header.
     * @param {number} indexMsOs20Descriptor
     * @return {number}
     * @private
     */
    renderMsOs20FunctionSubsetHeader_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexMsOs20Descriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexMsOs20Descriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Function Subset Header', parentClassName);
      rawDataTreeRoot.add(item);
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length (should be 8): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 2): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'First Interface Number: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Reserved (should be 0): ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Total Length: ',
          size: 2,
          formatter: formatShort,
        },
      ];

      const offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'Function Subset Header.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 compatible ID Descriptor.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     compatible ID descriptor.
     * @param {number} indexMsOs20Descriptor
     * @return {number}
     * @private
     */
    renderMsOs20FeatureCompatibleId_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexMsOs20Descriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexMsOs20Descriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Compatible ID Descriptor', parentClassName);
      rawDataTreeRoot.add(item);
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length (should be 20): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 3): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Compatible ID String: ',
          size: 8,
          formatter: formatCompatibleIdString,
        },
        {
          label: 'Sub-compatible ID String: ',
          size: 8,
          formatter: formatCompatibleIdString,
        },
      ];

      const offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'Compatible ID Descriptor.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 registry property
     * descriptor.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     registry property descriptor.
     * @param {number} indexMsOs20Descriptor
     * @return {number}
     * @private
     */
    renderMsOs20FeatureRegistryProperty_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexMsOs20Descriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexMsOs20Descriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Registry Property Descriptor', parentClassName);
      rawDataTreeRoot.add(item);
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);
      const featureRegistryPropertyDataType = data.getUint16(
          originalOffset +
              MS_OS_20_REGISTRY_PROPERTY_DESCRIPTOR_PROPERTY_DATA_TYPE_OFFSET,
          true);
      const propertyNameLength = data.getUint16(
          originalOffset +
              MS_OS_20_REGISTRY_PROPERTY_DESCRIPTOR_NAME_LENGTH_OFFSET,
          true);
      const fields = [
        {
          label: 'Length: ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 4): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Property Data Type: ',
          size: 2,
          formatter: formatFeatureRegistryPropertyDataType,
        },
        {
          label: 'Property Name Length: ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Property Name: ',
          size: propertyNameLength,
          formatter: formatUnknown,
          extraTreeItemFormatter: (rawData, offset, item, fieldLabel) =>
              this.renderFeatureRegistryPropertyDataItem_(
                  rawData, offset, item, fieldLabel,
                  featureRegistryPropertyDataType, propertyNameLength),
        },
      ];

      let offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      while (offset < originalOffset + length) {
        const propertyDataLength = data.getUint16(offset, true);
        const propertyDataFields = [
          {
            label: 'Property Data Length: ',
            size: 2,
            formatter: formatShort,
          },
          {
            label: 'Property Data: ',
            size: propertyDataLength,
            formatter: formatUnknown,
            extraTreeItemFormatter: (rawData, offset, item, fieldLabel) =>
                this.renderFeatureRegistryPropertyDataItem_(
                    rawData, offset, item, fieldLabel,
                    featureRegistryPropertyDataType, propertyDataLength),
          },
        ];
        offset = renderRawDataTree(
            item, rawDataByteElement, propertyDataFields, rawData, offset,
            this.rootElement_, parentClassName);
      }

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'Registry Property Descriptor.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 minimum USB resume time
     * descriptor.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     minimum USB resume time descriptor.
     * @param {number} indexMsOs20Descriptor
     * @return {number}
     * @private
     */
    renderMsOs20FeatureMinResumeTime_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexMsOs20Descriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexMsOs20Descriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Minimum USB Resume Time Descriptor',
          parentClassName);
      rawDataTreeRoot.add(item);
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length (should be 6): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 5): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Resume Recovery Time (milliseconds): ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Resume Signaling Time (milliseconds): ',
          size: 1,
          formatter: formatByte,
        },
      ];

      const offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'Minimum USB Resume Time Descriptor.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 model ID descriptor.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     model ID descriptor.
     * @param {number} indexMsOs20Descriptor
     * @return {number}
     * @private
     */
    renderMsOs20FeatureModelId_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexMsOs20Descriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexMsOs20Descriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Model ID Descriptor', parentClassName);
      rawDataTreeRoot.add(item);
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length (should be 20): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 6): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Model ID: ',
          size: 16,
          formatter: formatUuid,
        },
      ];

      const offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'Model ID Descriptor.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 Common Class Generic
     * Parent (CCGP) device descriptor.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     CCGP device descriptor.
     * @param {number} indexMsOs20Descriptor
     * @return {number}
     * @private
     */
    renderMsOs20FeatureCcgpDevice_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexMsOs20Descriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexMsOs20Descriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Common Class Generic Parent (CCGP) Device ' +
              'Descriptor',
          parentClassName);
      rawDataTreeRoot.add(item);
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length (should be 4): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 7): ',
          size: 2,
          formatter: formatShort,
        },
      ];

      const offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'CCGP Device Descriptor.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display Microsoft OS 2.0 vendor revision
     * descriptor.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the Microsoft OS 2.0
     *     vendor revision descriptor.
     * @param {number} indexMsOs20Descriptor
     * @return {number}
     * @private
     */
    renderMsOs20FeatureVendorRevision_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexMsOs20Descriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexMsOs20Descriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Vendor Revision Descriptor', parentClassName);
      rawDataTreeRoot.add(item);
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length (should be 6): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type (should be 8): ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Vendor Revision: ',
          size: 2,
          formatter: formatShort,
        },
      ];

      const offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'Vendor Revision Descriptor.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Renders a tree item to display an unknown Microsoft OS 2.0 descriptor.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset The start offset of the unknown Microsoft
     *     OS 2.0 descriptor.
     * @param {number} indexDescriptor
     * @return {number}
     * @private
     */
    renderUnknownMsOs20DescriptorDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexDescriptor) {
      const parentClassName =
          `descriptor-ms-os-20-subdescriptor-${indexDescriptor}`;
      const item = customTreeItem(
          'Microsoft OS 2.0 Descriptor Unknown Descriptor', parentClassName);
      rawDataTreeRoot.add(item);
      const data = new DataView(rawData.buffer);
      const length = data.getUint16(
          originalOffset + MS_OS_20_DESCRIPTOR_LENGTH_OFFSET, true);

      const fields = [
        {
          label: 'Length: ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'MS OS 2.0 Descriptor Type: ',
          size: 2,
          formatter: formatShort,
        },
      ];

      let offset = renderRawDataTree(
          item, rawDataByteElement, fields, rawData, originalOffset,
          this.rootElement_, parentClassName);

      const rawDataByteElements = rawDataByteElement.querySelectorAll('span');

      for (; offset < originalOffset + length; offset++) {
        rawDataByteElements[offset].classList.add(
            `field-offset-${offset}`, parentClassName);
      }

      if (offset !== originalOffset + length) {
        showError(
            'An error occurred while rendering Microsoft OS 2.0 ' +
                'Unknown Descriptor.',
            this.rootElement_);
      }

      return offset;
    }

    /**
     * Gets response of the given request.
     * @param {!device.mojom.UsbControlTransferParams} usbControlTransferParams
     * @param {number} length
     * @param {string} direction
     * @private
     */
    async sendTestingRequest_(usbControlTransferParams, length, direction) {
      try {
        await this.usbDeviceProxy_.open();

        if (direction === 'Device-to-Host') {
          const response = await this.usbDeviceProxy_.controlTransferIn(
              usbControlTransferParams, length, CONTROL_TRANSFER_TIMEOUT_MS);
          checkTransferSuccess(
              response.status, 'Failed to send request.', this.rootElement_);
          this.renderTestingData_(new Uint8Array(response.data));
        } else if (direction === 'Host-to-Device') {
          const dataString = this.rootElement_.querySelector('textarea').value;

          const data = [];
          for (let i = 0; i < dataString.length; i += 2) {
            data.push(Number.parseInt(dataString.substring(i, i + 2), 16));
          }

          const response = await this.usbDeviceProxy_.controlTransferOut(
              usbControlTransferParams, data, CONTROL_TRANSFER_TIMEOUT_MS);
          checkTransferSuccess(
              response.status, 'Failed to send request.', this.rootElement_);
        }
      } catch (e) {
        showError(e.message, this.rootElement_);
        return;
      } finally {
        await this.usbDeviceProxy_.close();
      }
    }

    /**
     * Renders a view to display response data in hex format.
     * @param {!Uint8Array} rawData
     * @private
     */
    async renderTestingData_(rawData) {
      const displayElement = addNewDescriptorDisplayElement(this.rootElement_);
      /** @type {!cr.ui.Tree} */
      const rawDataTreeRoot = displayElement.rawDataTreeRoot;
      rawDataTreeRoot.style.display = 'none';
      /** @type {!HTMLElement} */
      const rawDataByteElement = displayElement.rawDataByteElement;
      renderRawDataBytes(rawDataByteElement, rawData);
    }

    /**
     * Initializes the testing tool panel for input and query functionality.
     */
    initialTestingToolPanel() {
      showWarn(
          'Warning: This tool can send arbitrary commands to the device. ' +
              'Invalid commands may cause unexpected results.',
          this.rootElement_);
      const inputTableRows =
          this.rootElement_.querySelector('tbody').querySelectorAll('tr');
      const buttons =
          this.rootElement_.querySelector('tbody').querySelectorAll('button');
      const dataInputArea = this.rootElement_.querySelector('textarea');
      dataInputArea.addEventListener('keypress', () => {
        const index = dataInputArea.selectionStart;
        dataInputArea.value = dataInputArea.value.substring(0, index) +
            dataInputArea.value.substring(index + 1);
        dataInputArea.selectionEnd = index;
      });

      const testingToolPanelInputTypeSelector =
          this.rootElement_.querySelector('#input-type');
      testingToolPanelInputTypeSelector.addEventListener('change', () => {
        this.clearView();
        const index = testingToolPanelInputTypeSelector.selectedIndex;
        inputTableRows.forEach(row => row.hidden = true);
        const rowAtIndex = assertInstanceof(inputTableRows[index], HTMLElement);
        rowAtIndex.hidden = false;

        const direction = getRequestTypeDirection(rowAtIndex, index);
        const length = getRequestLength(rowAtIndex, index);
        this.rootElement_.querySelector('#data-input-area').hidden =
            (direction !== 'Host-to-Device');
        dataInputArea.value = '00'.repeat(length);
        dataInputArea.maxLength = length * 2;
      });


      inputTableRows.forEach((el, i) => {
        const inputTableRow = assertInstanceof(el, HTMLElement);
        let directionInputElement;
        switch (i) {
          case INPUT_TYPE_DECIMAL_WITH_DROPDOWN:
            directionInputElement =
                inputTableRow.querySelector('#transfer-direction');
            break;
          case INPUT_TYPE_HEX_BYTE:
            directionInputElement =
                inputTableRow.querySelector('#query-request-type');
            break;
        }
        directionInputElement.addEventListener('change', () => {
          this.rootElement_.querySelector('#data-input-area').hidden =
              (getRequestTypeDirection(inputTableRow, i) !== 'Host-to-Device');
        });

        inputTableRow.querySelector('#query-length')
            .addEventListener('blur', () => {
              const length = getRequestLength(inputTableRow, i);
              dataInputArea.value = '00'.repeat(length);
              dataInputArea.maxLength = length * 2;
            });
      });

      buttons.forEach((button, i) => {
        button.addEventListener('click', () => {
          this.clearView();

          const row = assertInstanceof(inputTableRows[i], HTMLElement);
          const direction = getRequestTypeDirection(row, i);
          const type = getRequestType(row, i);
          const recipient = getRequestTypeRecipient(row, i);
          const request = getRequestCode(row, i);
          const value = getRequestValue(row, i);
          const index = getRequestIndex(row, i);
          const dataLength = getRequestLength(row, i);

          if (this.checkEnumParamValid_(
                  type, 'Transfer Type', device.mojom.UsbControlTransferType) &&
              this.checkEnumParamValid_(
                  recipient, 'Transfer Recipient',
                  device.mojom.UsbControlTransferRecipient) &&
              this.checkParamValid_(request, 'Transfer Request', 0, 255) &&
              this.checkParamValid_(value, 'wValue', 0, 65535) &&
              this.checkParamValid_(index, 'wIndex', 0, 65535) &&
              this.checkParamValid_(dataLength, 'Length', 0, 65535)) {
            /** @type {!device.mojom.UsbControlTransferParams} */
            const usbControlTransferParams = {
              type: device.mojom.UsbControlTransferType[type],
              recipient: device.mojom.UsbControlTransferRecipient[recipient],
              request,
              value,
              index,
            };
            this.sendTestingRequest_(
                usbControlTransferParams, dataLength, direction);
          }
        });
      });
    }

    /**
     * Checks if the user input is a valid number.
     * @param {number} paramValue
     * @param {string} paramName
     * @param {number} min
     * @param {number} max
     * @return {boolean}
     * @private
     */
    checkParamValid_(paramValue, paramName, min, max) {
      if (Number.isNaN(paramValue) || paramValue < min || paramValue > max) {
        showError(`Invalid ${paramName}.`, this.rootElement_);
        return false;
      }
      return true;
    }

    /**
     * Checks if the user input for a enum field is valid.
     * @param {string} enumString
     * @param {string} paramName
     * @param {!Object} enumObject
     * @return {boolean}
     * @private
     */
    checkEnumParamValid_(enumString, paramName, enumObject) {
      if (enumObject[enumString] !== undefined) {
        return true;
      }
      showError(`Invalid ${paramName}`, this.rootElement_);
      return false;
    }
  }

  /**
   * Get the USB control transfer type.
   * @param {!HTMLElement} inputRow
   * @param {number} inputType
   * @return {string}
   */
  function getRequestType(inputRow, inputType) {
    switch (inputType) {
      case INPUT_TYPE_DECIMAL_WITH_DROPDOWN:
        return queryRequiredElement('#transfer-type', inputRow).value;
      case INPUT_TYPE_HEX_BYTE:
        const value = Number.parseInt(
            queryRequiredElement('#query-request-type', inputRow).value, 16);
        switch (value >> 5 & 0x03) {
          case 0:
            return 'STANDARD';
          case 1:
            return 'CLASS';
          case 2:
            return 'VENDOR';
        }
      default:
        return '';
    }
  }

  /**
   * Get the USB control transfer recipient.
   * @param {!HTMLElement} inputRow
   * @param {number} inputType
   * @return {string}
   */
  function getRequestTypeRecipient(inputRow, inputType) {
    switch (inputType) {
      case INPUT_TYPE_DECIMAL_WITH_DROPDOWN:
        return queryRequiredElement('#transfer-recipient', inputRow).value;
      case INPUT_TYPE_HEX_BYTE:
        const value = Number.parseInt(
            queryRequiredElement('#query-request-type', inputRow).value, 16);
        switch (value & 0x1F) {
          case 0:
            return 'DEVICE';
          case 1:
            return 'INTERFACE';
          case 2:
            return 'ENDPOINT';
          case 3:
            return 'OTHER';
        }
      default:
        return '';
    }
  }

  /**
   * Get the USB control transfer direction. 0 for device-to-host, 1 for
   * host-to-device.
   * @param {!HTMLElement} inputRow
   * @param {number} inputType
   * @return {string}
   */
  function getRequestTypeDirection(inputRow, inputType) {
    switch (inputType) {
      case INPUT_TYPE_DECIMAL_WITH_DROPDOWN:
        return queryRequiredElement('#transfer-direction', inputRow).value;
      case INPUT_TYPE_HEX_BYTE:
        const value = Number.parseInt(
            queryRequiredElement('#query-request-type', inputRow).value, 16);
        switch (value >> 7) {
          case CONTROL_TRANSFER_DIRECTION_HOST_TO_DEVICE:
            return 'Host-to-Device';
          case CONTROL_TRANSFER_DIRECTION_DEVICE_TO_HOST:
            return 'Device-to-Host';
        }
      default:
        return 'Device-to-Host';
    }
  }

  /**
   * Get the USB control transfer request code.
   * @param {!HTMLElement} inputRow
   * @param {number} inputType
   * @return {number}
   */
  function getRequestCode(inputRow, inputType) {
    switch (inputType) {
      case INPUT_TYPE_DECIMAL_WITH_DROPDOWN:
        return Number.parseInt(
            queryRequiredElement('#query-request', inputRow).value, 10);
      case INPUT_TYPE_HEX_BYTE:
        return Number.parseInt(
            queryRequiredElement('#query-request', inputRow).value, 16);
      default:
        return Number.NaN;
    }
  }

  /**
   * Get the value of USB control transfer request wValue field.
   * @param {!HTMLElement} inputRow
   * @param {number} inputType
   * @return {number}
   */
  function getRequestValue(inputRow, inputType) {
    switch (inputType) {
      case INPUT_TYPE_DECIMAL_WITH_DROPDOWN:
        return Number.parseInt(
            queryRequiredElement('#query-value', inputRow).value, 10);
      case INPUT_TYPE_HEX_BYTE:
        return Number.parseInt(
            queryRequiredElement('#query-value', inputRow).value, 16);
      default:
        return Number.NaN;
    }
  }

  /**
   * Get the value of USB control transfer request wIndex field.
   * @param {!HTMLElement} inputRow
   * @param {number} inputType
   * @return {number}
   */
  function getRequestIndex(inputRow, inputType) {
    switch (inputType) {
      case INPUT_TYPE_DECIMAL_WITH_DROPDOWN:
        return Number.parseInt(
            queryRequiredElement('#query-index', inputRow).value, 10);
      case INPUT_TYPE_HEX_BYTE:
        return Number.parseInt(
            queryRequiredElement('#query-index', inputRow).value, 16);
      default:
        return Number.NaN;
    }
  }

  /**
   * Get the length of the data transferred during USB control transfer.
   * @param {!HTMLElement} inputRow
   * @param {number} inputType
   * @return {number}
   */
  function getRequestLength(inputRow, inputType) {
    switch (inputType) {
      case INPUT_TYPE_DECIMAL_WITH_DROPDOWN:
        return Number.parseInt(
            queryRequiredElement('#query-length', inputRow).value, 10);
      case INPUT_TYPE_HEX_BYTE:
        return Number.parseInt(
            queryRequiredElement('#query-length', inputRow).value, 16);
      default:
        return Number.NaN;
    }
  }

  /**
   * Adds a display area which contains a tree view and a byte view.
   * @param {!HTMLElement} rootElement
   * @param {string=} descriptorPanelTitle
   * @return {{rawDataTreeRoot:!cr.ui.Tree,rawDataByteElement:!HTMLElement}}
   */
  function addNewDescriptorDisplayElement(
      rootElement, descriptorPanelTitle = undefined) {
    const descriptorPanelTemplate =
        queryRequiredElement('#descriptor-panel-template');

    const descriptorPanelClone = /** @type {!HTMLElement} */
        (document.importNode(descriptorPanelTemplate.content, true));

    /** @type {!HTMLElement} */
    const rawDataTreeRoot =
        queryRequiredElement('.raw-data-tree-view', descriptorPanelClone);
    /** @type {!HTMLElement} */
    const rawDataByteElement =
        queryRequiredElement('.raw-data-byte-view', descriptorPanelClone);

    cr.ui.decorate(rawDataTreeRoot, cr.ui.Tree);
    rawDataTreeRoot.detail = {payload: {}, children: {}};

    if (descriptorPanelTitle) {
      const descriptorPanelTitleTemplate =
          queryRequiredElement('#descriptor-panel-title');
      const clone =
          document.importNode(descriptorPanelTitleTemplate.content, true)
              .querySelector('descriptorpaneltitle');
      clone.textContent = descriptorPanelTitle;
      rootElement.appendChild(clone);
    }
    rootElement.appendChild(descriptorPanelClone);
    return {rawDataTreeRoot, rawDataByteElement};
  }

  /**
   * Shows an error message.
   * @param {string} message
   * @param {!HTMLElement} rootElement
   */
  function showError(message, rootElement) {
    const errorElement = document.createElement('error');
    errorElement.textContent = message;
    rootElement.prepend(errorElement);
  }

  /**
   * Shows a warning message.
   * @param {string} message
   * @param {!HTMLElement} rootElement
   */
  function showWarn(message, rootElement) {
    const warnElement = document.createElement('warn');
    warnElement.textContent = message;
    rootElement.prepend(warnElement);
  }

  /**
   * Renders a customized TreeItem with the given content and class name.
   * @param {string} itemLabel
   * @param {string=} className
   * @return {!cr.ui.TreeItem}
   */
  function customTreeItem(itemLabel, className = undefined) {
    const item = new cr.ui.TreeItem({
      label: itemLabel,
      icon: '',
    });
    if (className) {
      item.rowElement.classList.add(className);
    }
    return item;
  }

  /**
   * Adds function for mapping between two views.
   * @param {!cr.ui.Tree} rawDataTreeRoot
   * @param {!HTMLElement} rawDataByteElement
   */
  function addMappingAction(rawDataTreeRoot, rawDataByteElement) {
    // Highlights the byte(s) that hovered in the tree.
    rawDataTreeRoot.querySelectorAll('.tree-row').forEach((el) => {
      const classList = el.classList;
      // classList[0] is 'tree-row'. classList[1] of tree item for fields
      // starts with 'field-offset-', and classList[1] of tree item for
      // descriptors (ie. endpoint descriptor) is descriptor type and index.
      const fieldOffsetOrDescriptorClass = classList[1];
      assert(
          fieldOffsetOrDescriptorClass.startsWith('field-offset-') ||
          fieldOffsetOrDescriptorClass.startsWith('descriptor-'));

      el.addEventListener('pointerenter', (event) => {
        rawDataByteElement.querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
            .forEach((el) => el.classList.add('hovered-field'));
        event.stopPropagation();
      });

      el.addEventListener('pointerleave', () => {
        rawDataByteElement.querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
            .forEach((el) => el.classList.remove('hovered-field'));
      });

      el.addEventListener('click', (event) => {
        if (event.target.className != 'expand-icon') {
          // Clears all the selected elements before select another.
          rawDataByteElement.querySelectorAll('.raw-data-byte-view span')
              .forEach((el) => el.classList.remove('selected-field'));

          rawDataByteElement
              .querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
              .forEach((el) => el.classList.add('selected-field'));
        }
      });
    });

    // Selects the tree item that displays the byte hovered in the raw view.
    const rawDataByteElements = rawDataByteElement.querySelectorAll('span');
    rawDataByteElements.forEach((el) => {
      const classList = el.classList;
      if (!classList[0]) {
        // For a field that has failed to render there might be some leftover
        // bytes. Just skip them.
        return;
      }
      const fieldOffsetClass = classList[0];
      assert(fieldOffsetClass.startsWith('field-offset-'));

      el.addEventListener('pointerenter', () => {
        rawDataByteElement.querySelectorAll(`.${fieldOffsetClass}`)
            .forEach((el) => el.classList.add('hovered-field'));
        const el =
            queryRequiredElement(`.${fieldOffsetClass}`, rawDataTreeRoot);
        if (el) {
          el.classList.add('hover');
        }
      });

      el.addEventListener('pointerleave', () => {
        rawDataByteElement.querySelectorAll(`.${fieldOffsetClass}`)
            .forEach((el) => el.classList.remove('hovered-field'));
        const el =
            queryRequiredElement(`.${fieldOffsetClass}`, rawDataTreeRoot);
        if (el) {
          el.classList.remove('hover');
        }
      });

      el.addEventListener('click', () => {
        const el =
            queryRequiredElement(`.${fieldOffsetClass}`, rawDataTreeRoot);
        if (el) {
          el.click();
        }
      });
    });
  }

  /**
   * Renders a tree view to display the raw data in readable text.
   * @param {!cr.ui.Tree|!cr.ui.TreeItem} root
   * @param {!HTMLElement} rawDataByteElement
   * @param {!Array<Object>} fields
   * @param {!Uint8Array} rawData
   * @param {number} offset The start offset of the descriptor structure that
   *     want to be rendered.
   * @param {!HTMLElement} rootElement
   * @param {...string} parentClassNames
   * @return {number} The end offset of descriptor structure that want to be
   *     rendered.
   */
  function renderRawDataTree(
      root, rawDataByteElement, fields, rawData, offset, rootElement,
      ...parentClassNames) {
    const rawDataByteElements = rawDataByteElement.querySelectorAll('span');

    for (const field of fields) {
      const className = `field-offset-${offset}`;
      let item;
      try {
        item = customTreeItem(
            `${field.label}${field.formatter(rawData, offset)}`, className);

        for (let i = 0; i < field.size; i++) {
          rawDataByteElements[offset + i].classList.add(className);
          for (const parentClassName of parentClassNames) {
            rawDataByteElements[offset + i].classList.add(parentClassName);
          }
        }
      } catch (e) {
        showError(`Field at offset ${offset} is invalid.`, rootElement);
        break;
      }

      try {
        if (field.extraTreeItemFormatter) {
          field.extraTreeItemFormatter(rawData, offset, item, field.label);
        }
      } catch (e) {
        showError(
            `Error at rendering field at index ${offset}: ${e.message}`,
            rootElement);
      }

      root.add(item);
      offset += field.size;
    }
    return offset;
  }

  /**
   * Renders an element to display the raw data in hex, byte by byte.
   * @param {!HTMLElement} rawDataByteElement
   * @param {!Uint8Array} rawData
   */
  function renderRawDataBytes(rawDataByteElement, rawData) {
    const rawDataByteContainerTemplate =
        queryRequiredElement('#raw-data-byte-container-template');
    const rawDataByteContainerClone =
        document.importNode(rawDataByteContainerTemplate.content, true);
    const rawDataByteContainerElement =
        rawDataByteContainerClone.querySelector('div');

    const rawDataByteTemplate = queryRequiredElement('#raw-data-byte-template');
    for (const value of rawData) {
      const rawDataByteClone =
          document.importNode(rawDataByteTemplate.content, true);
      const rawDataByteElement = rawDataByteClone.querySelector('span');
      rawDataByteElement.textContent = toHex(value, 2);
      rawDataByteContainerElement.appendChild(rawDataByteElement);
    }
    rawDataByteElement.appendChild(rawDataByteContainerElement);
  }

  /**
   * Checks if the status of a control transfer indicates success.
   * @param {number} status
   * @param {string} defaultMessage
   * @param {!HTMLElement} rootElement
   */
  function checkTransferSuccess(status, defaultMessage, rootElement) {
    let failReason = '';
    switch (status) {
      case device.mojom.UsbTransferStatus.COMPLETED:
        return;
      case device.mojom.UsbTransferStatus.SHORT_PACKET:
        showError('Descriptor is too short.', rootElement);
        return;
      case device.mojom.UsbTransferStatus.BABBLE:
        showError('Descriptor is too long.', rootElement);
        return;
      case device.mojom.UsbTransferStatus.TRANSFER_ERROR:
        failReason = 'Transfer Error';
        break;
      case device.mojom.UsbTransferStatus.TIMEOUT:
        failReason = 'Timeout';
        break;
      case device.mojom.UsbTransferStatus.CANCELLED:
        failReason = 'Transfer was cancelled';
        break;
      case device.mojom.UsbTransferStatus.STALLED:
        failReason = 'Transfer Error';
        break;
      case device.mojom.UsbTransferStatus.DISCONNECT:
        failReason = 'Transfer stalled';
        break;
      case device.mojom.UsbTransferStatus.PERMISSION_DENIED:
        failReason = 'Permission denied';
        break;
    }
    // Response data will be null if |status| is neither COMPLETED, BABBLE, or
    // SHORT_PACKET. Throws an error to stop rendering response data.
    throw new Error(`${defaultMessage} (Reason: ${failReason})`);
  }

  /**
   * Converts a number to a hexadecimal string padded with zeros to the given
   * number of digits.
   * @param {number} number
   * @param {number} numOfDigits
   * @return {string}
   */
  function toHex(number, numOfDigits) {
    return number.toString(16).padStart(numOfDigits, '0').toUpperCase();
  }

  /**
   * Parses UTF-16 array to string.
   * @param {!Uint8Array} arr
   * @param {boolean=} isLittleEndian
   * @return {string}
   */
  function decodeUtf16Array(arr, isLittleEndian = false) {
    let str = '';
    const data = new DataView(arr.buffer);
    for (let i = 0; i < arr.length; i += 2) {
      str += String.fromCodePoint(data.getUint16(i, isLittleEndian));
    }
    return str;
  }

  /**
   * Parses UTF-8 array to string.
   * @param {!Uint8Array} arr
   * @return {string}
   */
  function decodeUtf8Array(arr) {
    return String.fromCodePoint(...arr);
  }

  /**
   * Parses one byte to decimal number string.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatByte(rawData, offset) {
    return rawData[offset].toString();
  }

  /**
   * Parses two bytes to decimal number.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {number}
   */
  function parseShort(rawData, offset) {
    const data = new DataView(rawData.buffer);
    return data.getUint16(offset, true);
  }

  /**
   * Parses two bytes to decimal number string.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatShort(rawData, offset) {
    return parseShort(rawData, offset).toString();
  }

  /**
   * Parses two bytes to decimal number string.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatLetter(rawData, offset) {
    const num = parseShort(rawData, offset);
    return String.fromCodePoint(num);
  }

  /**
   * Parses two bytes to a hex string.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatTwoBytesToHex(rawData, offset) {
    const num = parseShort(rawData, offset);
    return `0x${toHex(num, 4)}`;
  }

  /**
   * Parses two bytes to USB version format.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatUsbVersion(rawData, offset) {
    return `${rawData[offset + 1]}.${rawData[offset] >> 4}.${
        rawData[offset] & 0x0F}`;
  }

  /**
   * Parses one byte to a bitmap.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatBitmap(rawData, offset) {
    return rawData[offset].toString(2).padStart(8, '0');
  }

  /**
   * Parses descriptor type to a hex string.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatDescriptorType(rawData, offset) {
    return `0x${toHex(rawData[offset], 2)}`;
  }

  /**
   * Parses UUID field.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatUuid(rawData, offset) {
    // UUID is 16 bytes (Section 9.6.2.4 of Universal Serial Bus 3.1
    // Specification).
    // Additional reference: IETF RFC 4122. https://tools.ietf.org/html/rfc4122
    let uuidStr = '';
    const data = new DataView(rawData.buffer);

    uuidStr += toHex(data.getUint32(offset, true), 8);
    uuidStr += '-';
    uuidStr += toHex(data.getUint16(offset + 4, true), 4);
    uuidStr += '-';
    uuidStr += toHex(data.getUint16(offset + 6, true), 4);
    uuidStr += '-';
    uuidStr += toHex(data.getUint8(offset + 8), 2);
    uuidStr += toHex(data.getUint8(offset + 9), 2);
    uuidStr += '-';
    uuidStr += toHex(data.getUint8(offset + 10), 2);
    uuidStr += toHex(data.getUint8(offset + 11), 2);
    uuidStr += toHex(data.getUint8(offset + 12), 2);
    uuidStr += toHex(data.getUint8(offset + 13), 2);
    uuidStr += toHex(data.getUint8(offset + 14), 2);
    uuidStr += toHex(data.getUint8(offset + 15), 2);

    return uuidStr;
  }

  /**
   * Parses Compatible ID String field.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatCompatibleIdString(rawData, offset) {
    // Compatible ID String is 8 bytes, which is defined by Microsoft OS 2.0
    // Descriptors Specification (July, 2018).
    return decodeUtf8Array(rawData.slice(offset, offset + 8));
  }

  /**
   * Parses Windows Version field.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatWindowsVersion(rawData, offset) {
    const data = new DataView(rawData.buffer);
    const windowsVersion = data.getUint32(offset, true);
    switch (windowsVersion) {
      case WIN_81_HEADER:
        return 'Windows 8.1';
      default:
        return `0x${toHex(windowsVersion, 8)}`;
    }
  }

  /**
   * Parses Feature Registry Property Data Type.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatFeatureRegistryPropertyDataType(rawData, offset) {
    const data = new DataView(rawData.buffer);
    const propertyDataType = data.getUint16(offset, true);
    switch (propertyDataType) {
      case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_SZ:
        return 'A NULL-terminated Unicode String (REG_SZ)';
      case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_EXPAND_SZ:
        return 'A NULL-terminated Unicode String that includes environment ' +
            'variables (REG_EXPAND_SZ)';
      case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_BINARY:
        return 'Free-form binary (REG_BINARY)';
      case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_DWORD_LITTLE_ENDIAN:
        return 'A little-endian 32-bit integer (REG_DWORD_LITTLE_ENDIAN)';
      case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_DWORD_BIG_ENDIAN:
        return 'A big-endian 32-bit integer (REG_DWORD_BIG_ENDIAN)';
      case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_LINK:
        return 'A NULL-terminated Unicode string that contains a symbolic ' +
            'link (REG_LINK)';
      case MS_OS_20_FEATURE_REG_PROPERTY_DATA_TYPE_REG_MULTI_SZ:
        return 'Multiple NULL-terminated Unicode strings (REG_MULTI_SZ)';
      default:
        return 'Reserved';
    }
  }

  /**
   * Returns empty string for a field that can't or doesn't need to be parsed.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @return {string}
   */
  function formatUnknown(rawData, offset) {
    return '';
  }

  /**
   * Parses language code to readable language name.
   * @param {number} languageCode
   * @return {string}
   */
  function parseLanguageCode(languageCode) {
    switch (languageCode) {
      case LANGUAGE_CODE_EN_US:
        return 'en-US';
      default:
        return `0x${toHex(languageCode, 4)}`;
    }
  }

  /**
   * Checks if two UUIDs are same.
   * @param {!Uint8Array} rawData
   * @param {number} offset The offset of current field.
   * @param {!Array<number>} uuidArr
   */
  function isSameUuid(rawData, offset, uuidArr) {
    // Validate the Platform Capability Descriptor
    if (offset + 20 > rawData.length) {
      return false;
    }
    // UUID is from index 4 to index 19 (Section 9.6.2.4 of Universal Serial
    // Bus 3.1 Specification).
    for (const [i, num] of rawData.slice(offset + 4, offset + 20).entries()) {
      if (num !== uuidArr[i]) {
        return false;
      }
    }
    return true;
  }

  return {
    DescriptorPanel,
  };
});

window.deviceDescriptorCompleteFn =
    window.deviceDescriptorCompleteFn || function() {};
