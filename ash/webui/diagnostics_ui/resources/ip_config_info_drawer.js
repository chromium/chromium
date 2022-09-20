// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_shared.css.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {getSubnetMaskFromRoutingPrefix} from './diagnostics_utils.js';
import {Network} from './network_health_provider.mojom-webui.js';
import {getTemplate} from './ip_config_info_drawer.html.js';

/**
 * @fileoverview
 * 'ip-config-info-drawer' displays standard IP related configuration data in a
 * collapsible drawer.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const IpConfigInfoDrawerElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class IpConfigInfoDrawerElement extends IpConfigInfoDrawerElementBase {
  static get is() {
    return 'ip-config-info-drawer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

    };
  }

  static get observers() {
    return ['getNameServersHeader_(network.ipConfig.nameServers)'];
  }


  /** @override */
  constructor() {
    super();

    this.browserProxy_ = DiagnosticsBrowserProxyImpl.getInstance();
  }

  /**
   * @protected
   * @return {string}
   */
  computeGateway_() {
    if (this.network.ipConfig && this.network.ipConfig.gateway) {
      return this.network.ipConfig.gateway;
    }
    return '';
  }

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
  }

  /**
   * @protected
   * @return {string}
   */
  computeSubnetMask_() {
    // Routing prefix should be [1,32] when set. 0 indicates an unset value.
    if (this.network.ipConfig && this.network.ipConfig.routingPrefix &&
        this.network.ipConfig.routingPrefix >= 0 &&
        this.network.ipConfig.routingPrefix <= 32) {
      return getSubnetMaskFromRoutingPrefix(
          this.network.ipConfig.routingPrefix);
    }
    return '';
  }

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
  }
}

customElements.define(IpConfigInfoDrawerElement.is, IpConfigInfoDrawerElement);
