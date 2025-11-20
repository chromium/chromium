// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for ValueControl, served from chrome://bluetooth-internals/.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {GattResult, Property} from './device.mojom-webui.js';
import {connectToDevice} from './device_broker.js';
import {showSnackbar, SnackbarType} from './snackbar.js';
import {getTemplate} from './value_control.html.js';

interface ValueLoadOptions {
  deviceAddress: string;
  serviceId: string;
  characteristicId: string;
  descriptorId?: string;
  properties?: number;
}

export enum ValueDataType {
  HEXADECIMAL = 'Hexadecimal',
  UTF8 = 'UTF-8',
  DECIMAL = 'Decimal',
}

/**
 * A container for an array value that needs to be converted to multiple
 * display formats. Internally, the value is stored as an array and converted
 * to the needed display type at runtime.
 */
export class Value {
  private value_: number[];

  constructor(initialValue: number[]) {
    this.value_ = initialValue;
  }

  /**
   * Gets the backing array value.
   */
  getArray(): number[] {
    return this.value_;
  }

  /**
   * Sets the backing array value.
   */
  setArray(newValue: number[]) {
    this.value_ = newValue;
  }

  /**
   * Sets the value by converting the |newValue| string using the formatting
   * specified by |valueDataType|.
   */
  setAs(valueDataType: ValueDataType, newValue: string) {
    switch (valueDataType) {
      case ValueDataType.HEXADECIMAL:
        this.setValueFromHex_(newValue);
        break;

      case ValueDataType.UTF8:
        this.setValueFromUtf8_(newValue);
        break;

      case ValueDataType.DECIMAL:
        this.setValueFromDecimal_(newValue);
        break;
    }
  }

  /**
   * Gets the value as a string representing the given |valueDataType|.
   */
  getAs(valueDataType: ValueDataType): string {
    switch (valueDataType) {
      case ValueDataType.HEXADECIMAL:
        return this.toHex_();

      case ValueDataType.UTF8:
        return this.toUtf8_();

      case ValueDataType.DECIMAL:
        return this.toDecimal_();
    }
  }

  /**
   * Converts the value to a hex string.
   */
  private toHex_(): string {
    if (this.value_.length === 0) {
      return '';
    }

    return this.value_.reduce(function(result, value, _index) {
      return result + ('0' + value.toString(16)).substr(-2);
    }, '0x');
  }

