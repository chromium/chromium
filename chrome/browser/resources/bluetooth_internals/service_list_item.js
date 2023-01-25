// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './characteristic_list.js';
import './expandable_list_item.js';
import './object_fieldset.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './service_list_item.html.js';

/**
 * Property names that will be displayed in the ObjectFieldSetElement which
 * contains the ServiceInfo object.
 */
const PROPERTY_NAMES = {
  id: 'ID',
  'uuid.uuid': 'UUID',
  isPrimary: 'Type',
};

/**
 * A list item that displays the data in a ServiceInfo object. The brief
 * section contains the UUID of the given |serviceInfo|. The expanded section
 * contains an ObjectFieldSetElement that displays all of the properties in the
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

    const serviceFieldSet = this.shadowRoot.querySelector('object-field-set');
    serviceFieldSet.dataset.nameMap = JSON.stringify(PROPERTY_NAMES);
    serviceFieldSet.dataset.value = JSON.stringify({
      id: this.info.id,
      'uuid.uuid': this.info.uuid.uuid,
      isPrimary: this.info.isPrimary ? 'Primary' : 'Secondary',
    });
    serviceFieldSet.hidden = false;
  }
}

customElements.define('service-list-item', ServiceListItemElement);
