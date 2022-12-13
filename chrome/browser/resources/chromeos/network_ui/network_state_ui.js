// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {CrosNetworkConfig, CrosNetworkConfigRemote, FilterType, ManagedProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_state_ui.html.js';
import {NetworkUIBrowserProxy, NetworkUIBrowserProxyImpl} from './network_ui_browser_proxy.js';

/**
 * @typedef {!OncMojo.DeviceStateProperties|!OncMojo.NetworkStateProperties}
 */
OncMojo.StateProperties;

Polymer({
  is: 'network-state-ui',

  _template: getTemplate(),

  properties: {},

  /**
   * Properties to display in the network state table. Each entry can be either
   * a single state field or an array of state fields. If more than one is
   * specified then the first non empty value is used.
   * @const
   */
  NETWORK_STATE_FIELDS: [
    'guid',
    'name',
    'type',
    'connectionState',
    'connectable',
    'errorState',
    'wifi.security',
    ['cellular.networkTechnology', 'EAP.EAP'],
    'cellular.activationState',
    'cellular.roaming',
    'wifi.frequency',
    'wifi.signalStrength',
  ],

  /** @const */
  FAVORITE_STATE_FIELDS: ['guid', 'name', 'type', 'source'],

  /** @const */
  DEVICE_STATE_FIELDS: ['type', 'deviceState'],

  /**
   * This UI will use both the networkingPrivate extension API and the
   * networkConfig mojo API until we provide all of the required functionality
   * in networkConfig. TODO(stevenjb): Remove use of networkingPrivate api.
   * @private {?CrosNetworkConfigRemote}
   */
  networkConfig_: null,

  /** @private {!NetworkUIBrowserProxy} */
  browserProxy_: NetworkUIBrowserProxyImpl.getInstance(),

  /** @override */
  created() {
    this.networkConfig_ = CrosNetworkConfig.getRemote();
  },

  /** @override */
  attached() {
    /** Set the refresh rate if the interval is found in the url. */
    const interval = new URL(window.location.href).searchParams.get('refresh');
    if (interval) {
      window.setInterval(() => {
        this.requestNetworks_();
      }, parseInt(interval, 10) * 1000);
    }
    this.requestNetworks_();
  },

  /**
   * Creates and returns a typed HTMLTableCellElement.
   * @return {!HTMLTableCellElement} A new td element.
   * @private
   */
  createTableCellElement_() {
    return /** @type {!HTMLTableCellElement} */ (document.createElement('td'));
  },

  /**
   * Creates and returns a typed HTMLTableRowElement.
   * @return {!HTMLTableRowElement} A new tr element.
   * @private
   */
  createTableRowElement_() {
    return /** @type {!HTMLTableRowElement} */ (document.createElement('tr'));
  },

  /**
   * @param {string} key
   * @param {number|string|undefined} value
   * @return {string}
   * @private
   */
  getOncTypeString_(key, value) {
    if (value === undefined) {
      return '';
    }
    if (key == 'type' && value == 'etherneteap') {
      // Special case, not in production UI.
      return 'EthernetEAP';
    }
    return /** @type {string}*/ (OncMojo.getTypeString(key, value));
  },

  /**
   * Returns the ONC data property for |state| associated with a key. Used
   * to access properties in the state by |key| which may may refer to a
   * nested property, e.g. 'WiFi.Security'. If any part of a nested key is
   * missing, this will return undefined.
   *
   * @param {!OncMojo.StateProperties} state
   * @param {string} key The ONC key for the property.
   * @return {*} The value associated with the property or undefined if the
   *     key (any part of it) is not defined.
   * @private
   */
  getOncProperty_(state, key) {
    let dict = /** @type {!Object} */ (state);
    const keys = key.split('.');
    while (keys.length > 1) {
      const k = keys.shift();
      dict = dict[k];
      if (!dict || typeof dict != 'object') {
        return undefined;
      }
    }
    const k = keys.shift();
    return this.getOncTypeString_(k, dict[k]);
  },

  /**
   * Creates a cell with a button for expanding a network state table row.
   * @param {!OncMojo.StateProperties} state
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     given value.
   * @private
   */
  createStateTableExpandButton_(state) {
    const cell = this.createTableCellElement_();
    cell.className = 'state-table-expand-button-cell';
    const button = document.createElement('button');
    button.addEventListener('click', (event) => {
      this.toggleExpandRow_(/** @type {!HTMLElement} */ (event.target), state);
    });
    button.className = 'state-table-expand-button';
    button.textContent = '+';
    cell.appendChild(button);
    return cell;
  },

  /**
   * Creates a cell with an icon representing the network state.
   * @param {!OncMojo.StateProperties} state
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     icon.
   * @private
   */
  createStateTableIcon_(state) {
    const cell = this.createTableCellElement_();
    cell.className = 'state-table-icon-cell';
    const icon = /** @type {!NetworkIconElement} */ (
        document.createElement('network-icon'));
    icon.isListItem = true;
    icon.networkState = OncMojo.getDefaultNetworkState(state.type);
    cell.appendChild(icon);
    return cell;
  },

  /**
   * Creates a cell in the network state table.
   * @param {*} value Content in the cell.
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     given value.
   * @private
   */
  createStateTableCell_(value) {
    const cell = this.createTableCellElement_();
    cell.textContent = value || '';
    return cell;
  },

  /**
   * Creates a row in the network state table.
   * @param {Array} stateFields The state fields to use for the row.
   * @param {!OncMojo.StateProperties} state
   * @return {!HTMLTableRowElement} The created tr element that contains the
   *     network state information.
   * @private
   */
  createStateTableRow_(stateFields, state) {
    const row = this.createTableRowElement_();
    row.className = 'state-table-row';
    row.appendChild(this.createStateTableExpandButton_(state));
    row.appendChild(this.createStateTableIcon_(state));
    for (let i = 0; i < stateFields.length; ++i) {
      const field = stateFields[i];
      let value;
      if (typeof field == 'string') {
        value = this.getOncProperty_(state, field);
      } else {
        for (let j = 0; j < field.length; ++j) {
          value = this.getOncProperty_(state, field[j]);
          if (value != undefined) {
            break;
          }
        }
      }
      if (field == 'guid') {
        value = value.slice(0, 8);
      }
      row.appendChild(this.createStateTableCell_(value));
    }
    return row;
  },

  /**
   * Creates a table for networks or favorites.
   * @param {string} tablename The name of the table to be created.
   * @param {!Array<string>} stateFields The list of fields for the table.
   * @param {!Array<!OncMojo.StateProperties>} states
   * @private
   */
  createStateTable_(tablename, stateFields, states) {
    const table = this.$$('#' + tablename);
    const oldRows = table.querySelectorAll('.state-table-row');
    for (let i = 0; i < oldRows.length; ++i) {
      table.removeChild(oldRows[i]);
    }
    states.forEach((state) => {
      table.appendChild(this.createStateTableRow_(stateFields, state));
    });
  },

  /**
   * Returns a valid HTMLElement id from |guid|.
   * @param {string} guid A GUID which may start with a digit.
   * @return {string} A valid HTMLElement id.
   * @private
   */
  idFromGuid_(guid) {
    return '_' + guid.replace(/[{}]/g, '');
  },

  /**
   * Returns a valid HTMLElement id from |type|. Note: |type| may be a Shill
   * type or an ONC type, so strip _ and convert to lowercase to unify them.
   * @param {string} type A Shill or ONC network type
   * @return {string} A valid HTMLElement id.
   * @private
   */
  idFromTypeString_(type) {
    return '_' + type.replace(/[{}_]/g, '').toLowerCase();
  },

  /**
   * @param {!NetworkType} type
   * @return {string} A valid HTMLElement id.
   * @private
   */
  idFromType_(type) {
    return this.idFromTypeString_(this.getOncTypeString_('type', type));
  },

  /**
   * This callback function is triggered when visible networks are received.
   * @param {!Array<!OncMojo.NetworkStateProperties>} states
   * @private
   */
  onVisibleNetworksReceived_(states) {
    this.createStateTable_(
        'network-state-table', this.NETWORK_STATE_FIELDS, states);
  },

  /**
   * This callback function is triggered when favorite networks are received.
   * @param {!Array<!OncMojo.NetworkStateProperties>} states
   * @private
   */
  onFavoriteNetworksReceived_(states) {
    this.createStateTable_(
        'favorite-state-table', this.FAVORITE_STATE_FIELDS, states);
  },

  /**
   * This callback function is triggered when device states are received.
   * @param {!Array<!OncMojo.DeviceStateProperties>} states
   * @private
   */
  onDeviceStatesReceived_(states) {
    this.createStateTable_(
        'device-state-table', this.DEVICE_STATE_FIELDS, states);
  },

  /**
   * @return {string}
   * @private
   */
  getSelectedFormat_() {
    const formatSelect = this.$$('#get-property-format');
    return formatSelect.options[formatSelect.selectedIndex].value;
  },

  /**
   * Toggles the button state and add or remove a row displaying the complete
   * state information for a row.
   * @param {!HTMLElement} btn The button that was clicked.
   * @param {!OncMojo.StateProperties} state
   * @private
   */
  toggleExpandRow_(btn, state) {
    const cell = btn.parentNode;
    const row = /** @type {!HTMLTableRowElement} */ (cell.parentNode);
    if (btn.textContent == '-') {
      btn.textContent = '+';
      row.parentNode.removeChild(row.nextSibling);
    } else {
      btn.textContent = '-';
      const expandedRow = this.createExpandedRow_(state, row);
      row.parentNode.insertBefore(expandedRow, row.nextSibling);
    }
  },

  /**
   * Creates the expanded row for displaying the complete state as JSON.
   * @param {!HTMLTableRowElement} baseRow The unexpanded row associated with
   *     the new row.
   * @return {!HTMLTableRowElement} The created tr element for the expanded row.
   * @private
   */
  createExpandedRow_(state, baseRow) {
    assert(state);
    const guid = state.guid || '';
    const expandedRow = this.createTableRowElement_();
    expandedRow.className = 'state-table-row';
    const emptyCell = this.createTableCellElement_();
    emptyCell.style.border = 'none';
    expandedRow.appendChild(emptyCell);
    const detailCell = this.createTableCellElement_();
    detailCell.id =
        guid ? this.idFromGuid_(guid) : this.idFromType_(state.type);
    detailCell.className = 'state-table-expanded-cell';
    detailCell.colSpan = baseRow.childNodes.length - 1;
    expandedRow.appendChild(detailCell);
    if (guid) {
      this.handleNetworkDetail_(guid, this.getSelectedFormat_(), detailCell);
    } else {
      this.handleDeviceDetail_(state, this.getSelectedFormat_(), detailCell);
    }
    return expandedRow;
  },

  /**
   * Requests network details and calls showDetail_ with the result.
   * @param {string} guid
   * @param {string} selectedId
   * @param {!HTMLTableCellElement} detailCell
   * @private
   */
  handleNetworkDetail_(guid, selectedId, detailCell) {
    if (selectedId == 'shill') {
      this.browserProxy_.getShillNetworkProperties(guid).then((response) => {
        this.getShillNetworkPropertiesResult_(response);
      });
    } else if (selectedId == 'state') {
      this.networkConfig_.getNetworkState(guid)
          .then((responseParams) => {
            if (responseParams && responseParams.result) {
              this.showDetail_(detailCell, responseParams.result);
            } else {
              this.showDetailError_(
                  detailCell, 'getNetworkState(' + guid + ') failed');
            }
          })
          .catch((error) => {
            this.showDetailError_(detailCell, 'Mojo service failure: ' + error);
          });
    } else if (selectedId == 'managed') {
      this.networkConfig_.getManagedProperties(guid)
          .then((responseParams) => {
            if (responseParams && responseParams.result) {
              this.showDetail_(detailCell, responseParams.result);
            } else {
              this.showDetailError_(
                  detailCell, 'getManagedProperties(' + guid + ') failed');
            }
          })
          .catch((error) => {
            this.showDetailError_(detailCell, 'Mojo service failure: ' + error);
          });
    } else {
      chrome.networkingPrivate.getProperties(guid, (properties) => {
        this.showDetail_(detailCell, properties, chrome.runtime.lastError);
      });
    }
  },

  /**
   * Requests network details and calls showDetail_ with the result.
   * @param {!OncMojo.StateProperties} state
   * @param {string} selectedId
   * @param {!HTMLTableCellElement} detailCell
   * @private
   */
  handleDeviceDetail_(state, selectedId, detailCell) {
    if (selectedId == 'shill') {
      this.browserProxy_
          .getShillDeviceProperties(OncMojo.getNetworkTypeString(state.type))
          .then((response) => {
            this.getShillDevicePropertiesResult_(response);
          });
    } else {
      this.showDetail_(detailCell, state);
    }
  },

  /**
   * @param {!HTMLTableCellElement} detailCell
   * @param {!OncMojo.NetworkStateProperties|!OncMojo.DeviceStateProperties|
   *         !ManagedProperties|
   *         !chrome.networkingPrivate.NetworkProperties} state
   * @param {!Object=} error
   * @private
   */
  showDetail_(detailCell, state, error) {
    if (error && error.message) {
      this.showDetailError_(detailCell, error.message);
      return;
    }
    detailCell.textContent = JSON.stringify(state, (key, value) => {
      return typeof value === 'bigint' ? value.toString() : value;
    }, '\t');
  },

  /**
   * @param {!HTMLTableCellElement} detailCell
   * @param {string} error
   * @private
   */
  showDetailError_(detailCell, error) {
    detailCell.textContent = error;
  },

  /**
   * Callback invoked by Chrome after a getShillNetworkProperties call.
   * @param {Array} args The requested Shill properties. Will contain
   *     just the 'GUID' and 'ShillError' properties if the call failed.
   * @private
   */
  getShillNetworkPropertiesResult_(args) {
    const properties = args.shift();
    const guid = properties['GUID'];
    if (!guid) {
      console.error('No GUID in getShillNetworkPropertiesResult_');
      return;
    }

    const detailCell = this.$$('td#' + this.idFromGuid_(guid));
    if (!detailCell) {
      console.error('No cell for GUID: ' + guid);
      return;
    }

    if (properties['ShillError']) {
      detailCell.textContent = properties['ShillError'];
    } else {
      detailCell.textContent = JSON.stringify(properties, null, '\t');
    }
  },

  /**
   * Callback invoked by Chrome after a getShillDeviceProperties call.
   * @param {Array} args The requested Shill properties. Will contain
   *     just the 'Type' and 'ShillError' properties if the call failed.
   * @private
   */
  getShillDevicePropertiesResult_(args) {
    const properties = args.shift();
    const type = properties['Type'];
    if (!type) {
      console.error('No Type in getShillDevicePropertiesResult_');
      return;
    }

    const detailCell = this.$$('td#' + this.idFromTypeString_(type));
    if (!detailCell) {
      console.error('No cell for Type: ' + type);
      return;
    }

    if (properties['ShillError']) {
      detailCell.textContent = properties['ShillError'];
    } else {
      detailCell.textContent = JSON.stringify(properties, null, '\t');
    }
  },

  /**
   * Callback invoked by Chrome after a getShillEthernetEAP call.
   * @param {!Array} result The first arg will be the EAP properties if any.
   * @private
   */
  getShillEthernetEAPResult_(result) {
    const state = result.shift();
    const states = [];
    if (state) {
      // |state.type| is expected to be the string "etherneteap", which is not
      // supported by the rest of this UI. Use the kEthernet constant instead.
      // See https://crbug.com/1213176.
      state.type = NetworkType.kEthernet;
      states.push(state);
    }
    this.createStateTable_(
        'ethernet-eap-state-table', this.FAVORITE_STATE_FIELDS, states);
  },

  /**
   * Requests an update of all network info.
   * @private
   */
  requestNetworks_() {
    this.networkConfig_
        .getNetworkStateList({
          filter: FilterType.kVisible,
          networkType: NetworkType.kAll,
          limit: NO_LIMIT,
        })
        .then((responseParams) => {
          this.onVisibleNetworksReceived_(responseParams.result);
        });

    this.networkConfig_
        .getNetworkStateList({
          filter: FilterType.kConfigured,
          networkType: NetworkType.kAll,
          limit: NO_LIMIT,
        })
        .then((responseParams) => {
          this.onFavoriteNetworksReceived_(responseParams.result);
        });

    this.networkConfig_.getDeviceStateList().then((responseParams) => {
      this.onDeviceStatesReceived_(responseParams.result);
    });

    // Only request EthernetEAP properties when the 'shill' format is selected.
    if (this.getSelectedFormat_() == 'shill') {
      this.browserProxy_.getShillEthernetEAP().then((result) => {
        this.getShillEthernetEAPResult_(result);
      });
    }
  },
});
