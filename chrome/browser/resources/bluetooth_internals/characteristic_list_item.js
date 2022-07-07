// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './descriptor_list.js';
import './expandable_list_item.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './characteristic_list_item.html.js';
import {CharacteristicInfo, Property} from './device.mojom-webui.js';
import {ObjectFieldSet} from './object_fieldset.js';
import {ValueControl} from './value_control.js';

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
    /** @private {!ObjectFieldSet} */
    this.characteristicFieldSet_ = new ObjectFieldSet();
    /** @private {!ObjectFieldSet} */
    this.propertiesFieldSet_ = new ObjectFieldSet();
    /** @private {!ValueControl} */
    this.valueControl_ = new ValueControl();
  }

  connectedCallback() {
    this.classList.add('characteristic-list-item');
    this.shadowRoot.querySelector('expandable-list-item')
        .addEventListener('list-item-expanded', () => {
          const list = this.shadowRoot.querySelector('descriptor-list');
          list.load(this.deviceAddress_, this.serviceId_, this.info.id);
        });

    const propertiesBtn = this.shadowRoot.querySelector('.show-all-properties');
    propertiesBtn.addEventListener('click', () => {
      this.propertiesFieldSet_.showAll = !this.propertiesFieldSet_.showAll;
      propertiesBtn.textContent =
          this.propertiesFieldSet_.showAll ? 'Hide' : 'Show all';
      this.propertiesFieldSet_.redraw();
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

    this.characteristicFieldSet_.setPropertyDisplayNames(INFO_PROPERTY_NAMES);
    this.characteristicFieldSet_.setObject({
      id: this.info.id,
      'uuid.uuid': this.info.uuid.uuid,
    });

    this.propertiesFieldSet_.setPropertyDisplayNames(PROPERTIES_PROPERTY_NAMES);
    this.propertiesFieldSet_.showAll = false;
    this.propertiesFieldSet_.setObject({
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

    this.valueControl_.load({
      deviceAddress: this.deviceAddress_,
      serviceId: this.serviceId_,
      characteristicId: this.info.id,
      properties: this.info.properties,
    });
    this.valueControl_.setValue(this.info.lastKnownValue);

    // Create content for display in expanded content container.
    const characteristicDiv =
        this.shadowRoot.querySelector('.characteristic-div');
    characteristicDiv.appendChild(this.characteristicFieldSet_);

    const propertiesDiv = this.shadowRoot.querySelector('.properties-div');
    propertiesDiv.appendChild(this.propertiesFieldSet_);

    const infoDiv = this.shadowRoot.querySelector('.info-container');
    infoDiv.insertBefore(
        this.valueControl_,
        this.shadowRoot.querySelector('characteristic-list'));
  }
}

customElements.define(
    'characteristic-list-item', CharacteristicListItemElement);
