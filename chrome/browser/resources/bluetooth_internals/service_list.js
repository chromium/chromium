// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for ServiceList and ServiceListItem, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('service_list', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const ExpandableList = expandable_list.ExpandableList;
  const ExpandableListItem = expandable_list.ExpandableListItem;
  const Snackbar = snackbar.Snackbar;
  const SnackbarType = snackbar.SnackbarType;

  /**
   * Property names that will be displayed in the ObjectFieldSet which contains
   * the ServiceInfo object.
   */
  const PROPERTY_NAMES = {
    id: 'ID',
    'uuid.uuid': 'UUID',
    isPrimary: 'Type',
  };

  /**
   * A list item that displays the data in a ServiceInfo object. The brief
   * section contains the UUID of the given |serviceInfo|. The expanded section
   * contains an ObjectFieldSet that displays all of the properties in the
   * given |serviceInfo|. Data is not loaded until the ServiceListItem is
   * expanded for the first time.
   * @param {!bluetooth.mojom.ServiceInfo} serviceInfo
   * @param {string} deviceAddress
   * @extends {expandable_list.ExpandableListItem}
   * @constructor
   */
  function ServiceListItem(serviceInfo, deviceAddress) {
    const listItem = new ExpandableListItem();
    listItem.__proto__ = ServiceListItem.prototype;

    /** @type {!bluetooth.mojom.ServiceInfo} */
    listItem.info = serviceInfo;
    /** @private {string} */
    listItem.deviceAddress_ = deviceAddress;

    listItem.decorate();
    return listItem;
  }

  ServiceListItem.prototype = {
    __proto__: ExpandableListItem.prototype,

    /**
     * Decorates the element as a service list item. Creates layout and caches
     * references to the created header and fieldset.
     * @override
     */
    decorate: function() {
      this.classList.add('service-list-item');

      /** @private {!object_fieldset.ObjectFieldSet} */
      this.serviceFieldSet_ = new object_fieldset.ObjectFieldSet();
      this.serviceFieldSet_.setPropertyDisplayNames(PROPERTY_NAMES);
      this.serviceFieldSet_.setObject({
        id: this.info.id,
        'uuid.uuid': this.info.uuid.uuid,
        isPrimary: this.info.isPrimary ? 'Primary' : 'Secondary',
      });

      // Create content for display in brief content container.
      const serviceHeaderText = document.createElement('div');
      serviceHeaderText.textContent = 'Service:';

      const serviceHeaderValue = document.createElement('div');
      serviceHeaderValue.textContent = this.info.uuid.uuid;

      const serviceHeader = document.createElement('div');
      serviceHeader.appendChild(serviceHeaderText);
      serviceHeader.appendChild(serviceHeaderValue);
      this.briefContent_.appendChild(serviceHeader);

      // Create content for display in expanded content container.
      const serviceInfoHeader = document.createElement('h4');
      serviceInfoHeader.textContent = 'Service Info';

      const serviceDiv = document.createElement('div');
      serviceDiv.classList.add('flex');
      serviceDiv.appendChild(this.serviceFieldSet_);

      const characteristicsListHeader = document.createElement('h4');
      characteristicsListHeader.textContent = 'Characteristics';
      this.characteristicList_ = new characteristic_list.CharacteristicList();

      const infoDiv = document.createElement('div');
      infoDiv.classList.add('info-container');
      infoDiv.appendChild(serviceInfoHeader);
      infoDiv.appendChild(serviceDiv);
      infoDiv.appendChild(characteristicsListHeader);
      infoDiv.appendChild(this.characteristicList_);

      this.expandedContent_.appendChild(infoDiv);
    },

    /** @override */
    onExpandInternal: function(expanded) {
      this.characteristicList_.load(this.deviceAddress_, this.info.id);
    },
  };

  /**
   * A list that displays ServiceListItems.
   * @constructor
   * @extends {expandable_list.ExpandableList}
   */
  const ServiceList = cr.ui.define('list');

  ServiceList.prototype = {
    __proto__: ExpandableList.prototype,

    /** @override */
    decorate: function() {
      ExpandableList.prototype.decorate.call(this);

      /** @private {?string} */
      this.deviceAddress_ = null;
      /** @private {boolean} */
      this.servicesRequested_ = false;

      this.classList.add('service-list');
      this.setEmptyMessage('No Services Found');
    },

    /** @override */
    createItem: function(data) {
      return new ServiceListItem(data, assert(this.deviceAddress_));
    },

    /**
     * Loads the service list with an array of ServiceInfo from the
     * device with |deviceAddress|. If no active connection to the device
     * exists, one is created.
     * @param {string} deviceAddress
     */
    load: function(deviceAddress) {
      if (this.servicesRequested_ || !this.isSpinnerShowing()) {
        return;
      }

      this.deviceAddress_ = deviceAddress;
      this.servicesRequested_ = true;

      device_broker.connectToDevice(this.deviceAddress_)
          .then(function(device) {
            return device.getServices();
          }.bind(this))
          .then(function(response) {
            this.setData(new ArrayDataModel(response.services));
            this.setSpinnerShowing(false);
            this.servicesRequested_ = false;
          }.bind(this))
          .catch(function(error) {
            this.servicesRequested_ = false;
            Snackbar.show(
                deviceAddress + ': ' + error.message, SnackbarType.ERROR,
                'Retry', function() {
                  this.load(deviceAddress);
                }.bind(this));
          }.bind(this));
    },
  };

  return {
    ServiceList: ServiceList,
    ServiceListItem: ServiceListItem,
  };
});
