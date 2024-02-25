// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './service_list_item.js';
import './expandable_list.js';

import {assert} from 'chrome://resources/js/assert.js';

import {connectToDevice} from './device_broker.js';
import {ExpandableListElement} from './expandable_list.js';
import {showSnackbar, SnackbarType} from './snackbar.js';

/**
 * A list that displays ServiceListItems.
 */
export class ServiceListElement extends ExpandableListElement {
  constructor() {
    super();

    /** @private {?string} */
    this.deviceAddress_ = null;
    /** @private {boolean} */
    this.servicesRequested_ = false;
  }

  connectedCallback() {
    super.connectedCallback();

    this.classList.add('service-list');
  }

  /**
   * Sets the empty message text.
   * @param {string} message
   */
  setEmptyMessage(message) {
    const emptyMessage = this.shadowRoot.querySelector('.empty-message');
    emptyMessage.textContent = message;
  }

  /** @override */
  createItem(data) {
    const item = document.createElement('service-list-item');
    assert(this.deviceAddress_);
    item.initialize(data, this.deviceAddress_);
    return item;
  }

  /**
   * Loads the service list with an array of ServiceInfo from the
   * device with |deviceAddress|. If no active connection to the device
   * exists, one is created.
   * @param {string} deviceAddress
   */
  load(deviceAddress) {
    this.setEmptyMessage('No Services Found');

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
          this.setData(response.services || []);
          this.setSpinnerShowing(false);
          this.servicesRequested_ = false;
        }.bind(this))
        .catch(function(error) {
          this.servicesRequested_ = false;
          showSnackbar(
              deviceAddress + ': ' + error.message, SnackbarType.ERROR, 'Retry',
              function() {
                this.load(deviceAddress);
              }.bind(this));
        }.bind(this));
  }
}

customElements.define('service-list', ServiceListElement);
