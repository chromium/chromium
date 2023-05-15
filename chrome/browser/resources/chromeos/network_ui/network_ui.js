// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_select.js';
import 'chrome://resources/ash/common/network_health/network_diagnostics.js';
import 'chrome://resources/ash/common/network_health/network_health_summary.js';
import 'chrome://resources/ash/common/traffic_counters/traffic_counters.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './strings.m.js';
import './network_state_ui.js';
import './network_logs_ui.js';
import './network_metrics_ui.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfig, CrosNetworkConfigRemote, StartConnectResult} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_ui.html.js';
import {NetworkUIBrowserProxy, NetworkUIBrowserProxyImpl} from './network_ui_browser_proxy.js';

/**
 * @fileoverview
 * Polymer element network debugging UI.
 */

Polymer({
  is: 'network-ui',

  _template: getTemplate(),

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Names of the top level page tabs.
     * @type {!Array<String>}
     * @private
     */
    tabNames_: {
      type: Array,
      value: function() {
        const values = [
          this.i18n('generalTab'),
          this.i18n('networkHealthTab'),
          this.i18n('networkLogsTab'),
          this.i18n('networkStateTab'),
          this.i18n('networkSelectTab'),
          this.i18n('TrafficCountersTrafficCounters'),
          this.i18n('networkMetricsTab'),
        ];
        if (loadTimeData.valueExists('isHotspotEnabled') &&
            loadTimeData.getBoolean('isHotspotEnabled')) {
          values.push(this.i18n('networkHotspotTab'));
        }
        return values;
      },
    },

    /**
     * Index of the selected top level page tab.
     * @private
     */
    selectedTab_: {
      type: Number,
      value: 0,
    },

    /** @private */
    hostname_: {
      type: String,
      value: '',
    },

    /** @private */
    tetheringConfigToSet_: {
      type: String,
      value: '',
    },

    /** @private */
    isGuestModeActive_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('isGuestModeActive') &&
            loadTimeData.getBoolean('isGuestModeActive');
      },
    },

    /**@private */
    isTetheringEnabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * Set to true while an tethering state change is requested and the
     * callback hasn't been fired yet.
     * @private
     */
    tetheringChangeInProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    invalidJSON_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showNetworkSelect_: {
      type: Boolean,
      value: false,
    },

  },

  /** @type {?CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @type {!NetworkUIBrowserProxy} */
  browserProxy_: NetworkUIBrowserProxyImpl.getInstance(),

  /** @override */
  attached() {
    this.networkConfig_ = CrosNetworkConfig.getRemote();

    this.$$('#import-onc').value = '';

    this.requestGlobalPolicy_();
    this.getTetheringCapabilities_();
    this.getTetheringConfig_();
    this.getTetheringStatus_();
    this.getHostname_();
    this.selectTabFromHash_();
    window.addEventListener('hashchange', () => {
      this.selectTabFromHash_();
    });
  },

  /**
   * @param {*} result that need to stringify to JSON
   * @return {string}
   * @private
   */
  stringifyJSON_(result) {
    return JSON.stringify(result, null, '\t');
  },

  /** @private */
  selectTabFromHash_() {
    const selectedTab = window.location.hash.substring(1);
    if (!selectedTab) {
      return;
    }
    const tabpanels = this.$$('iron-pages').querySelectorAll('.tabpanel');
    for (let idx = 0; idx < tabpanels.length; ++idx) {
      if (tabpanels[idx].id == selectedTab) {
        this.selectedTab_ = idx;
      }
    }
  },

  /** @private */
  openCellularActivationUi_() {
    this.browserProxy_.openCellularActivationUi().then((response) => {
      const didOpenActivationUi = response.shift();
      this.$$('#cellular-error-text').hidden = didOpenActivationUi;
    });
  },

  /** @private */
  onResetESimCacheClick_() {
    this.browserProxy_.resetESimCache();
  },

  /** @private */
  onDisableActiveESimProfileClick_() {
    this.browserProxy_.disableActiveESimProfile();
  },

  /** @private */
  onResetEuiccClick_() {
    this.browserProxy_.resetEuicc();
  },

  /** @private */
  onResetApnMigratorClick_() {
    this.browserProxy_.resetApnMigrator();
  },

  /** @private */
  showAddNewWifi_() {
    this.browserProxy_.showAddNewWifi();
  },

  /**
   * Handles the ONC file input change event.
   * @param {!Event} event
   * @private
   */
  onImportOncChange_(event) {
    const file = event.target.files[0];
    event.stopPropagation();
    if (!file) {
      return;
    }
    const reader = new FileReader();
    reader.onloadend = (e) => {
      const content = reader.result;
      if (!content || typeof (content) != 'string') {
        console.error('File not read' + file);
        return;
      }
      this.browserProxy_.importONC(content).then((response) => {
        this.importONCResponse_(response);
      });
    };
    reader.readAsText(file);
  },

  /**
   * Handles the chrome 'importONC' response.
   * @param {Array} args
   * @private
   */
  importONCResponse_(args) {
    const result = args.shift();
    const isError = args.shift();
    const resultDiv = this.$$('#onc-import-result');
    resultDiv.innerText = result;
    resultDiv.classList.toggle('error', isError);
    this.$$('#import-onc').value = '';
  },

  /**
   * Requests the global policy dictionary and updates the page.
   * @private
   */
  requestGlobalPolicy_() {
    this.networkConfig_.getGlobalPolicy().then(result => {
      this.$$('#global-policy').textContent =
          this.stringifyJSON_(result.result);
    });
  },

  /** @private */
  getTetheringCapabilities_() {
    this.browserProxy_.getTetheringCapabilities().then(result => {
      this.$$('#tethering-capabilities-div').textContent =
          this.stringifyJSON_(result);
    });
  },

  /** @private */
  getTetheringStatus_() {
    this.browserProxy_.getTetheringStatus().then(result => {
      this.$$('#tethering-status-div').textContent =
          this.stringifyJSON_(result);
      const state = result['state'];
      const startingState = loadTimeData.getString('tetheringStateStarting');
      const activeState = loadTimeData.getString('tetheringStateActive');
      if (!!state && (state === startingState || state === activeState)) {
        this.isTetheringEnabled_ = true;
        return;
      }
      this.isTetheringEnabled_ = false;
    });
  },

  /** @private */
  getTetheringConfig_() {
    this.browserProxy_.getTetheringConfig().then(result => {
      this.$$('#tethering-config-div').textContent =
          this.stringifyJSON_(result);
    });
  },

  /** @private */
  setTetheringConfig_() {
    this.browserProxy_.setTetheringConfig(this.tetheringConfigToSet_)
        .then((result) => {
          const success = result === 'success';
          const resultDiv = this.$$('#set-tethering-config-result');
          resultDiv.innerText = result;
          resultDiv.classList.toggle('error', !success);
          if (success) {
            this.getTetheringConfig_();
          }
        });
  },

  /** @private */
  checkTetheringReadiness_() {
    this.browserProxy_.checkTetheringReadiness().then(result => {
      const resultDiv = this.$$('#check-tethering-readiness-result');
      resultDiv.innerText = result;
      resultDiv.classList.toggle('error', result !== 'ready');
    });
  },

  /**
   * Check if the input tethering config string is a valid JSON object.
   * @private
   */
  validateJSON_() {
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
  },

  /** @private */
  onTetheringToggleChanged_() {
    this.tetheringChangeInProgress_ = true;
    this.browserProxy_.setTetheringEnabled(this.isTetheringEnabled_)
        .then(result => {
          const resultDiv = this.$$('#set-tethering-enabled-result');
          resultDiv.innerText = result;
          resultDiv.classList.toggle('error', result !== 'success');
          this.getTetheringStatus_();
          this.tetheringChangeInProgress_ = false;
        });
  },

  /**
   * @param {!Event} event
   * @private
   */
  onHostnameChanged_(event) {
    this.browserProxy_.setHostname(this.hostname_);
  },

  /** @private */
  getHostname_() {
    this.browserProxy_.getHostname().then(result => this.hostname_ = result);
  },

  /**
   * Handles clicks on network items in the <network-select> element by
   * attempting a connection to the selected network or requesting a password
   * if the network requires a password.
   * @param {!Event<!OncMojo.NetworkStateProperties>} event
   * @private
   */
  onNetworkItemSelected_(event) {
    const networkState = event.detail;

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
      if (response.result == StartConnectResult.kSuccess) {
        return;
      }
      console.error(
          'startConnect error for: ' + networkState.guid + ' Result: ' +
          response.result.toString() + ' Message: ' + response.message);
      this.browserProxy_.showNetworkConfig(networkState.guid);
    });
  },

  /**
   * Returns and typecasts the network diagnostics element
   * @returns {!NetworkDiagnosticsElement}
   * @private
   */
  getNetworkDiagnosticsElement_() {
    return /** @type {!NetworkDiagnosticsElement} */ (
        this.$$('#network-diagnostics'));
  },

  /** @private */
  renderNetworkSelect_() {
    this.showNetworkSelect_ = true;
    flush();

    const select = this.$$('network-select');
    select.customItems = [
      {
        customItemName: 'addWiFiListItemName',
        polymerIcon: 'cr:add',
        customData: 'WiFi',
      },
    ];
  },

  /**
   * Handles requests to open the feedback report dialog. The provided string
   * in the event will be sent as a part of the feedback report.
   * @param {!Event<string>} event
   * @private
   */
  onSendFeedbackReportClick_(event) {
    chrome.send('OpenFeedbackDialog');
  },

  /**
   * Handles requests to open the feedback report dialog. The provided string
   * in the event will be sent as a part of the feedback report.
   * @param {!Event<string>} event
   * @private
   */
  onRunAllRoutinesClick_(event) {
    this.getNetworkDiagnosticsElement_().runAllRoutines();
  },

  /**
   * @param {!Event<!{detail:{customData: string}}>} event
   * @private
   */
  onCustomItemSelected_(event) {
    this.browserProxy_.addNetwork(event.detail.customData);
  },
});
