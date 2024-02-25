// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping network-select for login/oobe.
 */

import '//resources/ash/common/network/network_select.js';
import './oobe_network_icons.html.js';

import {NetworkList} from '//resources/ash/common/network/network_list_types.js';
import {OncMojo} from '//resources/ash/common/network/onc_mojo.js';
import {assert} from '//resources/js/assert.js';
import {StartConnectResult} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkSelectElement} from 'chrome://resources/ash/common/network/network_select.js';

import {Oobe} from '../cr_ui.js';

import {getTemplate} from './network_select_login.html.js';

interface NetworkCustomItemCustomData {
  onTap(): void;
}

interface NetworkCustomItem {
  customItemType: NetworkList.CustomItemType;
  customItemName: string;
  polymerIcon: string;
  showBeforeNetworksList: boolean;
  customData: NetworkCustomItemCustomData;
}

export class NetworkSelectLogin extends PolymerElement {
  static get is() {
    return 'network-select-login' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * True when connected to a network.
       */
      isNetworkConnected: {
        type: Boolean,
        notify: true,
        value: false,
      },

      /**
       * True when quick start is enabled.
       */
      isQuickStartVisible: {
        type: Boolean,
        value: false,
      },

      /**
       * If true, when a connected network is selected the configure UI will be
       * requested instead of sending 'userActed' + 'continue'.
       */
      configureConnected: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether this element should trigger periodic Wi-Fi scans to update the
       * list of networks. If true, a background scan is performed every 10
       * seconds.
       */
      enableWifiScans: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether to show technology badge on mobile network icons.
       */
      showTechnologyBadge: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isNetworkConnected: boolean;
  private isQuickStartVisible: boolean;
  private configureConnected: boolean;
  enableWifiScans: boolean;
  private showTechnologyBadge: boolean;

  /**
   * GUID of the user-selected network. It is remembered after user taps on
   * network entry. After we receive event "connected" on this network,
   * OOBE will proceed.
   */
  private networkLastSelectedGuid: string;

  /**
   * Flag that ensures that OOBE configuration is applied only once.
   */
  private configurationApplied: boolean;

  /**
   * Flag that reflects if this element is currently shown.
   */
  private isShown: boolean;

  constructor() {
    super();

    this.networkLastSelectedGuid = '';
    this.configurationApplied = false;
    this.isShown = false;
  }

  private isNetworkSelectElement(obj: any): obj is NetworkSelectElement {
    return typeof obj.refreshNetworks === 'function' &&
        typeof obj.focus === 'function' &&
        typeof obj.getDefaultNetwork === 'function' &&
        typeof obj.getNetwork === 'function' &&
        typeof obj.getNetworkListItemByNameForTest === 'function';
  }

  private getNetworkSelect(): NetworkSelectElement {
    const networkSelect =
        this.shadowRoot?.querySelector<NetworkSelectElement>('#networkSelect');
    // TODO: replace with instanceof and remove the function
    // once network_select.js has been migrated to TS (b/322154192)
    assert(this.isNetworkSelectElement(networkSelect));
    return networkSelect;
  }

  /** Refreshes the list of the networks. */
  refresh(): void {
    this.getNetworkSelect().refreshNetworks();
    this.networkLastSelectedGuid = '';
  }

  override focus(): void {
    this.getNetworkSelect().focus();
  }

  /** Called when dialog is shown. */
  onBeforeShow(): void {
    this.isShown = true;
    this.attemptApplyConfiguration();
  }

  /** Called when dialog is hidden. */
  onBeforeHide(): void {
    this.isShown = false;
  }

  /**
   * Returns custom items for network selector. Shows 'Proxy settings' only
   * when connected to a network.
   */
  private getNetworkCustomItems(): NetworkCustomItem[] {
    const items: NetworkCustomItem[] = [];
    if (this.isQuickStartVisible) {
      items.push({
        customItemType: NetworkList.CustomItemType.OOBE,
        customItemName: 'networkScreenQuickStart',
        polymerIcon: 'oobe-20:quick-start-android-device',
        showBeforeNetworksList: true,
        customData: {
          onTap: () => this.quickStartClicked(),
        },
      });
    }
    if (this.isNetworkConnected) {
      items.push({
        customItemType: NetworkList.CustomItemType.OOBE,
        customItemName: 'proxySettingsListItemName',
        polymerIcon: 'oobe-network-20:add-proxy',
        showBeforeNetworksList: false,
        customData: {
          onTap: () => this.openInternetDetailDialog(),
        },
      });
    }
    items.push({
      customItemType: NetworkList.CustomItemType.OOBE,
      customItemName: 'addWiFiListItemName',
      polymerIcon: 'oobe-network-20:add-wifi',
      showBeforeNetworksList: false,
      customData: {
        onTap: () => this.openAddWiFiNetworkDialog(),
      },
    });
    return items;
  }

  /**
   * Handle Network Setup screen "Quick Setup" button.
   *
   */
  private quickStartClicked(): void {
    this.dispatchEvent(new CustomEvent(
        'quick-start-clicked', {bubbles: true, composed: true}));
  }

  /**
   * Handle Network Setup screen "Proxy settings" button.
   *
   */
  private openInternetDetailDialog(): void {
    chrome.send('launchInternetDetailDialog');
  }

  /**
   * Handle Network Setup screen "Add WiFi network" button.
   *
   */
  private openAddWiFiNetworkDialog(): void {
    chrome.send('launchAddWiFiNetworkDialog');
  }

  /**
   * Called when network setup is done. Notifies parent that network setup is
   * done.
   */
  private onSelectedNetworkConnected(): void {
    this.networkLastSelectedGuid = '';
    this.dispatchEvent(new CustomEvent(
        'selected-network-connected', {bubbles: true, composed: true}));
  }

  /**
   * Event triggered when the default network state may have changed.
   */
  private onDefaultNetworkChanged(
      event: CustomEvent<OncMojo.NetworkStateProperties|undefined>): void {
    // Note: event.detail will be |undefined| if there is no default network.
    const networkState = event.detail?.type ? event.detail : undefined;
    this.isNetworkConnected = !!networkState &&
        OncMojo.connectionStateIsConnected(networkState.connectionState);
    if (!this.isNetworkConnected || !this.isShown) {
      return;
    }
    this.attemptApplyConfiguration();
  }

  /**
   * Event triggered when a network-list-item connection state changes.
   */
  private onNetworkConnectChanged(
      event: CustomEvent<OncMojo.NetworkStateProperties>): void {
    const networkState = event.detail;
    if (networkState && networkState.guid === this.networkLastSelectedGuid &&
        OncMojo.connectionStateIsConnected(networkState.connectionState)) {
      this.onSelectedNetworkConnected();
    }
  }

  /**
   * Event triggered when a list of networks get changed.
   */
  private onNetworkListChanged(
      _event: CustomEvent<OncMojo.NetworkStateProperties[]>): void {
    if (!this.isShown) {
      return;
    }
    this.attemptApplyConfiguration();
  }

  /**
   * Tries to apply OOBE configuration on current list of networks.
   */
  private attemptApplyConfiguration(): void {
    if (this.configurationApplied) {
      return;
    }
    const configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration) {
      return;
    }
    const defaultNetwork = this.getNetworkSelect().getDefaultNetwork();
    if (configuration.networkUseConnected && defaultNetwork &&
        OncMojo.connectionStateIsConnected(defaultNetwork.connectionState)) {
      window.setTimeout(() => this.handleNetworkSelection(defaultNetwork), 0);
      this.configurationApplied = true;
      return;
    }
    if (configuration.networkSelectGuid) {
      const network =
          this.getNetworkSelect().getNetwork(configuration.networkSelectGuid);
      if (network) {
        window.setTimeout(() => this.handleNetworkSelection(network), 0);
        this.configurationApplied = true;
        return;
      }
    }
  }

