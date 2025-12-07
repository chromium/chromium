// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './characteristic_list.js';
import './expandable_list_item.js';
import './object_fieldset.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import type {CharacteristicListElement} from './characteristic_list.js';
import type {ServiceInfo} from './device.mojom-webui.js';
import type {ExpandableListItemElement} from './expandable_list_item.js';
import type {ObjectFieldsetElement} from './object_fieldset.js';
import {getTemplate} from './service_list_item.html.js';

/**
 * Property names that will be displayed in the ObjectFieldsetElement which
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
 * contains an ObjectFieldsetElement that displays all of the properties in the
 * given |serviceInfo|. Data is not loaded until the ServiceListItem is
 * expanded for the first time.
 */
export class ServiceListItemElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  info: ServiceInfo|null = null;
  private deviceAddress_: string = '';

  connectedCallback() {
    this.classList.add('service-list-item');
    const expandableItem =
        this.shadowRoot!.querySelector<ExpandableListItemElement>(
            'expandable-list-item');
    assert(expandableItem);
    expandableItem.addEventListener('list-item-expanded', () => {
      const list = this.shadowRoot!.querySelector<CharacteristicListElement>(
          'characteristic-list');
      assert(list);
      assert(this.info);
      list.load(this.deviceAddress_, this.info.id);
    });
  }

  initialize(serviceInfo: ServiceInfo, deviceAddress: string) {
    this.info = serviceInfo;
    this.deviceAddress_ = deviceAddress;

    const headerValue = this.shadowRoot!.querySelector('.header-value');
    assert(headerValue);
    headerValue.textContent = this.info.uuid.uuid;

    const serviceFieldSet =
        this.shadowRoot!.querySelector<ObjectFieldsetElement>(
            'object-fieldset');
    assert(serviceFieldSet);
    serviceFieldSet.dataset['nameMap'] = JSON.stringify(PROPERTY_NAMES);
    serviceFieldSet.dataset['value'] = JSON.stringify({
      id: this.info.id,
      'uuid.uuid': this.info.uuid.uuid,
      isPrimary: this.info.isPrimary ? 'Primary' : 'Secondary',
    });
    serviceFieldSet.hidden = false;
  }
}

customElements.define('service-list-item', ServiceListItemElement);

declare global {
  interface HTMLElementTagNameMap {
    'service-list-item': ServiceListItemElement;
  }
}
