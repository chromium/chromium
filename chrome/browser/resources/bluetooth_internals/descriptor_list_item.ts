// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './expandable_list_item.js';
import './object_fieldset.js';
import './value_control.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './descriptor_list_item.html.js';
import type {DescriptorInfo} from './device.mojom-webui.js';

/** Property names for the DescriptorInfo fieldset */
const INFO_PROPERTY_NAMES = {
  id: 'ID',
  'uuid.uuid': 'UUID',
};

export class DescriptorListItemElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private info: DescriptorInfo|null = null;
  private deviceAddress_: string = '';
  private serviceId_: string = '';
  private characteristicId_: string = '';

  connectedCallback() {
    this.classList.add('descriptor-list-item');
  }

  initialize(
      descriptorInfo: DescriptorInfo, deviceAddress: string, serviceId: string,
      characteristicId: string) {
    this.info = descriptorInfo;
    this.deviceAddress_ = deviceAddress;
    this.serviceId_ = serviceId;
    this.characteristicId_ = characteristicId;

    const fieldSet =
        this.shadowRoot!.querySelector<HTMLElement>('object-fieldset');
    assert(fieldSet);
    fieldSet.dataset['nameMap'] = JSON.stringify(INFO_PROPERTY_NAMES);
    fieldSet.dataset['value'] = JSON.stringify({
      id: this.info.id,
      'uuid.uuid': this.info.uuid.uuid,
    });
    fieldSet.hidden = false;

    const valueControl =
        this.shadowRoot!.querySelector<HTMLElement>('value-control');
    assert(valueControl);
    valueControl.dataset['options'] = JSON.stringify({
      deviceAddress: this.deviceAddress_,
      serviceId: this.serviceId_,
      characteristicId: this.characteristicId_,
      descriptorId: this.info.id,
    });
    valueControl.hidden = false;

    const descriptorHeaderValue =
        this.shadowRoot!.querySelector('.header-value');
    assert(descriptorHeaderValue);
    descriptorHeaderValue.textContent = this.info.uuid.uuid;
  }
}

customElements.define('descriptor-list-item', DescriptorListItemElement);

declare global {
  interface HTMLElementTagNameMap {
    'descriptor-list-item': DescriptorListItemElement;
  }
}
