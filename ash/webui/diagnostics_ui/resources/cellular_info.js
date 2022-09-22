// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cellular_info.html.js';
import {getLockType, getSignalStrength} from './diagnostics_utils.js';
import {LockType, Network, RoamingState} from './network_health_provider.mojom-webui.js';

/**
 * @fileoverview
 * 'cellular-info' is responsible for displaying data points related
 * to a Cellular network.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CellularInfoElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class CellularInfoElement extends CellularInfoElementBase {
  static get is() {
    return 'cellular-info';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!Network} */
      network: {
        type: Object,
      },

    };
  }

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
  }

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
  }

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
  }

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
  }
}

customElements.define(CellularInfoElement.is, CellularInfoElement);
