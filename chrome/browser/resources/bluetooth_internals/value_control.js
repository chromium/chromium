// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for ValueControl, served from chrome://bluetooth-internals/.
 */

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {GattResult, Property} from './device.mojom-webui.js';
import {connectToDevice} from './device_broker.js';
import {showSnackbar, SnackbarType} from './snackbar.js';
import {getTemplate} from './value_control.html.js';

/**
 * @typedef {{
 *    deviceAddress: string,
 *    serviceId: string,
 *    characteristicId: string,
 *    descriptorId: (string|undefined),
 *    properties: (number|undefined),
 *  }}
 */
let ValueLoadOptions;

/** @enum {string}  */
export const ValueDataType = {
  HEXADECIMAL: 'Hexadecimal',
  UTF8: 'UTF-8',
  DECIMAL: 'Decimal',
};

/**
 * A container for an array value that needs to be converted to multiple
 * display formats. Internally, the value is stored as an array and converted
 * to the needed display type at runtime.
 */
export class Value {
  /** @param {!Array<number>} initialValue */
  constructor(initialValue) {
    /** @private {!Array<number>} */
    this.value_ = initialValue;
  }

  /**
   * Gets the backing array value.
   * @return {!Array<number>}
   */
  getArray() {
    return this.value_;
  }

  /**
   * Sets the backing array value.
   * @param {!Array<number>} newValue
   */
  setArray(newValue) {
    this.value_ = newValue;
  }

  /**
   * Sets the value by converting the |newValue| string using the formatting
   * specified by |valueDataType|.
   * @param {!ValueDataType} valueDataType
   * @param {string} newValue
   */
  setAs(valueDataType, newValue) {
    switch (valueDataType) {
      case ValueDataType.HEXADECIMAL:
        this.setValueFromHex_(newValue);
        break;

      case ValueDataType.UTF8:
        this.setValueFromUTF8_(newValue);
        break;

      case ValueDataType.DECIMAL:
        this.setValueFromDecimal_(newValue);
        break;
    }
  }

  /**
   * Gets the value as a string representing the given |valueDataType|.
   * @param {!ValueDataType} valueDataType
   * @return {string}
   */
  getAs(valueDataType) {
    switch (valueDataType) {
      case ValueDataType.HEXADECIMAL:
        return this.toHex_();

      case ValueDataType.UTF8:
        return this.toUTF8_();

      case ValueDataType.DECIMAL:
        return this.toDecimal_();
    }
    assertNotReached();
    return '';
  }

  /**
   * Converts the value to a hex string.
   * @return {string}
   * @private
   */
  toHex_() {
    if (this.value_.length === 0) {
      return '';
    }

    return this.value_.reduce(function(result, value, index) {
      return result + ('0' + value.toString(16)).substr(-2);
    }, '0x');
  }

  /**
   * Sets the value from a hex string.
   * @param {string} newValue
   * @private
   */
  setValueFromHex_(newValue) {
    if (!newValue) {
      this.value_ = [];
      return;
    }

    if (!newValue.startsWith('0x')) {
      throw new Error('Expected new value to start with "0x".');
    }

    const result = [];
    for (let i = 2; i < newValue.length; i += 2) {
      result.push(parseInt(newValue.substr(i, 2), 16));
    }

    this.value_ = result;
  }

  /**
   * Converts the value to a UTF-8 encoded text string.
   * @return {string}
   * @private
   */
  toUTF8_() {
    return this.value_.reduce(function(result, value) {
      return result + String.fromCharCode(value);
    }, '');
  }

  /**
   * Sets the value from a UTF-8 encoded text string.
   * @param {string} newValue
   * @private
   */
  setValueFromUTF8_(newValue) {
    if (!newValue) {
      this.value_ = [];
      return;
    }

    this.value_ = Array.from(newValue).map(function(char) {
      return char.charCodeAt(0);
    });
  }

  /**
   * Converts the value to a decimal string with numbers delimited by '-'.
   * @return {string}
   * @private
   */
  toDecimal_() {
    return this.value_.join('-');
  }

  /**
   * Sets the value from a decimal string delimited by '-'.
   * @param {string} newValue
   * @private
   */
  setValueFromDecimal_(newValue) {
    if (!newValue) {
      this.value_ = [];
      return;
    }

    if (!/^[0-9\-]*$/.test(newValue)) {
      throw new Error('New value can only contain numbers and hyphens.');
    }

    this.value_ = newValue.split('-').map(function(val) {
      return parseInt(val, 10);
    });
  }
}

/**
 * A set of inputs that allow a user to request reads and writes of values.
 * This control allows the value to be displayed in multiple forms
 * as defined by the |ValueDataType| array. Values must be written
 * in these formats. Read and write capability is controlled by a
 * 'properties' bitfield provided by the characteristic.
 * @constructor
 * @extends {HTMLDivElement}
 */
export class ValueControlElement extends CustomElement {
  static get is() {
    return 'value-control';
  }

  static get template() {
    return getTemplate();
  }

  static get observedAttributes() {
    return ['data-value', 'data-options'];
  }

  constructor() {
    super();

    /** @private {!Value} */
    this.value_ = new Value([]);
    /** @private {?string} */
    this.deviceAddress_ = null;
    /** @private {?string} */
    this.serviceId_ = null;
    /** @private {?string} */
    this.characteristicId_ = null;
    /** @private {?string|undefined} */
    this.descriptorId_ = null;
    /** @private {number} */
    this.properties_ = Number.MAX_SAFE_INTEGER;
    /** @private {!HTMLInputElement} */
    this.valueInput_ = this.shadowRoot.querySelector('input');
    /** @private {!HTMLSelectElement} */
    this.typeSelect_ = this.shadowRoot.querySelector('select');
    /** @private {!HTMLButtonElement} */
    this.writeBtn_ = this.shadowRoot.querySelector('button.write');
    /** @private {!HTMLButtonElement} */
    this.readBtn_ = this.shadowRoot.querySelector('button.read');
    /** @private {!HTMLElement} */
    this.unavailableMessage_ = this.shadowRoot.querySelector('h3');
  }

