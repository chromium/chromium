// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './characteristic_list.js';
import './expandable_list_item.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {ServiceInfo} from './device.mojom-webui.js';
import {ObjectFieldSet} from './object_fieldset.js';
import {getTemplate} from './service_list_item.html.js';

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
 */
export class ServiceListItemElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  constructor() {
    super();
    /** @type {?ServiceInfo} */
    this.info = null;
    /** @private {string} */
    this.deviceAddress_ = '';
    /** @private {!ObjectFieldSet} */
    this.serviceFieldSet_ = new ObjectFieldSet();
  }

  connectedCallback() {
    this.classList.add('service-list-item');
    this.shadowRoot.querySelector('expandable-list-item')
        .addEventListener('list-item-expanded', () => {
          const list = this.shadowRoot.querySelector('characteristic-list');
          list.load(this.deviceAddress_, this.info.id);
        });
  }

  /**
   * @param {!ServiceInfo} serviceInfo
   * @param {string} deviceAddress
   */
  initialize(serviceInfo, deviceAddress) {
    this.info = serviceInfo;
    this.deviceAddress_ = deviceAddress;

    this.shadowRoot.querySelector('.header-value').textContent =
        this.info.uuid.uuid;

    this.serviceFieldSet_.setPropertyDisplayNames(PROPERTY_NAMES);
    this.serviceFieldSet_.setObject({
      id: this.info.id,
      'uuid.uuid': this.info.uuid.uuid,
      isPrimary: this.info.isPrimary ? 'Primary' : 'Secondary',
    });

    const serviceDiv = this.shadowRoot.querySelector('.flex');
    serviceDiv.appendChild(this.serviceFieldSet_);
  }
}

customElements.define('service-list-item', ServiceListItemElement);
