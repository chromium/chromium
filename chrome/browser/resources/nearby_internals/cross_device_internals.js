// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_style.css.js';
import './np_list_object.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cross_device_internals.html.js';
import {NearbyPresenceBrowserProxy} from './nearby_presence_browser_proxy.js';
import {PresenceDevice} from './types.js';

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
  },

  onStartScanClicked() {
    this.browserProxy_.SendStartScan();
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

    // If there is not a device with this endpoint_id currently in the devices
    // list, add it.
    if (!this.npDiscoveredDevicesList_.find(
            list_device => list_device.endpoint_id === endpointId)) {
      this.unshift('npDiscoveredDevicesList_', {
        'connectable': true,
        'type': type,
        'endpoint_id': endpointId,
      });
    }
  },

  // TODO(b/277820435): Add and update device name for devices that have names
  // included.
  onPresenceDeviceChanged_(device) {
    const type = device['type'];
    const endpointId = device['endpoint_id'];

    const index = this.npDiscoveredDevicesList_.findIndex(
        list_device => list_device.endpoint_id === endpointId);

    // If a device was changed but we don't have a record of it being found,
    // add it to the array like onPresenceDeviceFound_().
    if (index === -1) {
      this.unshift('npDiscoveredDevicesList_', {
        'connectable': true,
        'type': type,
        'endpoint_id': endpointId,
      });
      return;
    }

    this.npDiscoveredDevicesList_[index] = {
      'connectable': true,
      'type': type,
      'endpoint_id': endpointId,
    };
  },

  onPresenceDeviceLost_(device) {
    const type = device['type'];
    const endpointId = device['endpoint_id'];

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
    };
  },
});
