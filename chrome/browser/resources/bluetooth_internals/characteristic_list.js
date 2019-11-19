// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for CharacteristicList and CharacteristicListItem, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('characteristic_list', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const ExpandableList = expandable_list.ExpandableList;
  const ExpandableListItem = expandable_list.ExpandableListItem;
  const Snackbar = snackbar.Snackbar;
  const SnackbarType = snackbar.SnackbarType;

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
   * @constructor
   * @param {!bluetooth.mojom.CharacteristicInfo} characteristicInfo
   * @param {string} deviceAddress
   * @param {string} serviceId
   * @extends {expandable_list.ExpandableListItem}
   */
  function CharacteristicListItem(
      characteristicInfo, deviceAddress, serviceId) {
    const listItem = new ExpandableListItem();
    listItem.__proto__ = CharacteristicListItem.prototype;

    /** @type {!bluetooth.mojom.CharacteristicInfo} */
    listItem.info = characteristicInfo;
    /** @private {string} */
    listItem.deviceAddress_ = deviceAddress;
    /** @private {string} */
    listItem.serviceId_ = serviceId;

    listItem.decorate();
    return listItem;
  }

  CharacteristicListItem.prototype = {
    __proto__: ExpandableListItem.prototype,

    /**
     * Decorates the element as a characteristic list item. Creates and caches
     * two fieldsets for displaying property values.
     * @override
     */
    decorate: function() {
      this.classList.add('characteristic-list-item');

      /** @private {!object_fieldset.ObjectFieldSet} */
      this.characteristicFieldSet_ = new object_fieldset.ObjectFieldSet();
      this.characteristicFieldSet_.setPropertyDisplayNames(INFO_PROPERTY_NAMES);
      this.characteristicFieldSet_.setObject({
        id: this.info.id,
        'uuid.uuid': this.info.uuid.uuid,
      });

      /** @private {!object_fieldset.ObjectFieldSet} */
      this.propertiesFieldSet_ = new object_fieldset.ObjectFieldSet();
      this.propertiesFieldSet_.setPropertyDisplayNames(
          PROPERTIES_PROPERTY_NAMES);
      const Property = bluetooth.mojom.Property;
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

      /** @private {!value_control.ValueControl} */
      this.valueControl_ = new value_control.ValueControl();

      this.valueControl_.load({
        deviceAddress: this.deviceAddress_,
        serviceId: this.serviceId_,
        characteristicId: this.info.id,
        properties: this.info.properties,
      });
      this.valueControl_.setValue(this.info.lastKnownValue);

      /** @private {!descriptor_list.DescriptorList} */
      this.descriptorList_ = new descriptor_list.DescriptorList();

      // Create content for display in brief content container.
      const characteristicHeaderText = document.createElement('div');
      characteristicHeaderText.textContent = 'Characteristic:';

      const characteristicHeaderValue = document.createElement('div');
      characteristicHeaderValue.textContent = this.info.uuid.uuid;

      const characteristicHeader = document.createElement('div');
      characteristicHeader.appendChild(characteristicHeaderText);
      characteristicHeader.appendChild(characteristicHeaderValue);
      this.briefContent_.appendChild(characteristicHeader);

      // Create content for display in expanded content container.
      const characteristicInfoHeader = document.createElement('h4');
      characteristicInfoHeader.textContent = 'Characteristic Info';

      const characteristicDiv = document.createElement('div');
      characteristicDiv.classList.add('flex');
      characteristicDiv.appendChild(this.characteristicFieldSet_);

      const propertiesHeader = document.createElement('h4');
      propertiesHeader.textContent = 'Properties';

      const propertiesBtn = document.createElement('button');
      propertiesBtn.textContent = 'Show All';
      propertiesBtn.classList.add('show-all-properties');
      propertiesBtn.addEventListener('click', () => {
        this.propertiesFieldSet_.showAll = !this.propertiesFieldSet_.showAll;
        propertiesBtn.textContent =
            this.propertiesFieldSet_.showAll ? 'Hide' : 'Show all';
        this.propertiesFieldSet_.redraw();
      });
      propertiesHeader.appendChild(propertiesBtn);

      const propertiesDiv = document.createElement('div');
      propertiesDiv.classList.add('flex');
      propertiesDiv.appendChild(this.propertiesFieldSet_);

      const descriptorsHeader = document.createElement('h4');
      descriptorsHeader.textContent = 'Descriptors';

      const infoDiv = document.createElement('div');
      infoDiv.classList.add('info-container');

      const valueHeader = document.createElement('h4');
      valueHeader.textContent = 'Value';

      infoDiv.appendChild(characteristicInfoHeader);
      infoDiv.appendChild(characteristicDiv);
      infoDiv.appendChild(propertiesHeader);
      infoDiv.appendChild(propertiesDiv);
      infoDiv.appendChild(valueHeader);
      infoDiv.appendChild(this.valueControl_);
      infoDiv.appendChild(descriptorsHeader);
      infoDiv.appendChild(this.descriptorList_);

      this.expandedContent_.appendChild(infoDiv);
    },

    /** @override */
    onExpandInternal: function(expanded) {
      this.descriptorList_.load(
          this.deviceAddress_, this.serviceId_, this.info.id);
    },
  };

  /**
   * A list that displays CharacteristicListItems.
   * @constructor
   * @extends {expandable_list.ExpandableList}
   */
  const CharacteristicList = cr.ui.define('list');

  CharacteristicList.prototype = {
    __proto__: ExpandableList.prototype,

    /** @override */
    decorate: function() {
      ExpandableList.prototype.decorate.call(this);

      /** @private {?string} */
      this.deviceAddress_ = null;
      /** @private {?string} */
      this.serviceId_ = null;
      /** @private {boolean} */
      this.characteristicsRequested_ = false;

      this.classList.add('characteristic-list');
      this.setEmptyMessage('No Characteristics Found');
    },

    createItem: function(data) {
      return new CharacteristicListItem(
          data, assert(this.deviceAddress_), assert(this.serviceId_));
    },

    /**
     * Loads the characteristic list with an array of CharacteristicInfo from
     * the device with |deviceAddress| and service with |serviceId|. If no
     * active connection to the device exists, one is created.
     * @param {string} deviceAddress
     * @param {string} serviceId
     */
    load: function(deviceAddress, serviceId) {
      if (this.characteristicsRequested_ || !this.isSpinnerShowing()) {
        return;
      }

      this.deviceAddress_ = deviceAddress;
      this.serviceId_ = serviceId;
      this.characteristicsRequested_ = true;

      device_broker.connectToDevice(deviceAddress)
          .then(function(device) {
            return device.getCharacteristics(serviceId);
          }.bind(this))
          .then(function(response) {
            this.setData(new ArrayDataModel(response.characteristics || []));
            this.setSpinnerShowing(false);
            this.characteristicsRequested_ = false;
          }.bind(this))
          .catch(function(error) {
            this.characteristicsRequested_ = false;
            Snackbar.show(
                deviceAddress + ': ' + error.message, SnackbarType.ERROR,
                'Retry', function() {
                  this.load(deviceAddress, serviceId);
                }.bind(this));
          }.bind(this));
    },
  };

  return {
    CharacteristicList: CharacteristicList,
    CharacteristicListItem: CharacteristicListItem,
  };
});
