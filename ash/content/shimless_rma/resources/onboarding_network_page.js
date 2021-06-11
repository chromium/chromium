// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared_css.js';
import './strings.m.js';
import 'chrome://resources/cr_components/chromeos/network/network_list.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNetworkConfigService} from './mojo_interface_provider.js';
import {NetworkConfigServiceRemote} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'onboarding-network-page' is the page where the user can choose to join a
 * network.
 */
export class OnboardingNetworkPage extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'onboarding-network-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {?NetworkConfigServiceRemote} */
      networkConfig_: {
        type: Object,
        value: null,
      },

      /**
       * Array of available networks
       * @protected
       * @type {!Array<string>}
       */
      networks_: {
        type: Array,
        value: [],
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.networkConfig_ = getNetworkConfigService();
    this.refreshNetworks();
    // TODO(joonbug): Set interval continuously refresh networks
  }

  refreshNetworks() {
    const networkFilter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
    };
    this.networkConfig_.getNetworkStateList(networkFilter).then(res => {
      this.networks_ = res.result;
    });
  }

  onNetworkSelected_(event) {
    return;
  }
};

customElements.define(OnboardingNetworkPage.is, OnboardingNetworkPage);
