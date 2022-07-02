// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_shared_css.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {Network} from './diagnostics_types.js';
import {getSubnetMaskFromRoutingPrefix} from './diagnostics_utils.js';

/**
 * @fileoverview
 * 'ip-config-info-drawer' displays standard IP related configuration data in a
 * collapsible drawer.
 */
Polymer({
  is: 'ip-config-info-drawer',

  _template: html`{__html_template__}`,

  /**  @private {?DiagnosticsBrowserProxy} */
  browserProxy_: null,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @protected
     * @type {boolean}
     */
    expanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * @protected
     * @type {string}
     */
    gateway_: {
      type: String,
      computed: 'computeGateway_(network.ipConfig.gateway)',
    },

    /**
     * @protected
     * @type {string}
     */
    nameServers_: {
      type: String,
      computed: 'computeNameServers_(network.ipConfig.nameServers)',
    },

    /** @type {!Network} */
    network: {
      type: Object,
    },

    /**
     * @protected
     * @type {string}
     */
    subnetMask_: {
      type: String,
      computed: 'computeSubnetMask_(network.ipConfig.routingPrefix)',
    },

    /** @protected {string} */
    nameServersHeader_: {
      type: String,
      value: '',
    },
  },

  observers: ['getNameServersHeader_(network.ipConfig.nameServers)'],

  /** @override */
  created() {
    this.browserProxy_ = DiagnosticsBrowserProxyImpl.getInstance();
  },

  /**
   * @protected
   * @return {string}
   */
  computeGateway_() {
    if (this.network.ipConfig && this.network.ipConfig.gateway) {
      return this.network.ipConfig.gateway;
    }
    return '';
  },

  /**
   * @protected
   * @return {string}
   */
  computeNameServers_() {
    if (!this.network.ipConfig) {
      return '';
    }

    // Handle name servers null or zero length state.
    if (!this.network.ipConfig.nameServers ||
        this.network.ipConfig.nameServers.length === 0) {
      return loadTimeData.getStringF('networkDnsNotConfigured');
    }

    return this.network.ipConfig.nameServers.join(', ');
  },

  /**
   * @protected
   * @return {string}
   */
  computeSubnetMask_() {
    if (this.network.ipConfig && this.network.ipConfig.routingPrefix) {
      return getSubnetMaskFromRoutingPrefix(
          this.network.ipConfig.routingPrefix);
    }
    return '';
  },

  /**
   * @protected
   * @param {?Array<string>} nameServers
   */
  getNameServersHeader_(nameServers) {
    const count = nameServers ? nameServers.length : 0;
    this.browserProxy_.getPluralString('nameServersText', count)
        .then(localizedString => {
          this.nameServersHeader_ = localizedString;
        });
  },
});
