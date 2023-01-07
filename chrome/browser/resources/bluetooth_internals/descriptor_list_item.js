// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './expandable_list_item.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './descriptor_list_item.html.js';
import {DescriptorInfo} from './device.mojom-webui.js';
import {ObjectFieldSet} from './object_fieldset.js';
import {ValueControl} from './value_control.js';

/** Property names for the DescriptorInfo fieldset */
const INFO_PROPERTY_NAMES = {
  id: 'ID',
  'uuid.uuid': 'UUID',
};

export class DescriptorListItemElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  constructor() {
    super();

    /** @private {?DescriptorInfo} */
    this.info = null;
    /** @private {string} */
    this.deviceAddress_ = '';
    /** @private {string} */
    this.serviceId_ = '';
    /** @private {string} */
    this.characteristicId_ = '';

    /** @private {!ObjectFieldSet} */
    this.descriptorFieldSet_ = new ObjectFieldSet();

    /** @private {!ValueControl} */
    this.valueControl_ = new ValueControl();
  }

  connectedCallback() {
    this.classList.add('descriptor-list-item');
  }

  initialize(descriptorInfo, deviceAddress, serviceId, characteristicId) {
    this.info = descriptorInfo;
    this.deviceAddress_ = deviceAddress;
    this.serviceId_ = serviceId;
    this.characteristicId_ = characteristicId;

    this.descriptorFieldSet_.setPropertyDisplayNames(INFO_PROPERTY_NAMES);
    this.descriptorFieldSet_.setObject({
      id: this.info.id,
      'uuid.uuid': this.info.uuid.uuid,
    });

    this.valueControl_.load({
      deviceAddress: this.deviceAddress_,
      serviceId: this.serviceId_,
      characteristicId: this.characteristicId_,
      descriptorId: this.info.id,
    });

    const descriptorHeaderValue =
        this.shadowRoot.querySelector('.header-value');
    descriptorHeaderValue.textContent = this.info.uuid.uuid;

    const infoDiv = this.shadowRoot.querySelector('.info-container');
    infoDiv.insertBefore(
        this.valueControl_,
        this.shadowRoot.querySelector('characteristic-list'));

    const descriptorDiv = this.shadowRoot.querySelector('.flex');
    descriptorDiv.appendChild(this.descriptorFieldSet_);
  }
}

customElements.define('descriptor-list-item', DescriptorListItemElement);
