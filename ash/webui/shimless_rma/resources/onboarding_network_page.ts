// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './icons.html.js';
import './shimless_rma_shared.css.js';
import './strings.m.js';
import 'chrome://resources/ash/common/network/network_config.js';
import 'chrome://resources/ash/common/network/network_list.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {assert} from 'chrome://resources/js/assert.js';
import {NetworkConfigElement} from 'chrome://resources/ash/common/network/network_config.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrosNetworkConfigInterface as NetworkConfigServiceInterface, FilterType, NetworkStateProperties, NetworkFilter, NO_LIMIT, StartConnectResult} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNetworkConfigService, getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_network_page.html.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {enableNextButton, focusPageTitle} from './shimless_rma_util.js';
import {createCustomEvent, SetNextButtonLabelEvent, SET_NEXT_BUTTON_LABEL} from './events.js';

declare global {
  interface HTMLElementEventMap {
    [SET_NEXT_BUTTON_LABEL]: SetNextButtonLabelEvent;
  }
}

/**
 * @fileoverview
 * 'onboarding-network-page' is the page where the user can choose to join a
 * network.
 */

const OnboardingNetworkPageBase = I18nMixin(PolymerElement);

export class OnboardingNetworkPage extends OnboardingNetworkPageBase {
  static get is() {
    return 'onboarding-network-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       */
      allButtonsDisabled: Boolean,

      /**
       * Array of available networks
       */
      networks: {
        type: Array,
        value: [],
      },

      /**
       * Tracks whether network has configuration to be connected
       */
      enableConnect: {
        type: Boolean,
      },

      /**
       * The type of network to be configured as a string. May be set initially
       * or updated by network-config.
       */
      networkType: {
        type: String,
        value: '',
      },

      /**
       * WARNING: This string may contain malicious HTML and should not be used
       * for Polymer bindings in CSS code. For additional information see
       * b/286254915.
       *
       * The name of the network. May be set initially or updated by
       * network-config.
       */
      networkName: {
        type: String,
        value: '',
      },

      /**
       * The GUID when an existing network is being configured. This will be
       * empty when configuring a new network.
       */
      guid: {
        type: String,
        value: '',
      },

      /**
       * Tracks whether network shows connect button or disconnect button.
       */
      networkShowConnect: {
        type: Boolean,
      },

      /**
       * Set by network-config when a configuration error occurs.
       */
      error: {
        type: String,
        value: '',
      },

      /**
       * Set to true to when connected to at least one active network.
       */
      isOnline: {
        type: Boolean,
        value: false,
        observer: OnboardingNetworkPage.prototype.onIsOnlineChange,
      },
    };
  }

  allButtonsDisabled: boolean;
  protected networkName: string;
  protected networkType: string;
  protected guid: string;
  protected enableConnect: boolean;
  protected networkShowConnect: boolean;
  protected networks: NetworkStateProperties[];
  protected isOnline: boolean;
  private error: string;
  private shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  private networkConfig: NetworkConfigServiceInterface = getNetworkConfigService();

  override ready() {
    super.ready();

    // Before displaying the available networks, track the pre-existing
    // configured networks.
    this.shimlessRmaService.trackConfiguredNetworks();
    this.refreshNetworks();
    enableNextButton(this);

    focusPageTitle(this);
  }

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged(): void {
    this.refreshNetworks();
  }

  async refreshNetworks(): Promise<void> {
    const networkFilter: NetworkFilter = {
      filter: FilterType.kVisible,
      networkType: NetworkType.kAll,
      limit: NO_LIMIT,
    };
    const response = await this.networkConfig.getNetworkStateList(networkFilter);
    const networkIsWiFiOrEthernet = (n: NetworkStateProperties) => [NetworkType.kWiFi, NetworkType.kEthernet].includes(n.type);
    this.networks = response.result.filter(networkIsWiFiOrEthernet);
    this.isOnline = this.networks.some(n => OncMojo.connectionStateIsConnected(n.connectionState));
  }

  /**
   * Event triggered when a network list item is selected.
   */
  protected onNetworkSelected(event: CustomEvent<OncMojo.NetworkStateProperties>): void {
    const networkState = event.detail;
    const type = networkState.type;
    const displayName = OncMojo.getNetworkStateDisplayNameUnsafe(networkState);

    this.networkShowConnect =
        (networkState.connectionState === ConnectionStateType.kNotConnected);

    if (!this.canAttemptConnection(networkState)) {
      this.showConfig(type, networkState.guid, displayName);
      return;
    }

    this.networkConfig.startConnect(networkState.guid).then((response: {result: StartConnectResult, message: string}) => {
      this.refreshNetworks();
      if (response.result === StartConnectResult.kUnknown) {
        console.error(
            'startConnect failed for: ' + networkState.guid +
            ' Error: ' + response.message);
        return;
      }
    });
  }

  /**
   * Determines whether or not it is possible to attempt a connection to the
   * provided network (e.g., whether it's possible to connect or configure the
   * network for connection).
   */
  private canAttemptConnection(state: OncMojo.NetworkStateProperties): boolean {
    if (state.connectionState !== ConnectionStateType.kNotConnected) {
      return false;
    }

    if (OncMojo.networkTypeHasConfigurationFlow(state.type) &&
        (!OncMojo.isNetworkConnectable(state) || !!state.errorState)) {
      return false;
    }

    return true;
  }

  private showConfig(type: NetworkType, guid: string, name: string): void {
    assert(type !== NetworkType.kCellular && type !== NetworkType.kTether);

    this.networkType = OncMojo.getNetworkTypeString(type);
    this.networkName = name || '';
    this.guid = guid || '';

    const networkConfig: NetworkConfigElement|null = this.shadowRoot!.querySelector('#networkConfig');
    assert(networkConfig);
    networkConfig.init();

    const dialog: CrDialogElement|null = this.shadowRoot!.querySelector('#dialog');
    assert(dialog);
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  protected closeConfig(): void {
    const dialog: CrDialogElement|null = this.shadowRoot!.querySelector('#dialog');
    assert(dialog);
    if (dialog.open) {
      dialog.close();
    }

    // Reset the network state properties.
    this.networkType = '';
    this.networkName = '';
    this.guid = '';
  }

  protected connectNetwork(): void {
    const networkConfig: NetworkConfigElement|null = this.shadowRoot!.querySelector('#networkConfig');
    assert(networkConfig);
    networkConfig.connect();
  }

  protected disconnectNetwork(): void {
    this.networkConfig.startDisconnect(this.guid).then(response => {
      if (!response.success) {
        console.error('Disconnect failed for: ' + this.guid);
      }
    });
    this.closeConfig();
  }

  private getError(): string {
    if (this.i18nExists(this.error)) {
      return this.i18n(this.error);
    }
    return this.i18n('networkErrorUnknown');
  }

  protected onPropertiesSet(): void {
    this.refreshNetworks();
  }

  private onConfigClose(): void {
    this.closeConfig();
    this.refreshNetworks();
  }

  protected getDialogTitle(): string {
    if (this.networkName && !this.networkShowConnect) {
      return loadTimeData.getStringF('internetConfigName', this.networkName);
    }
    const type = this.i18n('OncType' + this.networkType);
    return this.i18n('internetJoinType', type);
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    return this.shimlessRmaService.networkSelectionComplete();
  }

  private onIsOnlineChange(): void {
    this.dispatchEvent(createCustomEvent(SET_NEXT_BUTTON_LABEL,
      this.isOnline ? 'nextButtonLabel' : 'skipButtonLabel'));
 }

 showConfigForTesting(networkType: NetworkType, guid: string, name: string):
     void {
   this.showConfig(networkType, guid, name);
 }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingNetworkPage.is]: OnboardingNetworkPage;
  }
}

customElements.define(OnboardingNetworkPage.is, OnboardingNetworkPage);
