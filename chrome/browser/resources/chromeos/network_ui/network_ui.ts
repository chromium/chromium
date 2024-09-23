// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network_health/network_diagnostics.js';
import 'chrome://resources/ash/common/network_health/network_health_summary.js';
import 'chrome://resources/ash/common/traffic_counters/traffic_counters.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/ash/common/network/network_select.js';
import './strings.m.js';
import './network_state_ui.js';
import './network_logs_ui.js';
import './network_metrics_ui.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {NetworkList} from 'chrome://resources/ash/common/network/network_list_types.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {NetworkDiagnosticsElement} from 'chrome://resources/ash/common/network_health/network_diagnostics.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrosNetworkConfig, CrosNetworkConfigRemote, StartConnectResult} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_ui.html.js';
import {NetworkUiBrowserProxy, NetworkUiBrowserProxyImpl} from './network_ui_browser_proxy.js';

function stringifyJson(result: any): string {
  return JSON.stringify(result, null, '\t');
}

/**
 * @fileoverview
 * Polymer element network debugging UI.
 */

const NetworkUiElementBase = I18nMixin(PolymerElement);

class NetworkUiElement extends NetworkUiElementBase {
  static get is() {
    return 'network-ui' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Names of the top level page tabs.
       */
      tabNames_: {
        type: Array,
        computed: 'computeTabNames_(isWifiDirectEnabled_)',
      },

      /**
       * Index of the selected top level page tab.
       */
      selectedTab_: {
        type: Number,
        value: 0,
      },

      hostname_: {
        type: String,
        value: '',
      },

      tetheringConfigToSet_: {
        type: String,
        value: '',
      },

      isGuestModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isGuestModeActive') &&
              loadTimeData.getBoolean('isGuestModeActive');
        },
      },

      isWifiDirectEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isWifiDirectEnabled') &&
              loadTimeData.getBoolean('isWifiDirectEnabled');
        },
      },

      invalidJSON_: {
        type: Boolean,
        value: false,
      },

      showNetworkSelect_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private tabNames_: string[];
  private selectedTab_: number;
  private hostname_: string;
  private tetheringConfigToSet_: string;
  private isGuestModeActive_: boolean;
  private isWifiDirectEnabled_: boolean;
  private invalidJSON_: boolean;
  private showNetworkSelect_: boolean;
  private onHashChange_: () => void = () => {
    this.selectTabFromHash_();
  };

  private networkConfig_: CrosNetworkConfigRemote =
      CrosNetworkConfig.getRemote();

  private browserProxy_: NetworkUiBrowserProxy =
      NetworkUiBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.shadowRoot!.querySelector<HTMLInputElement>('#import-onc')!.value = '';

    this.requestGlobalPolicy_();
    this.getTetheringCapabilities_();
    this.getTetheringConfig_();
    this.getTetheringStatus_();

    if (this.isWifiDirectEnabled_) {
      this.getWifiDirectCapabilities_();
      this.getWifiDirectClientInfo_();
      this.getWifiDirectOwnerInfo_();
    }
    this.getHostname_();
    this.selectTabFromHash_();
    window.addEventListener('hashchange', this.onHashChange_);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    window.removeEventListener('hashchange', this.onHashChange_);
  }

  private computeTabNames_(): string[] {
    const values: string[] = [
      this.i18n('generalTab'),
      this.i18n('networkHealthTab'),
      this.i18n('networkLogsTab'),
      this.i18n('networkStateTab'),
      this.i18n('networkSelectTab'),
      this.i18n('TrafficCountersTrafficCounters'),
      this.i18n('networkMetricsTab'),
      this.i18n('networkHotspotTab'),
    ];
    if (this.isWifiDirectEnabled_) {
      values.push(this.i18n('networkWifiDirectTab'));
    }
    return values;
  }

  private selectTabFromHash_() {
    const selectedTab = window.location.hash.substring(1);
    if (!selectedTab) {
      return;
    }
    const tabpanels = this.shadowRoot!.querySelectorAll('iron-pages .tabpanel');
    for (let idx = 0; idx < tabpanels.length; ++idx) {
      if (tabpanels[idx].id === selectedTab) {
        this.selectedTab_ = idx;
      }
    }
  }

  private async openCellularActivationUi_() {
    const response = await this.browserProxy_.openCellularActivationUi();
    this.shadowRoot!.querySelector<HTMLElement>(
                        '#cellular-error-text')!.hidden = response[0];
  }

  private onResetEsimCacheClick_() {
    this.browserProxy_.resetEsimCache();
  }

  private onDisableActiveEsimProfileClick_() {
    this.browserProxy_.disableActiveEsimProfile();
  }

  private onResetEuiccClick_() {
    this.browserProxy_.resetEuicc();
  }

  private onResetApnMigratorClick_() {
    this.browserProxy_.resetApnMigrator();
  }

  private showAddNewWifi_() {
    this.browserProxy_.showAddNewWifi();
  }

  /**
   * Handles the ONC file input change event.
   */
  private onImportOncChange_(event: Event) {
    const target = event.target as HTMLInputElement;
    const file: File|null =
        (target.files && target.files.length > 0) ? target.files[0] : null;
    event.stopPropagation();
    if (!file) {
      return;
    }
    const reader = new FileReader();
    reader.onloadend = (_) => {
      const content = reader.result;
      if (!content || typeof (content) !== 'string') {
        console.error('File not read' + file);
        return;
      }
      this.browserProxy_.importOnc(content).then((response) => {
        this.importOncResponse_(response);
      });
    };
    reader.readAsText(file);
  }

  /**
   * Handles the chrome 'importOnc' response.
   */
  private importOncResponse_(args: [string, boolean]) {
    const resultDiv =
        this.shadowRoot!.querySelector<HTMLElement>('#onc-import-result');
    assert(resultDiv);
    resultDiv.innerText = args[0];
    resultDiv.classList.toggle('error', args[1]);
    this.shadowRoot!.querySelector<HTMLInputElement>('#import-onc')!.value = '';
  }

  /**
   * Requests the global policy dictionary and updates the page.
   */
  private async requestGlobalPolicy_() {
    const result = await this.networkConfig_.getGlobalPolicy();
    this.shadowRoot!.querySelector('#global-policy')!.textContent =
        stringifyJson(result.result);
  }

  private async getTetheringCapabilities_() {
    const result = await this.browserProxy_.getTetheringCapabilities();
    const div = this.shadowRoot!.querySelector('#tethering-capabilities-div');
    if (div) {
      div.textContent = stringifyJson(result);
    }
  }

  private async getTetheringStatus_() {
    const result = await this.browserProxy_.getTetheringStatus();
    const div = this.shadowRoot!.querySelector('#tethering-status-div');
    if (div) {
      div.textContent = stringifyJson(result);
    }
  }

  private async getTetheringConfig_() {
    const result = await this.browserProxy_.getTetheringConfig();
    const div = this.shadowRoot!.querySelector('#tethering-config-div');
    if (div) {
      div.textContent = stringifyJson(result);
    }
  }

  private async setTetheringConfig_() {
    const result =
        await this.browserProxy_.setTetheringConfig(this.tetheringConfigToSet_);
    const success = result === 'success';
    const resultDiv = this.shadowRoot!.querySelector<HTMLElement>(
        '#set-tethering-config-result');
    assert(resultDiv);
    resultDiv.innerText = result;
    resultDiv.classList.toggle('error', !success);
    if (success) {
      this.getTetheringConfig_();
    }
  }

  private async checkTetheringReadiness_() {
    const result = await this.browserProxy_.checkTetheringReadiness();
    const resultDiv = this.shadowRoot!.querySelector<HTMLElement>(
        '#check-tethering-readiness-result');
    assert(resultDiv);
    resultDiv.innerText = result;
    resultDiv.classList.toggle('error', result !== 'ready');
  }

  /**
   * Check if the input tethering config string is a valid JSON object.
   */
  private validateJson_() {
    if (this.tetheringConfigToSet_ === '') {
      this.invalidJSON_ = false;
      return;
    }

    try {
      const parsed = JSON.parse(this.tetheringConfigToSet_);
      // Check if the parsed JSON is object type by its constructor
      if (parsed.constructor === ({}).constructor) {
        this.invalidJSON_ = false;
        return;
      }
      this.invalidJSON_ = true;
    } catch (e) {
      this.invalidJSON_ = true;
    }
  }

  private async getWifiDirectCapabilities_() {
    const result = await this.browserProxy_.getWifiDirectCapabilities();
    const div = this.shadowRoot!.querySelector('#wifi-direct-capabilities-div');
    if (div) {
      div.textContent = stringifyJson(result);
    }
  }

  private async getWifiDirectOwnerInfo_() {
    const result = await this.browserProxy_.getWifiDirectOwnerInfo();
    const div = this.shadowRoot!.querySelector('#wifi-direct-owner-info-div');
    if (div) {
      div.textContent = stringifyJson(result);
    }
  }

  private async getWifiDirectClientInfo_() {
    const result = await this.browserProxy_.getWifiDirectClientInfo();
    const div = this.shadowRoot!.querySelector('#wifi-direct-client-info-div');
    if (div) {
      div.textContent = stringifyJson(result);
    }
  }

  private onHostnameChanged_(_: Event) {
    this.browserProxy_.setHostname(this.hostname_);
  }

  private getHostname_() {
    this.browserProxy_.getHostname().then(result => this.hostname_ = result);
  }

  /**
   * Handles clicks on network items in the <network-select> element by
   * attempting a connection to the selected network or requesting a password
   * if the network requires a password.
   */
  private onNetworkItemSelected_(
      event: CustomEvent<OncMojo.NetworkStateProperties>) {
    const networkState: OncMojo.NetworkStateProperties = event.detail;

    // If the network is already connected, show network details.
    if (OncMojo.connectionStateIsConnected(networkState.connectionState)) {
      this.browserProxy_.showNetworkDetails(networkState.guid);
      return;
    }

    // If the network is not connectable, show a configuration dialog.
    if (networkState.connectable === false || networkState.errorState) {
      this.browserProxy_.showNetworkConfig(networkState.guid);
      return;
    }

    // Otherwise, connect.
    this.networkConfig_.startConnect(networkState.guid).then(response => {
      if (response.result === StartConnectResult.kSuccess) {
        return;
      }
      console.error(
          'startConnect error for: ' + networkState.guid + ' Result: ' +
          response.result.toString() + ' Message: ' + response.message);
      this.browserProxy_.showNetworkConfig(networkState.guid);
    });
  }

  /**
   * Returns and typecasts the network diagnostics element
   */
  private getNetworkDiagnosticsElement_(): NetworkDiagnosticsElement {
    return this.shadowRoot!.querySelector('#network-diagnostics')!;
  }

  private renderNetworkSelect_() {
    this.showNetworkSelect_ = true;
    flush();

    const select = this.shadowRoot!.querySelector('network-select');
    assert(select);
    select.customItems = [
      {
        customItemName: 'addWiFiListItemName',
        polymerIcon: 'cr:add',
        customData: 'WiFi',
      },
    ];
  }

  /**
   * Handles requests to open the feedback report dialog. The provided string
   * in the event will be sent as a part of the feedback report.
   */
  private onSendFeedbackReportClick_(_: Event) {
    chrome.send('OpenFeedbackDialog');
  }

  /**
   * Handles requests to open the feedback report dialog. The provided string
   * in the event will be sent as a part of the feedback report.
   */
  private onRunAllRoutinesClick_(_: Event) {
    this.getNetworkDiagnosticsElement_().runAllRoutines();
  }

  private onCustomItemSelected_(event:
                                    CustomEvent<NetworkList.CustomItemState>) {
    this.browserProxy_.addNetwork(event.detail!.customData as string);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkUiElement.is]: NetworkUiElement;
  }
}

customElements.define(NetworkUiElement.is, NetworkUiElement);