  /**
   * This is called when user taps on network entry in networks list.
   */
  private onNetworkListNetworkItemSelected(
      event: CustomEvent<OncMojo.NetworkStateProperties>): void {
    this.handleNetworkSelection(event.detail);
  }

  /**
   * Handles selection of particular network.
   */
  private handleNetworkSelection(networkState: OncMojo.NetworkStateProperties):
      void {
    assert(networkState);

    const isNetworkConnected =
        OncMojo.connectionStateIsConnected(networkState.connectionState);

    // If |configureConnected| is false and a connected network is selected,
    // continue to the next screen.
    if (!this.configureConnected && isNetworkConnected) {
      this.onSelectedNetworkConnected();
      return;
    }

    // If user has previously selected another network, there
    // is pending connection attempt. So even if new selection is currently
    // connected, it may get disconnected at any time.
    // So just send one more connection request to cancel current attempts.
    this.networkLastSelectedGuid = networkState.guid;

    const oncType = OncMojo.getNetworkTypeString(networkState.type);
    const guid = networkState.guid;

    let shouldShowNetworkDetails = isNetworkConnected ||
        networkState.connectionState === ConnectionStateType.kConnecting;
    // Cellular should normally auto connect. If it is selected, show the
    // details UI since there is no configuration UI for Cellular.
    shouldShowNetworkDetails ||= networkState.type === NetworkType.kCellular;

    if (shouldShowNetworkDetails) {
      chrome.send('showNetworkDetails', [oncType, guid]);
      return;
    }

    if (!networkState.connectable || networkState.errorState) {
      chrome.send('showNetworkConfig', [guid]);
      return;
    }

    const networkConfig =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();

    networkConfig.startConnect(guid).then(response => {
      switch (response.result) {
        case StartConnectResult.kSuccess:
          return;
        case StartConnectResult.kInvalidGuid:
        case StartConnectResult.kInvalidState:
        case StartConnectResult.kCanceled:
          // TODO(stevenjb/khorimoto): Consider handling these cases.
          return;
        case StartConnectResult.kNotConfigured:
          if (!OncMojo.networkTypeIsMobile(networkState.type)) {
            chrome.send('showNetworkConfig', [guid]);
          } else {
            console.error('Cellular network is not configured: ' + guid);
          }
          return;
        case StartConnectResult.kBlocked:
        case StartConnectResult.kUnknown:
          console.error(
              'startConnect failed for: ' + guid + ': ' + response.message);
          return;
      }
    });
  }

  /**
   */
  private onNetworkListCustomItemSelected(
      event: CustomEvent<{customData: NetworkCustomItemCustomData}>): void {
    const itemState = event.detail;
    itemState.customData.onTap();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkSelectLogin.is]: NetworkSelectLogin;
  }
}

customElements.define(NetworkSelectLogin.is, NetworkSelectLogin);
