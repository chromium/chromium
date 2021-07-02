// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNetworkConfigService} from './mojo_interface_provider.js';
import {NetworkConfigServiceInterface, RmadErrorCode, RmaState, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'onboarding-landing-page' is the main landing page for the shimless rma
 * process.
 */
export class OnboardingLandingPage extends PolymerElement {
  static get is() {
    return 'onboarding-landing-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {?NetworkConfigServiceInterface} */
      networkConfig_: {
        type: Object,
        value: null,
      },

      /** @private {boolean} */
      networkConnected_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.networkConfig_ = getNetworkConfigService();
    this.hasNetworkConnection_();
  }

  /** @private */
  hasNetworkConnection_() {
    const networkFilter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
    };

    this.networkConfig_.getNetworkStateList(networkFilter).then(res => {
      const connectedNetworks = res.result.filter(
          (network) =>
              (network.connectionState ===
               chromeos.networkConfig.mojom.ConnectionStateType.kOnline) &&
              [
                chromeos.networkConfig.mojom.NetworkType.kWiFi,
                chromeos.networkConfig.mojom.NetworkType.kEthernet,
                chromeos.networkConfig.mojom.NetworkType.kCellular,
              ].includes(network.type));

      this.networkConnected_ = connectedNetworks.length > 0;
    });
  }

  /** @return {!Promise<StateResult>} */
  onNextButtonClick() {
    if (this.networkConnected_) {
      // TODO(crbug.com/1218180): Replace with a state specific function e.g.
      // ProvisioningComplete()
      return Promise.resolve(
          {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk});
    } else {
      return Promise.resolve();
    }
  }
};

customElements.define(OnboardingLandingPage.is, OnboardingLandingPage);