  connectedCallback() {
    this.classList.add('value-control');

    this.valueInput_.addEventListener('change', function() {
      try {
        this.value_.setAs(this.typeSelect_.value, this.valueInput_.value);
      } catch (e) {
        showSnackbar(e.message, SnackbarType.ERROR);
      }
    }.bind(this));

    this.typeSelect_.addEventListener('change', this.redraw.bind(this));

    this.readBtn_.addEventListener('click', this.readValue_.bind(this));

    this.writeBtn_.addEventListener('click', this.writeValue_.bind(this));

    this.redraw();
  }

  /**
   * Sets the settings used by the value control and redraws the control to
   * match the read/write settings in |options.properties|. If properties
   * are not provided, no restrictions on reading/writing are applied.
   */
  attributeChangedCallback(name, oldValue, newValue) {
    assert(name === 'data-value' || name === 'data-options');

    if (oldValue === newValue) {
      return;
    }

    if (name === 'data-options') {
      const options = JSON.parse(newValue);
      this.deviceAddress_ = options.deviceAddress;
      this.serviceId_ = options.serviceId;
      this.characteristicId_ = options.characteristicId;
      this.descriptorId_ = options.descriptorId;

      if (options.properties) {
        this.properties_ = options.properties;
      }
    } else {
      this.value_.setArray(JSON.parse(newValue));
    }

    this.redraw();
  }

  /**
   * Redraws the value control with updated layout depending on the
   * availability of reads and writes and the current cached value.
   */
  redraw() {
    this.readBtn_.hidden = (this.properties_ & Property.READ) === 0;
    this.writeBtn_.hidden = (this.properties_ & Property.WRITE) === 0 &&
        (this.properties_ & Property.WRITE_WITHOUT_RESPONSE) === 0;

    const isAvailable = !this.readBtn_.hidden || !this.writeBtn_.hidden;
    this.unavailableMessage_.hidden = isAvailable;
    this.valueInput_.hidden = !isAvailable;
    this.typeSelect_.hidden = !isAvailable;

    if (!isAvailable) {
      return;
    }

    this.valueInput_.value = this.value_.getAs(this.typeSelect_.value);
  }

  /**
   * Sets the value of the control.
   * @param {!Array<number>} value
   */
  setValue(value) {
    this.value_.setArray(value);
    this.redraw();
  }

  /**
   * Gets an error string describing the given |result| code.
   * @param {!GattResult} result
   * @private
   */
  getErrorString_(result) {
    // TODO(crbug.com/40492643): Replace with more descriptive error
    // messages.
    return Object.keys(GattResult).find(function(key) {
      return GattResult[key] === result;
    });
  }

  /**
   * Called when the read button is pressed. Connects to the device and
   * retrieves the current value of the characteristic in the |service_id|
   * with id |characteristic_id|. If |descriptor_id| is defined,  the
   * descriptor value with |descriptor_id| is read instead.
   * @private
   */
  readValue_() {
    this.readBtn_.disabled = true;

    assert(this.deviceAddress_);
    connectToDevice(this.deviceAddress_)
        .then(function(device) {
          if (this.descriptorId_) {
            return device.readValueForDescriptor(
                this.serviceId_, this.characteristicId_, this.descriptorId_);
          }

          return device.readValueForCharacteristic(
              this.serviceId_, this.characteristicId_);
        }.bind(this))
        .then(function(response) {
          this.readBtn_.disabled = false;

          if (response.result === GattResult.SUCCESS) {
            this.setValue(response.value);
            showSnackbar(
                this.deviceAddress_ + ': Read succeeded', SnackbarType.SUCCESS);
            return;
          }

          const errorString = this.getErrorString_(response.result);
          showSnackbar(
              this.deviceAddress_ + ': ' + errorString, SnackbarType.ERROR,
              'Retry', this.readValue_.bind(this));
        }.bind(this));
  }

  /**
   * Called when the write button is pressed. Connects to the device and
   * retrieves the current value of the characteristic in the
   * |service_id| with id |characteristic_id|. If |descriptor_id| is defined,
   * the descriptor value with |descriptor_id| is written instead.
   * @private
   */
  writeValue_() {
    this.writeBtn_.disabled = true;

    assert(this.deviceAddress_);
    connectToDevice(this.deviceAddress_)
        .then(function(device) {
          if (this.descriptorId_) {
            return device.writeValueForDescriptor(
                this.serviceId_, this.characteristicId_, this.descriptorId_,
                this.value_.getArray());
          }

          return device.writeValueForCharacteristic(
              this.serviceId_, this.characteristicId_, this.value_.getArray());
        }.bind(this))
        .then(function(response) {
          this.writeBtn_.disabled = false;

          if (response.result === GattResult.SUCCESS) {
            showSnackbar(
                this.deviceAddress_ + ': Write succeeded',
                SnackbarType.SUCCESS);
            return;
          }

          const errorString = this.getErrorString_(response.result);
          showSnackbar(
              this.deviceAddress_ + ': ' + errorString, SnackbarType.ERROR,
              'Retry', this.writeValue_.bind(this));
        }.bind(this));
  }
}

customElements.define('value-control', ValueControlElement);
