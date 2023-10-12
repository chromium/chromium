// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DescriptorList and DescriptorListItem, served from
 *     chrome://bluetooth-internals/.
 */

import './descriptor_list_item.js';
import './expandable_list.js';

import {assert} from 'chrome://resources/js/assert.js';

import {connectToDevice} from './device_broker.js';
import {ExpandableListElement} from './expandable_list.js';
import {showSnackbar, SnackbarType} from './snackbar.js';

/**
 * A list that displays DescriptorListItems.
 */
export class DescriptorListElement extends ExpandableListElement {
  constructor() {
    super();

    /** @private {?string} */
    this.deviceAddress_ = null;
    /** @private {?string} */
    this.serviceId_ = null;
    /** @private {?string} */
    this.characteristicId_ = null;
    /** @private {boolean} */
    this.descriptorsRequested_ = false;
  }

  connectedCallback() {
    super.connectedCallback();
    this.classList.add('descriptor-list');
  }

  createItem(data) {
    const item = document.createElement('descriptor-list-item');
    assert(this.deviceAddress_);
    assert(this.serviceId_);
    assert(this.characteristicId_);
    item.initialize(
        data, this.deviceAddress_, this.serviceId_, this.characteristicId_);
    return item;
  }

  /**
   * Loads the descriptor list with an array of DescriptorInfo from
   * the device with |deviceAddress|, service with |serviceId|, and
   * characteristic with |characteristicId|. If no active connection to the
   * device exists, one is created.
   * @param {string} deviceAddress
   * @param {string} serviceId
   * @param {string} characteristicId
   */
  load(deviceAddress, serviceId, characteristicId) {
    this.setEmptyMessage('No Descriptors Found');
    if (this.descriptorsRequested_ || !this.isSpinnerShowing()) {
      return;
    }

    this.deviceAddress_ = deviceAddress;
    this.serviceId_ = serviceId;
    this.characteristicId_ = characteristicId;
    this.descriptorsRequested_ = true;

    connectToDevice(deviceAddress)
        .then(function(device) {
          return device.getDescriptors(serviceId, characteristicId);
        }.bind(this))
        .then(function(response) {
          this.setData(response.descriptors || []);
          this.setSpinnerShowing(false);
          this.descriptorsRequested_ = false;
        }.bind(this))
        .catch(function(error) {
          this.descriptorsRequested_ = false;
          showSnackbar(
              deviceAddress + ': ' + error.message, SnackbarType.ERROR, 'Retry',
              function() {
                this.load(deviceAddress, serviceId, characteristicId);
              }.bind(this));
        }.bind(this));
  }
}

customElements.define('descriptor-list', DescriptorListElement);
