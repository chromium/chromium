// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DeviceTable UI, served from chrome://bluetooth-internals/.
 */
import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {DeviceCollection} from './device_collection.js';
import {getTemplate} from './device_table.html.js';
import {formatManufacturerDataMap, formatServiceUuids} from './device_utils.js';

const COLUMNS = {
  NAME: 0,
  ADDRESS: 1,
  RSSI: 2,
  MANUFACTURER_DATA: 3,
  SERVICE_UUIDS: 4,
  CONNECTION_STATE: 5,
  LINKS: 6,
};

/**
 * A table that lists the devices and responds to changes in the given
 * DeviceCollection. Fires events for inspection requests from listed
 * devices.
 */
export class DeviceTableElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  static get is() {
    return 'device-table';
  }

  constructor() {
    super();

    /** @private {?DeviceCollection} */
    this.devices_ = null;

    /** @private {?HTMLTBodyElement} */
    this.body_ = null;

    /** @private {?NodeListOf<Element>} */
    this.headers_ = null;

    /** @private {!Map<!DeviceInfo, boolean>} */
    this.inspectionMap_ = new Map();
  }

  /**
   * Decorates an element as a UI element class. Caches references to the
   *    table body and headers.
   */
  connectedCallback() {
    this.body_ = this.shadowRoot.querySelector('tbody');
    this.headers_ = this.shadowRoot.querySelector('thead').rows[0].cells;
  }

  /**
   * Sets the tables device collection.
   * @param {!DeviceCollection} deviceCollection
   */
  setDevices(deviceCollection) {
    assert(!this.devices_, 'Devices can only be set once.');

    this.devices_ = deviceCollection;
    this.devices_.addEventListener(
        'device-update', this.handleDeviceUpdate_.bind(this));
    this.devices_.addEventListener(
        'device-added', this.handleDeviceAdded_.bind(this));
    this.devices_.addEventListener(
        'devices-reset-for-test', this.redraw_.bind(this));

    this.redraw_();
  }

  /**
   * Updates the inspect status of the row matching the given |deviceInfo|.
   * If |isInspecting| is true, the forget link is enabled otherwise it's
   * disabled.
   * @param {!DeviceInfo} deviceInfo
   * @param {boolean} isInspecting
   */
  setInspecting(deviceInfo, isInspecting) {
    this.inspectionMap_.set(deviceInfo, isInspecting);
    this.updateRow_(deviceInfo, this.devices_.getByAddress(deviceInfo.address));
  }

  /**
   * Fires a forget pressed event for the row |index|.
   * @param {number} index
   * @private
   */
  handleForgetClick_(index) {
    const event = new CustomEvent('forgetpressed', {
      bubbles: true,
      composed: true,
      detail: {
        address: this.devices_.item(index).address,
      },
    });
    this.dispatchEvent(event);
  }

  /**
   * Updates table row on change event of the device collection.
   * @param {!CustomEvent<number>} event
   * @private
   */
  handleDeviceUpdate_(event) {
    this.updateRow_(
        /** @type {!DeviceInfo} */ (this.devices_.item(event.detail)),
        event.detail);
  }

  /**
   * Fires an inspect pressed event for the row |index|.
   * @param {number} index
   * @private
   */
  handleInspectClick_(index) {
    const event = new CustomEvent('inspectpressed', {
      bubbles: true,
      composed: true,
      detail: {
        address: this.devices_.item(index).address,
      },
    });
    this.dispatchEvent(event);
  }

  /**
   * Updates table row on splice event of the device collection.
   * @param {!CustomEvent<device: DeviceInfo, index: number>} event
   * @private
   */
  handleDeviceAdded_(event) {
    this.insertRow_(event.detail.device, event.detail.index);
  }

  /**
   * Inserts a new row at |index| and updates it with info from |device|.
   * @param {!DeviceInfo} device
   * @param {?number} index
   * @private
   */
  insertRow_(device, index) {
    const row = this.body_.insertRow(index);
    row.id = device.address;

    for (let i = 0; i < this.headers_.length; i++) {
      // Skip the LINKS column. It has no data-field attribute.
      if (i === COLUMNS.LINKS) {
        continue;
      }
      row.insertCell();
    }

    // Make two extra cells for the inspect link and connect errors.
    const inspectCell = row.insertCell();

    const inspectLink = document.createElement('a', {is: 'action-link'});
    inspectLink.setAttribute('is', 'action-link');
    inspectLink.textContent = 'Inspect';
    inspectCell.appendChild(inspectLink);
    inspectLink.addEventListener('click', function() {
      this.handleInspectClick_(row.sectionRowIndex);
    }.bind(this));

    const forgetLink = document.createElement('a', {is: 'action-link'});
    forgetLink.setAttribute('is', 'action-link');
    forgetLink.textContent = 'Forget';
    inspectCell.appendChild(forgetLink);
    forgetLink.addEventListener('click', function() {
      this.handleForgetClick_(row.sectionRowIndex);
    }.bind(this));

    this.updateRow_(device, row.sectionRowIndex);
  }

  /**
   * Deletes and recreates the table using the cached |devices_|.
   * @private
   */
  redraw_() {
    const table = this.shadowRoot.querySelector('table');
    table.removeChild(this.body_);
    table.appendChild(document.createElement('tbody'));
    this.body_ = this.shadowRoot.querySelector('tbody');
    this.body_.classList.add('table-body');

    for (let i = 0; i < this.devices_.length; i++) {
      this.insertRow_(
          /** @type {!DeviceInfo} */ (this.devices_.item(i)), null);
    }
  }

  /**
   * Updates the row at |index| with the info from |device|.
   * @param {!DeviceInfo} device
   * @param {number} index
   * @private
   */
  updateRow_(device, index) {
    const row = this.body_.rows[index];
    assert(row, 'Row ' + index + ' is not in the table.');

    row.classList.toggle('removed', this.devices_.isRemoved(device));

    const forgetLink = row.cells[COLUMNS.LINKS].children[1];

    if (this.inspectionMap_.has(device)) {
      forgetLink.disabled = !this.inspectionMap_.get(device);
    } else {
      forgetLink.disabled = true;
    }

    // Update the properties based on the header field path.
    for (let i = 0; i < this.headers_.length; i++) {
      // Skip the LINKS column. It has no data-field attribute.
      if (i === COLUMNS.LINKS) {
        continue;
      }

      const header = this.headers_[i];
      const propName = header.dataset.field;

      const parts = propName.split('.');
      let obj = device;
      while (obj != null && parts.length > 0) {
        const part = parts.shift();
        obj = obj[part];
      }

      if (propName === 'isGattConnected') {
        obj = obj ? 'Connected' : 'Not Connected';
      } else if (propName === 'serviceUuids') {
        obj = formatServiceUuids(obj);
      } else if (propName === 'manufacturerDataMap') {
        obj = formatManufacturerDataMap(obj);
      }

      const cell = row.cells[i];
      cell.textContent = obj == null ? 'Unknown' : obj;
      cell.dataset.label = header.textContent;
    }
  }
}

customElements.define('device-table', DeviceTableElement);
