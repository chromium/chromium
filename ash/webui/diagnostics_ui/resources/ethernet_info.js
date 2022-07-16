// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthenticationType, Network} from './diagnostics_types.js';


/**
 * @fileoverview
 * 'ethernet-info' is responsible for displaying data points related
 * to an Ethernet network.
 */
Polymer({
  is: 'ethernet-info',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
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
  },

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
  },

  /**
   * @protected
   * @return {string}
   */
  computeIpAddress_() {
    if (this.network.ipConfig && this.network.ipConfig.ipAddress) {
      return this.network.ipConfig.ipAddress;
    }
    return '';
  },
});
