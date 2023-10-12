// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './characteristic_list_item.js';
import './expandable_list.js';

import {assert} from 'chrome://resources/js/assert.js';

import {connectToDevice} from './device_broker.js';
import {ExpandableListElement} from './expandable_list.js';
import {showSnackbar, SnackbarType} from './snackbar.js';

export class CharacteristicListElement extends ExpandableListElement {
  constructor() {
    super();

    /** @private {?string} */
    this.deviceAddress_ = null;
    /** @private {?string} */
    this.serviceId_ = null;
    /** @private {boolean} */
    this.characteristicsRequested_ = false;
  }

  connectedCallback() {
    super.connectedCallback();
    this.classList.add('characteristic-list');
  }

  createItem(data) {
    const item = document.createElement('characteristic-list-item');
    assert(this.deviceAddress_);
    assert(this.serviceId_);
    item.initialize(data, this.deviceAddress_, this.serviceId_);
    return item;
  }

  /**
   * Loads the characteristic list with an array of CharacteristicInfo from
   * the device with |deviceAddress| and service with |serviceId|. If no
   * active connection to the device exists, one is created.
   * @param {string} deviceAddress
   * @param {string} serviceId
   */
  load(deviceAddress, serviceId) {
    this.setEmptyMessage('No Characteristics Found');
    if (this.characteristicsRequested_ || !this.isSpinnerShowing()) {
      return;
    }

    this.deviceAddress_ = deviceAddress;
    this.serviceId_ = serviceId;
    this.characteristicsRequested_ = true;

    connectToDevice(deviceAddress)
        .then(function(device) {
          return device.getCharacteristics(serviceId);
        }.bind(this))
        .then(function(response) {
          this.setData(response.characteristics || []);
          this.setSpinnerShowing(false);
          this.characteristicsRequested_ = false;
        }.bind(this))
        .catch(function(error) {
          this.characteristicsRequested_ = false;
          showSnackbar(
              deviceAddress + ': ' + error.message, SnackbarType.ERROR, 'Retry',
              function() {
                this.load(deviceAddress, serviceId);
              }.bind(this));
        }.bind(this));
  }
}

customElements.define('characteristic-list', CharacteristicListElement);
