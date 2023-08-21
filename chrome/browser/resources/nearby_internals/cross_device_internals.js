// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_style.css.js';
import './np_list_object.js';
import './logging_tab.js';
import '//resources/cr_elements/md_select.css.js';
import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cross_device_internals.html.js';
import {NearbyPresenceBrowserProxy} from './nearby_presence_browser_proxy.js';
import {ActionValues, FeatureValues, PresenceDevice, SelectOption} from './types.js';

Polymer({
  is: 'cross-device-internals',

  _template: getTemplate(),

  behaviors: [
    WebUIListenerBehavior,
  ],

  /** @private {?NearbyPresenceBrowserProxy} */
  browserProxy_: null,

  properties: {
    /** @private {!Array<!PresenceDevice>} */
    npDiscoveredDevicesList_: {
      type: Array,
      value: [],
    },

    /** @private {!Array<!SelectOption>} */
    featuresList: {
      type: Array,
      value: [
        {name: 'Nearby Presence', value: FeatureValues.NP},
        {name: 'Nearby Share', value: FeatureValues.NS},
        {name: 'Nearby Connections', value: FeatureValues.NC},
        {name: 'Fast Pair', value: FeatureValues.FP},
      ],
    },

    /** @private {!Array<!SelectOption>} */
    nearbyPresenceActionList: {
      type: Array,
      value: [
        {name: 'Start Scan', value: ActionValues.STARTSCAN},
        {name: 'Stop Scan', value: ActionValues.STOPSCAN},
        {name: 'Sync Credentials', value: ActionValues.SYNCCREDENTIALS},
        {name: 'First time flow', value: ActionValues.FIRSTTIMEFLOW},
      ],
    },

    /** @private {!Array<!SelectOption>} */
    nearbyShareActionList: {
      type: Array,
      value: [],
    },

    /** @private {!Array<!SelectOption>} */
    nearbyConnectionsActionList: {
      type: Array,
      value: [],
    },


    /** @private {!Array<!SelectOption>} */
    fastPairActionList: {
      type: Array,
      value: [],
    },

    actionsSelectList: {
      type: Array,
      value: [],
    },

  },

  created() {
    this.browserProxy_ = NearbyPresenceBrowserProxy.getInstance();
  },

  /**
   * When the page is initialized, notify the C++ layer to allow JavaScript.
   * @override
   */
  attached() {
    this.browserProxy_.initialize();
    this.addWebUIListener(
        'presence-device-found', device => this.onPresenceDeviceFound_(device));
    this.addWebUIListener(
        'presence-device-changed',
        device => this.onPresenceDeviceChanged_(device));
    this.addWebUIListener(
        'presence-device-lost', device => this.onPresenceDeviceLost_(device));
    this.set('actionsSelectList', this.nearbyPresenceActionList);
  },

  onStartScanClicked() {
    this.browserProxy_.SendStartScan();
  },

  updateActionsSelect() {
    switch (this.$.actionGroup.value) {
      case FeatureValues.NP:
        this.set('actionsSelectList', this.nearbyPresenceActionList);
        break;
      case FeatureValues.NC:
        this.set('actionsSelectList', this.nearbyConnectionsActionList);
        break;
      case FeatureValues.NS:
        this.set('actionsSelectList', this.nearbyShareActionList);
        break;
      case FeatureValues.FP:
        this.set('actionsSelectList', this.fastPairActionList);
        break;
    }
  },

  perform_action() {
    switch (this.$.actionSelect.value) {
      case ActionValues.STARTSCAN:
        this.browserProxy_.SendStartScan();
        break;
      case ActionValues.STOPSCAN:
        this.browserProxy_.SendStopScan();
        break;
      case ActionValues.SYNCCREDENTIALS:
        this.browserProxy_.SendSyncCredentials();
        break;
      case ActionValues.FIRSTTIMEFLOW:
        this.browserProxy_.SendFirstTimeFlow();
        break;
      default:
        break;
    }
  },


  onStopScanClicked() {
    this.browserProxy_.SendStopScan();
  },

  onSyncCredentialsClicked() {
    this.browserProxy_.SendSyncCredentials();
  },

  onFirstTimeFlowClicked() {
    this.browserProxy_.SendFirstTimeFlow();
  },

  onPresenceDeviceFound_(device) {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    // If there is not a device with this endpoint_id currently in the devices
    // list, add it.
    if (!this.npDiscoveredDevicesList_.find(
            list_device => list_device.endpoint_id === endpointId)) {
      this.unshift('npDiscoveredDevicesList_', {
        'connectable': true,
        'type': type,
        'endpoint_id': endpointId,
        'actions': actions,
      });
    }
  },

  // TODO(b/277820435): Add and update device name for devices that have names
  // included.
  onPresenceDeviceChanged_(device) {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    const index = this.npDiscoveredDevicesList_.findIndex(
        list_device => list_device.endpoint_id === endpointId);

    // If a device was changed but we don't have a record of it being found,
    // add it to the array like onPresenceDeviceFound_().
    if (index === -1) {
      this.unshift('npDiscoveredDevicesList_', {
        'connectable': true,
        'type': type,
        'endpoint_id': endpointId,
        'actions': actions,
      });
      return;
    }

    this.npDiscoveredDevicesList_[index] = {
      'connectable': true,
      'type': type,
      'endpoint_id': endpointId,
      'actions': actions,
    };
  },

  onPresenceDeviceLost_(device) {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    const index = this.npDiscoveredDevicesList_.findIndex(
        list_device => list_device.endpoint_id === endpointId);

    // The device was not found in the list.
    if (index === -1) {
      return;
    }

    this.npDiscoveredDevicesList_[index] = {
      'connectable': false,
      'type': type,
      'endpoint_id': endpointId,
      'actions': actions,
    };
  },
});
