// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './descriptor_list.js';
import './expandable_list_item.js';
import './object_fieldset.js';
import './value_control.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './characteristic_list_item.html.js';
import {Property} from './device.mojom-webui.js';

/** Property names for the CharacteristicInfo fieldset */
const INFO_PROPERTY_NAMES = {
  id: 'ID',
  'uuid.uuid': 'UUID',
};

/** Property names for the Properties fieldset. */
const PROPERTIES_PROPERTY_NAMES = {
  broadcast: 'Broadcast',
  read: 'Read',
  write_without_response: 'Write Without Response',
  write: 'Write',
  notify: 'Notify',
  indicate: 'Indicate',
  authenticated_signed_writes: 'Authenticated Signed Writes',
  extended_properties: 'Extended Properties',
  reliable_write: 'Reliable Write',
  writable_auxiliaries: 'Writable Auxiliaries',
  read_encrypted: 'Read Encrypted',
  write_encrypted: 'Write Encrypted',
  read_encrypted_authenticated: 'Read Encrypted Authenticated',
  write_encrypted_authenticated: 'Write Encrypted Authenticated',
};

/**
 * A list item that displays the properties of a CharacteristicInfo object.
 * Two fieldsets are created within the element: one for the primitive
 * properties, 'id' and 'uuid', and one for the 'properties' bitfield in the
 * CharacteristicInfo object.
 */
export class CharacteristicListItemElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  constructor() {
    super();

    /** @type {?CharacteristicInfo} */
    this.info = null;
    /** @private {string} */
    this.deviceAddress_ = '';
    /** @private {string} */
    this.serviceId_ = '';
  }

  connectedCallback() {
    this.classList.add('characteristic-list-item');
    this.shadowRoot.querySelector('expandable-list-item')
        .addEventListener('list-item-expanded', () => {
          const list = this.shadowRoot.querySelector('descriptor-list');
          list.load(this.deviceAddress_, this.serviceId_, this.info.id);
        });

    const propertiesBtn = this.shadowRoot.querySelector('.show-all-properties');
    const propertiesFieldSet =
        this.shadowRoot.querySelector('object-field-set.properties');
    propertiesBtn.addEventListener('click', () => {
      propertiesFieldSet.toggleAttribute(
          'show-all', !propertiesFieldSet.showAll);
      propertiesBtn.textContent =
          propertiesFieldSet.showAll ? 'Hide' : 'Show all';
    });
  }

  /**
   * @param {!CharacteristicInfo} characteristicInfo
   * @param {string} deviceAddress
   * @param {string} serviceId
   */
  initialize(characteristicInfo, deviceAddress, serviceId) {
    this.info = characteristicInfo;
    this.deviceAddress_ = deviceAddress;
    this.serviceId_ = serviceId;

    this.shadowRoot.querySelector('.header-value').textContent =
        this.info.uuid.uuid;

    // Create content for display in expanded content container.
    const characteristicFieldSet =
        this.shadowRoot.querySelector('object-field-set.characteristics');
    characteristicFieldSet.dataset.nameMap =
        JSON.stringify(INFO_PROPERTY_NAMES);
    characteristicFieldSet.dataset.value = JSON.stringify({
      id: this.info.id,
      'uuid.uuid': this.info.uuid.uuid,
    });
    characteristicFieldSet.hidden = false;

    const propertiesFieldSet =
        this.shadowRoot.querySelector('object-field-set.properties');
    propertiesFieldSet.dataset.nameMap =
        JSON.stringify(PROPERTIES_PROPERTY_NAMES);
    propertiesFieldSet.dataset.value = JSON.stringify({
      broadcast: (this.info.properties & Property.BROADCAST) > 0,
      read: (this.info.properties & Property.READ) > 0,
      write_without_response:
          (this.info.properties & Property.WRITE_WITHOUT_RESPONSE) > 0,
      write: (this.info.properties & Property.WRITE) > 0,
      notify: (this.info.properties & Property.NOTIFY) > 0,
      indicate: (this.info.properties & Property.INDICATE) > 0,
      authenticated_signed_writes:
          (this.info.properties & Property.AUTHENTICATED_SIGNED_WRITES) > 0,
      extended_properties:
          (this.info.properties & Property.EXTENDED_PROPERTIES) > 0,
      reliable_write: (this.info.properties & Property.RELIABLE_WRITE) > 0,
      writable_auxiliaries:
          (this.info.properties & Property.WRITABLE_AUXILIARIES) > 0,
      read_encrypted: (this.info.properties & Property.READ_ENCRYPTED) > 0,
      write_encrypted: (this.info.properties & Property.WRITE_ENCRYPTED) > 0,
      read_encrypted_authenticated:
          (this.info.properties & Property.READ_ENCRYPTED_AUTHENTICATED) > 0,
      write_encrypted_authenticated:
          (this.info.properties & Property.WRITE_ENCRYPTED_AUTHENTICATED) > 0,
    });
    propertiesFieldSet.hidden = false;

    const valueControl = this.shadowRoot.querySelector('value-control');
    valueControl.dataset.options = JSON.stringify({
      deviceAddress: this.deviceAddress_,
      serviceId: this.serviceId_,
      characteristicId: this.info.id,
      properties: this.info.properties,
    });
    valueControl.dataset.value = JSON.stringify(this.info.lastKnownValue);
    valueControl.hidden = false;
  }
}

customElements.define(
    'characteristic-list-item', CharacteristicListItemElement);
