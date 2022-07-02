// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './connectivity_card.js';
import './diagnostics_shared_css.js';
import './icons.js';
import './network_card.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {NetworkHealthProviderInterface, NetworkListObserverInterface, NetworkListObserverReceiver} from './diagnostics_types.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';
import {TestSuiteStatus} from './routine_list_executor.js';

/**
 * @fileoverview
 * 'network-list' is responsible for displaying Ethernet, Cellular,
 *  and WiFi networks.
 */
Polymer({
  is: 'network-list',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private {?DiagnosticsBrowserProxy} */
  browserProxy_: null,

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
    /** @type {!TestSuiteStatus} */
    testSuiteStatus: {
      type: Number,
      value: TestSuiteStatus.kNotRunning,
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

    /** @protected {boolean} */
    isLoggedIn_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isLoggedIn'),
    },
  },

  /** @override */
  created() {
    this.browserProxy_ = DiagnosticsBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize();
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
    // TODO(ashleydp): Update when connectivity/network card's are merged.
    if (isActive) {
      // Focus the first visible card title. If no cards are present,
      // fallback to focusing the element's main container.
      afterNextRender(this, () => {
        if (this.activeGuid_) {
          this.$$('connectivity-card').$$('#cardTitle').focus();
          return;
        } else if (this.otherNetworkGuids_.length > 0) {
          this.$$('network-card').$$('#cardTitle').focus();
          return;
        }
        this.$.networkListContainer.focus();
      });
      // TODO(ashleydp): Remove when a call can be made at a higher component
      // to avoid duplicate code in all navigatable pages.
      this.browserProxy_.recordNavigation('connectivity');
    }
  },

  /** @protected */
  getSettingsString_() {
    return this.i18nAdvanced('settingsLinkText');
  },
});
