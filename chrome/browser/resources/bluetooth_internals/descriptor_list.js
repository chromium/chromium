// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DescriptorList and DescriptorListItem, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('descriptor_list', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const ExpandableList = expandable_list.ExpandableList;
  const ExpandableListItem = expandable_list.ExpandableListItem;
  const Snackbar = snackbar.Snackbar;
  const SnackbarType = snackbar.SnackbarType;

  /** Property names for the DescriptorInfo fieldset */
  const INFO_PROPERTY_NAMES = {
    id: 'ID',
    'uuid.uuid': 'UUID',
  };

  /**
   * A list item that displays the properties of a DescriptorInfo object.
   * A fieldset is created within the element for the primitive
   * properties, 'id' and 'uuid' within the DescriptorInfo object.
   * @constructor
   * @param {!bluetooth.mojom.DescriptorInfo} descriptorInfo
   * @param {string} deviceAddress
   * @param {string} serviceId
   * @param {string} characteristicId
   * @extends {expandable_list.ExpandableListItem}
   */
  function DescriptorListItem(
      descriptorInfo, deviceAddress, serviceId, characteristicId) {
    const listItem = new ExpandableListItem();
    listItem.__proto__ = DescriptorListItem.prototype;

    /** @type {!bluetooth.mojom.DescriptorInfo} */
    listItem.info = descriptorInfo;
    /** @private {string} */
    listItem.deviceAddress_ = deviceAddress;
    /** @private {string} */
    listItem.serviceId_ = serviceId;
    /** @private {string} */
    listItem.characteristicId_ = characteristicId;

    listItem.decorate();
    return listItem;
  }

  DescriptorListItem.prototype = {
    __proto__: ExpandableListItem.prototype,

    /**
     * Decorates the element as a descriptor list item. Creates and caches
     * a fieldset for displaying property values.
     * @override
     */
    decorate: function() {
      this.classList.add('descriptor-list-item');

      /** @private {!object_fieldset.ObjectFieldSet} */
      this.descriptorFieldSet_ = new object_fieldset.ObjectFieldSet();
      this.descriptorFieldSet_.setPropertyDisplayNames(INFO_PROPERTY_NAMES);
      this.descriptorFieldSet_.setObject({
        id: this.info.id,
        'uuid.uuid': this.info.uuid.uuid,
      });

      /** @private {!value_control.ValueControl} */
      this.valueControl_ = new value_control.ValueControl();
      this.valueControl_.load({
        deviceAddress: this.deviceAddress_,
        serviceId: this.serviceId_,
        characteristicId: this.characteristicId_,
        descriptorId: this.info.id,
      });

      // Create content for display in brief content container.
      const descriptorHeaderText = document.createElement('div');
      descriptorHeaderText.textContent = 'Descriptor:';

      const descriptorHeaderValue = document.createElement('div');
      descriptorHeaderValue.textContent = this.info.uuid.uuid;

      const descriptorHeader = document.createElement('div');
      descriptorHeader.appendChild(descriptorHeaderText);
      descriptorHeader.appendChild(descriptorHeaderValue);
      this.briefContent_.appendChild(descriptorHeader);

      // Create content for display in expanded content container.
      const descriptorInfoHeader = document.createElement('h4');
      descriptorInfoHeader.textContent = 'Descriptor Info';

      const descriptorDiv = document.createElement('div');
      descriptorDiv.classList.add('flex');
      descriptorDiv.appendChild(this.descriptorFieldSet_);

      const valueHeader = document.createElement('h4');
      valueHeader.textContent = 'Value';

      const infoDiv = document.createElement('div');
      infoDiv.classList.add('info-container');
      infoDiv.appendChild(descriptorInfoHeader);
      infoDiv.appendChild(descriptorDiv);
      infoDiv.appendChild(valueHeader);
      infoDiv.appendChild(this.valueControl_);

      this.expandedContent_.appendChild(infoDiv);
    },
  };

  /**
   * A list that displays DescriptorListItems.
   * @constructor
   * @extends {expandable_list.ExpandableList}
   */
  const DescriptorList = cr.ui.define('list');

  DescriptorList.prototype = {
    __proto__: ExpandableList.prototype,

    /** @override */
    decorate: function() {
      ExpandableList.prototype.decorate.call(this);

      /** @private {?string} */
      this.deviceAddress_ = null;
      /** @private {?string} */
      this.serviceId_ = null;
      /** @private {?string} */
      this.characteristicId_ = null;
      /** @private {boolean} */
      this.descriptorsRequested_ = false;

      this.classList.add('descriptor-list');
      this.setEmptyMessage('No Descriptors Found');
    },

    createItem: function(data) {
      return new DescriptorListItem(
          data, assert(this.deviceAddress_), assert(this.serviceId_),
          assert(this.characteristicId_));
    },

    /**
     * Loads the descriptor list with an array of DescriptorInfo from
     * the device with |deviceAddress|, service with |serviceId|, and
     * characteristic with |characteristicId|. If no active connection to the
     * device exists, one is created.
     * @param {string} deviceAddress
     * @param {string} serviceId
     * @param {string} characteristicId
     */
    load: function(deviceAddress, serviceId, characteristicId) {
      if (this.descriptorsRequested_ || !this.isSpinnerShowing()) {
        return;
      }

      this.deviceAddress_ = deviceAddress;
      this.serviceId_ = serviceId;
      this.characteristicId_ = characteristicId;
      this.descriptorsRequested_ = true;

      device_broker.connectToDevice(deviceAddress)
          .then(function(device) {
            return device.getDescriptors(serviceId, characteristicId);
          }.bind(this))
          .then(function(response) {
            this.setData(new ArrayDataModel(response.descriptors || []));
            this.setSpinnerShowing(false);
            this.descriptorsRequested_ = false;
          }.bind(this))
          .catch(function(error) {
            this.descriptorsRequested_ = false;
            Snackbar.show(
                deviceAddress + ': ' + error.message, SnackbarType.ERROR,
                'Retry', function() {
                  this.load(deviceAddress, serviceId, characteristicId);
                }.bind(this));
          }.bind(this));
    },
  };

  return {
    DescriptorList: DescriptorList,
    DescriptorListItem: DescriptorListItem,
  };
});
