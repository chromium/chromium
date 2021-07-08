// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './connectivity_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './network_card.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NetworkHealthProviderInterface, NetworkListObserverInterface, NetworkListObserverReceiver} from './diagnostics_types.js'
import {getNetworkHealthProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'network-list' is responsible for displaying Ethernet, Cellular,
 *  and WiFi networks.
 */
Polymer({
  is: 'network-list',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?NetworkHealthProviderInterface}
   */
  networkHealthProvider_: null,

  /**
   * Receiver responsible for observing active network guids.
   * @private {?NetworkListObserverReceiver}
   */
  networkListObserverReceiver_: null,

  properties: {
    /** @type {boolean} */
    isTestRunning: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /** @private {Array<?string>} */
    otherNetworkGuids_: {
      type: Array,
      value: () => [],
    },

    /** @private {string} */
    activeGuid_: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    isActive: {
      type: Boolean,
      value: true,
    },
  },

  /** @override */
  created() {
    this.networkHealthProvider_ = getNetworkHealthProvider();
    this.observeNetworkList_();
  },

  /** @override */
  detached() {
    this.networkListObserverReceiver_.$.close();
  },

  /** @private */
  observeNetworkList_() {
    // Calling observeNetworkList will trigger onNetworkListChanged.
    this.networkListObserverReceiver_ = new NetworkListObserverReceiver(
        /**
         * @type {!NetworkListObserverInterface}
         */
        (this));

    this.networkHealthProvider_.observeNetworkList(
        this.networkListObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Implements NetworkListObserver.onNetworkListChanged
   * @param {!Array<string>} networkGuids
   * @param {string} activeGuid
   */
  onNetworkListChanged(networkGuids, activeGuid) {
    // The connectivity-card is responsible for displaying the active network
    // so we need to filter out the activeGuid to avoid displaying a
    // a network-card for it.
    this.otherNetworkGuids_ = networkGuids.filter(guid => guid !== activeGuid);
    this.activeGuid_ = activeGuid;
  },

  /**
   * 'navigation-view-panel' is responsible for calling this function when
   * the active page changes.
   * @param {{isActive: boolean}} isActive
   * @public
   */
  onNavigationPageChanged({isActive}) {
    this.isActive = isActive;
  },

  /** @protected */
  getSettingsString_() {
    return this.i18nAdvanced('settingsLinkText');
  },
});
