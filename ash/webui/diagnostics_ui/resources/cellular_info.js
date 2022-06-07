// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_shared_css.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockType, Network, RoamingState} from './diagnostics_types.js';
import {getLockType, getSignalStrength} from './diagnostics_utils.js';

/**
 * @fileoverview
 * 'cellular-info' is responsible for displaying data points related
 * to a Cellular network.
 */
Polymer({
  is: 'cellular-info',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!Network} */
    network: {
      type: Object,
    },
  },

  /**
   * Get correct display text for known cellular network technology.
   * @protected
   * @return {string}
   */
  computeNetworkTechnologyText_() {
    if (!this.network.typeProperties) {
      return '';
    }

    const technology = this.network.typeProperties.cellular.networkTechnology;
    switch (technology) {
      case 'CDMA1XRTT':
        return this.i18n('networkTechnologyCdma1xrttLabel');
      case 'EDGE':
        return this.i18n('networkTechnologyEdgeLabel');
      case 'EVDO':
        return this.i18n('networkTechnologyEvdoLabel');
      case 'GPRS':
        return this.i18n('networkTechnologyGprsLabel');
      case 'GSM':
        return this.i18n('networkTechnologyGsmLabel');
      case 'HSPA':
        return this.i18n('networkTechnologyHspaLabel');
      case 'HSPAPlus':
        return this.i18n('networkTechnologyHspaPlusLabel');
      case 'LTE':
        return this.i18n('networkTechnologyLteLabel');
      case 'LTEAdvanced':
        return this.i18n('networkTechnologyLteAdvancedLabel');
      case 'UMTS':
        return this.i18n('networkTechnologyUmtsLabel');
      default:
        assertNotReached();
        return '';
    }
  },

  /**
   * @protected
   * @return {string}
   */
  computeRoamingText_() {
    if (!this.network.typeProperties) {
      return '';
    }

    if (!this.network.typeProperties.cellular.roaming) {
      return this.i18n('networkRoamingOff');
    }

    const state = this.network.typeProperties.cellular.roamingState;
    switch (state) {
      case RoamingState.kNone:
        return '';
      case RoamingState.kRoaming:
        return this.i18n('networkRoamingStateRoaming');
      case RoamingState.kHome:
        return this.i18n('networkRoamingStateHome');
    }

    assertNotReached();
    return '';
  },

  /**
   * @protected
   * @return {string}
   */
  computeSimLockedText_() {
    if (!this.network.typeProperties) {
      return '';
    }

    const {simLocked, lockType} = this.network.typeProperties.cellular;
    return (simLocked && lockType !== LockType.kNone) ?
        this.i18n('networkSimLockedText', getLockType(lockType)) :
        this.i18n('networkSimUnlockedText');
  },

  /**
   * @protected
   * @return {string}
   */
  computeSignalStrength_() {
    if (this.network.typeProperties && this.network.typeProperties.cellular) {
      return getSignalStrength(
          this.network.typeProperties.cellular.signalStrength);
    }
    return '';
  },
});
