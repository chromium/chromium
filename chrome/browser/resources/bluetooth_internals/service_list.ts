// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './service_list_item.js';
import './expandable_list.js';

import {assert} from 'chrome://resources/js/assert.js';

import type {ServiceInfo} from './device.mojom-webui.js';
import {connectToDevice} from './device_broker.js';
import {ExpandableListElement} from './expandable_list.js';
import type {ServiceListItemElement} from './service_list_item.js';
import {showSnackbar, SnackbarType} from './snackbar.js';

/**
 * A list that displays ServiceListItems.
 */
export class ServiceListElement extends ExpandableListElement<ServiceInfo> {
  private deviceAddress_: string|null = null;
  private servicesRequested_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.classList.add('service-list');
  }

  /**
   * Sets the empty message text.
   */
  override setEmptyMessage(message: string) {
    const emptyMessage = this.shadowRoot!.querySelector('.empty-message')!;
    emptyMessage.textContent = message;
  }

  override createItem(data: ServiceInfo): ServiceListItemElement {
    const item = document.createElement('service-list-item');
    assert(this.deviceAddress_);
    item.initialize(data, this.deviceAddress_);
    return item;
  }

  /**
   * Loads the service list with an array of ServiceInfo from the
   * device with |deviceAddress|. If no active connection to the device
   * exists, one is created.
   */
  load(deviceAddress: string) {
    this.setEmptyMessage('No Services Found');

    if (this.servicesRequested_ || !this.isSpinnerShowing()) {
      return;
    }

    this.deviceAddress_ = deviceAddress;
    this.servicesRequested_ = true;

    connectToDevice(this.deviceAddress_)
        .then((device) => {
          return device.getServices();
        })
        .then((response) => {
          this.setData(response.services || []);
          this.setSpinnerShowing(false);
          this.servicesRequested_ = false;
        })
        .catch((error) => {
          this.servicesRequested_ = false;
          showSnackbar(
              deviceAddress + ': ' + error.message, SnackbarType.ERROR, 'Retry',
              () => {
                this.load(deviceAddress);
              });
        });
  }
}

customElements.define('service-list', ServiceListElement);

declare global {
  interface HTMLElementTagNameMap {
    'service-list': ServiceListElement;
  }
}
