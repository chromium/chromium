// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cellular_info.html.js';
import {getLockType, getSignalStrength} from './diagnostics_utils.js';
import {LockType, Network, RoamingState} from './network_health_provider.mojom-webui.js';

/**
 * @fileoverview
 * 'cellular-info' is responsible for displaying data points related
 * to a Cellular network.
 */

const CellularInfoElementBase = I18nMixin(PolymerElement);

export class CellularInfoElement extends CellularInfoElementBase {
  static get is(): 'cellular-info' {
    return 'cellular-info' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      network: {
        type: Object,
      },
    };
  }

  network: Network;

  /**
   * Get correct display text for known cellular network technology.
   */
  protected computeNetworkTechnologyText(): string {
    if (!this.network.typeProperties?.cellular) {
      return '';
    }

    const technology =
        this.network?.typeProperties?.cellular?.networkTechnology;
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
    }
    assertNotReached();
  }

  protected computeRoamingText(): string {
    if (!this.network?.typeProperties?.cellular) {
      return '';
    }

    if (!this.network?.typeProperties?.cellular?.roaming) {
      return this.i18n('networkRoamingOff');
    }

    const state = this.network?.typeProperties?.cellular?.roamingState;
    switch (state) {
      case RoamingState.kNone:
        return '';
      case RoamingState.kRoaming:
        return this.i18n('networkRoamingStateRoaming');
      case RoamingState.kHome:
        return this.i18n('networkRoamingStateHome');
    }
    assertNotReached();
  }

  protected computeSimLockedText(): string {
    if (!this.network?.typeProperties?.cellular) {
      return '';
    }

    const cellularProps = this.network.typeProperties.cellular;
    assert(cellularProps);
    const {simLocked, lockType} = cellularProps;
    return (simLocked && lockType !== LockType.kNone) ?
        this.i18n('networkSimLockedText', getLockType(lockType)) :
        this.i18n('networkSimUnlockedText');
  }

  protected computeSignalStrength(): string {
    if (this.network?.typeProperties?.cellular) {
      return getSignalStrength(
          this.network.typeProperties.cellular.signalStrength);
    }
    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CellularInfoElement.is]: CellularInfoElement;
  }
}

customElements.define(CellularInfoElement.is, CellularInfoElement);
