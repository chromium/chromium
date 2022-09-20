// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthenticationType, Network} from './network_health_provider.mojom-webui.js';
import {getTemplate} from './ethernet_info.html.js';

/**
 * @fileoverview
 * 'ethernet-info' is responsible for displaying data points related
 * to an Ethernet network.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const EthernetInfoElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class EthernetInfoElement extends EthernetInfoElementBase {
  static get is() {
    return 'ethernet-info';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * @protected
       * @type {string}
       */
      authentication_: {
        type: String,
        computed: 'computeAuthentication_(network.typeProperties.ethernet.' +
            'authentication)',
      },

      /**
       * @protected
       * @type {string}
       */
      ipAddress_: {
        type: String,
        computed: 'computeIpAddress_(network.ipConfig.ipAddress)',
      },

      /** @type {!Network} */
      network: {
        type: Object,
      },

    };
  }

  /**
   * @protected
   * @return {string}
   */
  computeAuthentication_() {
    if (this.network.typeProperties) {
      /** @type {!AuthenticationType} */
      const authentication =
          this.network.typeProperties.ethernet.authentication;
      switch (authentication) {
        case AuthenticationType.kNone:
          return this.i18n('networkEthernetAuthenticationNoneLabel');
        case AuthenticationType.k8021x:
          return this.i18n('networkEthernetAuthentication8021xLabel');
        default:
          assertNotReached();
          return '';
      }
    }
    return '';
  }

  /**
   * @protected
   * @return {string}
   */
  computeIpAddress_() {
    if (this.network.ipConfig && this.network.ipConfig.ipAddress) {
      return this.network.ipConfig.ipAddress;
    }
    return '';
  }
}

customElements.define(EthernetInfoElement.is, EthernetInfoElement);