  /**
   * Sets the value from a hex string.
   */
  private setValueFromHex_(newValue: string) {
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
   */
  private toUtf8_(): string {
    return this.value_.reduce(function(result, value) {
      return result + String.fromCharCode(value);
    }, '');
  }

  /**
   * Sets the value from a UTF-8 encoded text string.
   */
  private setValueFromUtf8_(newValue: string) {
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
   */
  private toDecimal_(): string {
    return this.value_.join('-');
  }

  /**
   * Sets the value from a decimal string delimited by '-'.
   */
  private setValueFromDecimal_(newValue: string) {
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
 */
export class ValueControlElement extends CustomElement {
  static get is() {
    return 'value-control';
  }

  static override get template() {
    return getTemplate();
  }

  static get observedAttributes() {
    return ['data-value', 'data-options'];
  }

  private value_: Value;
  private deviceAddress_: string|null;
  private serviceId_: string|null;
  private characteristicId_: string|null;
  private descriptorId_: string|null;
  private properties_: number;
  private valueInput_: HTMLInputElement;
  private typeSelect_: HTMLSelectElement;
  private writeBtn_: HTMLButtonElement;
  private readBtn_: HTMLButtonElement;
  private unavailableMessage_: HTMLElement;

  constructor() {
    super();

    this.value_ = new Value([]);
    this.deviceAddress_ = null;
    this.serviceId_ = null;
    this.characteristicId_ = null;
    this.descriptorId_ = null;
    this.properties_ = Number.MAX_SAFE_INTEGER;
    assert(this.shadowRoot);
    this.valueInput_ = this.shadowRoot.querySelector('input')!;
    this.typeSelect_ = this.shadowRoot.querySelector('select')!;
    this.writeBtn_ = this.shadowRoot.querySelector('button.write')!;
    this.readBtn_ = this.shadowRoot.querySelector('button.read')!;
    this.unavailableMessage_ = this.shadowRoot.querySelector('h3')!;
  }

  connectedCallback() {
    this.classList.add('value-control');

    this.valueInput_.addEventListener('change', () => {
      try {
        this.value_.setAs(
            this.typeSelect_.value as ValueDataType, this.valueInput_.value);
      } catch (e) {
        showSnackbar((e as Error).message, SnackbarType.ERROR);
      }
    });

    this.typeSelect_.addEventListener('change', () => this.redraw());

    this.readBtn_.addEventListener('click', () => this.readValue_());

    this.writeBtn_.addEventListener('click', () => this.writeValue_());

    this.redraw();
  }

  /**
   * Sets the settings used by the value control and redraws the control to
   * match the read/write settings in |options.properties|. If properties
   * are not provided, no restrictions on reading/writing are applied.
   */
  attributeChangedCallback(name: string, oldValue: string, newValue: string) {
    assert(name === 'data-value' || name === 'data-options');

    if (oldValue === newValue) {
      return;
    }

    if (name === 'data-options') {
      const options = JSON.parse(newValue) as ValueLoadOptions;
      this.deviceAddress_ = options.deviceAddress;
      this.serviceId_ = options.serviceId;
      this.characteristicId_ = options.characteristicId;
      this.descriptorId_ = options.descriptorId ?? null;

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

    this.valueInput_.value =
        this.value_.getAs(this.typeSelect_.value as ValueDataType);
  }

  /**
   * Sets the value of the control.
   */
  setValue(value: number[]) {
    this.value_.setArray(value);
    this.redraw();
  }

  /**
   * Gets an error string describing the given |result| code.
   */
  private getErrorString_(result: GattResult): string {
    // TODO(crbug.com/40492643): Replace with more descriptive error
    // messages.
    return GattResult[result];
  }

  /**
   * Called when the read button is pressed. Connects to the device and
   * retrieves the current value of the characteristic in the |service_id|
   * with id |characteristic_id|. If |descriptor_id| is defined,  the
   * descriptor value with |descriptor_id| is read instead.
   */
  private readValue_() {
    this.readBtn_.disabled = true;

    assert(this.deviceAddress_);
    connectToDevice(this.deviceAddress_)
        .then(device => {
          assert(this.serviceId_);
          assert(this.characteristicId_);
          if (this.descriptorId_) {
            return device.readValueForDescriptor(
                this.serviceId_, this.characteristicId_, this.descriptorId_);
          }

          return device.readValueForCharacteristic(
              this.serviceId_, this.characteristicId_);
        })
        .then(response => {
          this.readBtn_.disabled = false;
          assert(this.deviceAddress_);

          if (response.result === GattResult.SUCCESS) {
            this.setValue(response.value!);
            showSnackbar(
                this.deviceAddress_ + ': Read succeeded', SnackbarType.SUCCESS);
            return;
          }

          const errorString = this.getErrorString_(response.result);
          showSnackbar(
              this.deviceAddress_ + ': ' + errorString, SnackbarType.ERROR,
              'Retry', () => this.readValue_());
        });
  }

  /**
   * Called when the write button is pressed. Connects to the device and
   * retrieves the current value of the characteristic in the
   * |service_id| with id |characteristic_id|. If |descriptor_id| is defined,
   * the descriptor value with |descriptor_id| is written instead.
   */
  private writeValue_() {
    this.writeBtn_.disabled = true;

    assert(this.deviceAddress_);
    connectToDevice(this.deviceAddress_)
        .then(device => {
          assert(this.serviceId_);
          assert(this.characteristicId_);
          if (this.descriptorId_) {
            return device.writeValueForDescriptor(
                this.serviceId_, this.characteristicId_, this.descriptorId_,
                this.value_.getArray());
          }

          return device.writeValueForCharacteristic(
              this.serviceId_, this.characteristicId_, this.value_.getArray());
        })
        .then(response => {
          this.writeBtn_.disabled = false;
          assert(this.deviceAddress_);

          if (response.result === GattResult.SUCCESS) {
            showSnackbar(
                this.deviceAddress_ + ': Write succeeded',
                SnackbarType.SUCCESS);
            return;
          }

          const errorString = this.getErrorString_(response.result);
          showSnackbar(
              this.deviceAddress_ + ': ' + errorString, SnackbarType.ERROR,
              'Retry', () => this.writeValue_());
        });
  }
}

customElements.define('value-control', ValueControlElement);

declare global {
  interface HTMLElementTagNameMap {
    'value-control': ValueControlElement;
  }
}
