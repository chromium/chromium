// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {define as crUiDefine} from 'chrome://resources/js/cr/ui.m.js';
import {ArrayDataModel} from 'chrome://resources/js/cr/ui/array_data_model.m.js';

import {CharacteristicList} from './characteristic_list.js';
import {connectToDevice} from './device_broker.js';
import {ExpandableList, ExpandableListItem} from './expandable_list.js';
import {ObjectFieldSet} from './object_fieldset.js';
import {Snackbar, SnackbarType} from './snackbar.js';

/**
 * Javascript for ServiceList and ServiceListItem, served from
 *     chrome://bluetooth-internals/.
 */

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
 * @extends {ExpandableListItem}
 * @constructor
 */
export function ServiceListItem(serviceInfo, deviceAddress) {
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
  decorate() {
    this.classList.add('service-list-item');

    /** @private {!ObjectFieldSet} */
    this.serviceFieldSet_ = new ObjectFieldSet();
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
    this.characteristicList_ = new CharacteristicList();

    const infoDiv = document.createElement('div');
    infoDiv.classList.add('info-container');
    infoDiv.appendChild(serviceInfoHeader);
    infoDiv.appendChild(serviceDiv);
    infoDiv.appendChild(characteristicsListHeader);
    infoDiv.appendChild(this.characteristicList_);

    this.expandedContent_.appendChild(infoDiv);
  },

  /** @override */
  onExpandInternal(expanded) {
    this.characteristicList_.load(this.deviceAddress_, this.info.id);
  },
};

/**
 * A list that displays ServiceListItems.
 * @constructor
 * @extends {ExpandableList}
 */
export const ServiceList = crUiDefine('list');

ServiceList.prototype = {
  __proto__: ExpandableList.prototype,

  /** @override */
  decorate() {
    ExpandableList.prototype.decorate.call(this);

    /** @private {?string} */
    this.deviceAddress_ = null;
    /** @private {boolean} */
    this.servicesRequested_ = false;

    this.classList.add('service-list');
    this.setEmptyMessage('No Services Found');
  },

  /** @override */
  createItem(data) {
    return new ServiceListItem(data, assert(this.deviceAddress_));
  },

  /**
   * Loads the service list with an array of ServiceInfo from the
   * device with |deviceAddress|. If no active connection to the device
   * exists, one is created.
   * @param {string} deviceAddress
   */
  load(deviceAddress) {
    if (this.servicesRequested_ || !this.isSpinnerShowing()) {
      return;
    }

    this.deviceAddress_ = deviceAddress;
    this.servicesRequested_ = true;

    connectToDevice(this.deviceAddress_)
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
              deviceAddress + ': ' + error.message, SnackbarType.ERROR, 'Retry',
              function() {
                this.load(deviceAddress);
              }.bind(this));
        }.bind(this));
  },
};
