// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/chromeos/cellular_setup/cellular_setup_icons.m.js';
import '//resources/cr_components/chromeos/network/sim_lock_dialogs.m.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_settings_icons_css.js';
import '../../prefs/prefs.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import './cellular_setup_dialog.js';
import './internet_config.js';
import './internet_detail_menu.js';
import './internet_detail_page.js';
import './internet_known_networks_page.js';
import './internet_subpage.js';
import './network_summary.js';
import './esim_rename_dialog.js';
import './esim_remove_profile_dialog.js';

import {Button, ButtonBarState, ButtonState, CellularSetupPageName} from '//resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
import {getESimProfile, getESimProfileProperties, getEuicc, getNonPendingESimProfiles, getNumESimProfiles, getPendingESimProfiles} from '//resources/cr_components/chromeos/cellular_setup/esim_manager_utils.m.js';
import {getSimSlotCount, hasActiveCellularNetwork, isActiveSim, isConnectedToNonCellularNetwork} from '//resources/cr_components/chromeos/network/cellular_utils.m.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from '//resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {NetworkListenerBehavior} from '//resources/cr_components/chromeos/network/network_listener_behavior.m.js';
import {OncMojo} from '//resources/cr_components/chromeos/network/onc_mojo.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page_browser_proxy.js';


const mojom = chromeos.networkConfig.mojom;

/** @type {number} */
const ESIM_PROFILE_LIMIT = 5;

/**
 * @fileoverview
 * 'settings-internet-page' is the settings page containing internet
 * settings.
 */
Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-internet-page',

  behaviors: [
    NetworkListenerBehavior,
    DeepLinkingBehavior,
    I18nBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The device state for each network device type, keyed by NetworkType. Set
     * by network-summary.
     * @type {!Object<!OncMojo.DeviceStateProperties>|undefined}
     * @private
     */
    deviceStates: {
      type: Object,
      notify: true,
      observer: 'onDeviceStatesChanged_',
    },

    /**
     * Highest priority connected network or null. Set by network-summary.
     * @type {?OncMojo.NetworkStateProperties|undefined}
     */
    defaultNetwork: {
      type: Object,
      notify: true,
    },

    /**
     * Set by internet-subpage. Controls spinner visibility in subpage header.
     * @private
     */
    showSpinner_: Boolean,

    /**
     * The network type for the networks subpage when shown.
     * @type {chromeos.networkConfig.mojom.NetworkType}
     * @private
     */
    subpageType_: Number,

    /**
     * The network type for the known networks subpage when shown.
     * @type {chromeos.networkConfig.mojom.NetworkType}
     * @private
     */
    knownNetworksType_: Number,

    /**
     * Whether the 'Add connection' section is expanded.
     * @private
     */
    addConnectionExpanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * True if VPN is prohibited by policy.
     * @private {boolean}
     */
    vpnIsProhibited_: {
      type: Boolean,
      value: false,
    },

    /** @private {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} */
    globalPolicy_: Object,

    /**
     * Whether a managed network is available in the visible network list.
     * @private {boolean}
     */
    managedNetworkAvailable: {
      type: Boolean,
      value: false,
    },

    /**
     * List of third party (Extension + Arc) VPN providers.
     * @type {!Array<!chromeos.networkConfig.mojom.VpnProvider>}
     * @private
     */
    vpnProviders_: {
      type: Array,
      value() {
        return [];
      }
    },

    /** @private {boolean} */
    showInternetConfig_: {
      type: Boolean,
      value: false,
    },

    /**
     * Page name, if defined, indicating that the next deviceStates update
     * should call attemptShowCellularSetupDialog_().
     * @private {CellularSetupPageName|null}
     */
    pendingShowCellularSetupDialogAttemptPageName_: {
      type: String,
      value: null,
    },

    /** @private {boolean} */
    showCellularSetupDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Name of cellular setup dialog page.
     * @private {!CellularSetupPageName|null}
     */
    cellularSetupDialogPageName_: String,

    /** @private {boolean} */
    hasActiveCellularNetwork_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    isConnectedToNonCellularNetwork_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showESimProfileRenameDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showESimRemoveProfileDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Flag, if true, indicating that the next deviceStates update
     * should set showSimLockDialog_ to true.
     * @private
     */
    pendingShowSimLockDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showSimLockDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * eSIM network used in internet detail menu.
     * @private {chromeos.networkConfig.mojom.NetworkStateProperties}
     */
    eSimNetworkState_: {
      type: Object,
      value: '',
    },

    /** @private {!Map<string, Element>} */
    focusConfig_: {
      type: Object,
      value() {
        return new Map();
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kWifiOnOff,
        chromeos.settings.mojom.Setting.kMobileOnOff,
      ]),
    },

    /** @private */
    errorToastMessage_: {
      type: String,
      value: '',
    },
  },

  /**
   * Type of last detail page visited
   * @private {chromeos.networkConfig.mojom.NetworkType|undefined}
   */
  detailType_: undefined,

  // Element event listeners
  listeners: {
    'device-enabled-toggled': 'onDeviceEnabledToggled_',
    'network-connect': 'onNetworkConnect_',
    'show-cellular-setup': 'onShowCellularSetupDialog_',
    'show-config': 'onShowConfig_',
    'show-detail': 'onShowDetail_',
    'show-known-networks': 'onShowKnownNetworks_',
    'show-networks': 'onShowNetworks_',
    'show-esim-profile-rename-dialog': 'onShowESimProfileRenameDialog_',
    'show-esim-remove-profile-dialog': 'onShowESimRemoveProfileDialog_',
    'show-error-toast': 'onShowErrorToast_',
  },

  /** @private  {?InternetPageBrowserProxy} */
  browserProxy_: null,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.browserProxy_ = InternetPageBrowserProxyImpl.getInstance();
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  },

  /** @override */
  attached() {
    this.onPoliciesApplied(/*userhash=*/ '');
    this.onVpnProvidersChanged();
    this.onNetworkStateListChanged();
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    // Manually show the deep links for settings nested within elements.
    let networkType = null;
    if (settingId === chromeos.settings.mojom.Setting.kWifiOnOff) {
      networkType = mojom.NetworkType.kWiFi;
    } else if (settingId === chromeos.settings.mojom.Setting.kMobileOnOff) {
      networkType = mojom.NetworkType.kCellular;
    }

    afterNextRender(this, () => {
      const networkRow = this.$$('network-summary').getNetworkRow(networkType);
      if (networkRow && networkRow.getDeviceEnabledToggle()) {
        this.showDeepLinkElement(networkRow.getDeviceEnabledToggle());
        return;
      }
      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (route === routes.INTERNET_NETWORKS) {
      // Handle direct navigation to the networks page,
      // e.g. chrome://settings/internet/networks?type=WiFi
      const queryParams = Router.getInstance().getQueryParameters();
      const type = queryParams.get('type');
      if (type) {
        this.subpageType_ = OncMojo.getNetworkTypeFromString(type);
      }

      if (!oldRoute && queryParams.get('showCellularSetup') === 'true') {
        const pageName = queryParams.get('showPsimFlow') === 'true' ?
            CellularSetupPageName.PSIM_FLOW_UI :
            CellularSetupPageName.ESIM_FLOW_UI;
        // If the page just loaded, deviceStates will not be fully initialized
        // yet. Set pendingShowCellularSetupDialogAttemptPageName_ to indicate
        // showCellularSetupDialogAttempt_() should be called next deviceStates
        // update.
        this.pendingShowCellularSetupDialogAttemptPageName_ = pageName;
      }

      // If the page just loaded, deviceStates will not be fully initialized
      // yet. Set pendingShowSimLockDialog_ to indicate
      // showSimLockDialog_ should be set next deviceStates
      // update.
      this.pendingShowSimLockDialog_ = !oldRoute &&
          !!queryParams.get('showSimLockDialog') &&
          this.subpageType_ === mojom.NetworkType.kCellular;
    } else if (route === routes.KNOWN_NETWORKS) {
      // Handle direct navigation to the known networks page,
      // e.g. chrome://settings/internet/knownNetworks?type=WiFi
      const queryParams = Router.getInstance().getQueryParameters();
      const type = queryParams.get('type');
      if (type) {
        this.knownNetworksType_ = OncMojo.getNetworkTypeFromString(type);
      } else {
        this.knownNetworksType_ = mojom.NetworkType.kWiFi;
      }
    } else if (route === routes.INTERNET) {
      // Show deep links for the internet page.
      this.attemptDeepLink();
    } else if (route !== routes.BASIC) {
      // If we are navigating to a non internet section, do not set focus.
      return;
    }

    if (!routes.INTERNET || !routes.INTERNET.contains(oldRoute)) {
      return;
    }

    // Focus the subpage arrow where appropriate.
    let element;
    if (route === routes.INTERNET_NETWORKS) {
      // iron-list makes the correct timing to focus an item in the list
      // very complicated, and the item may not exist, so just focus the
      // entire list for now.
      const subPage = this.$$('settings-internet-subpage');
      if (subPage) {
        element = subPage.$$('#networkList');
      }
    } else if (this.detailType_ !== undefined) {
      const rowForDetailType =
          this.$$('network-summary').getNetworkRow(this.detailType_);

      // Note: It is possible that the row is no longer present in the DOM
      // (e.g., when a Cellular dongle is unplugged or when Instant Tethering
      // becomes unavailable due to the Bluetooth controller disconnecting).
      if (rowForDetailType) {
        element = rowForDetailType.$$('.subpage-arrow');
      }
    }
    if (element) {
      this.focusConfig_.set(oldRoute.path, element);
    } else {
      this.focusConfig_.delete(oldRoute.path);
    }
  },

  /** NetworkListenerBehavior override */
  onNetworkStateListChanged() {
    hasActiveCellularNetwork().then((hasActive) => {
      this.hasActiveCellularNetwork_ = hasActive;
    });
    this.updateIsConnectedToNonCellularNetwork_();
  },

  onVpnProvidersChanged() {
    this.networkConfig_.getVpnProviders().then(response => {
      const providers = response.providers;
      providers.sort(this.compareVpnProviders_);
      this.vpnProviders_ = providers;
    });
  },

  /** @param {string} userhash */
  onPoliciesApplied(userhash) {
    this.networkConfig_.getGlobalPolicy().then(response => {
      this.globalPolicy_ = response.result;
    });
  },

  /**
   * @return {!Promise<boolean>}
   * @private
   */
  updateIsConnectedToNonCellularNetwork_() {
    return isConnectedToNonCellularNetwork().then((isConnected) => {
      this.isConnectedToNonCellularNetwork_ = isConnected;
      return isConnected;
    });
  },

  /**
   * Event triggered by a device state enabled toggle.
   * @param {!CustomEvent<!{
   *     enabled: boolean,
   *     type: chromeos.networkConfig.mojom.NetworkType
   * }>} event
   * @private
   */
  onDeviceEnabledToggled_(event) {
    this.networkConfig_.setNetworkTypeEnabledState(
        event.detail.type, event.detail.enabled);
    recordSettingChange();
  },

  /**
   * @param {!CustomEvent<!{type: string, guid: ?string, name: ?string}>} event
   * @private
   */
  onShowConfig_(event) {
    const type = OncMojo.getNetworkTypeFromString(event.detail.type);
    if (!event.detail.guid) {
      // New configuration
      this.showConfig_(true /* configAndConnect */, type);
    } else {
      this.showConfig_(
          false /* configAndConnect */, type, event.detail.guid,
          event.detail.name);
    }
  },

  /**
   * @param {Event} event
   * @private
   */
  onShowCellularSetupDialog_(event) {
    this.attemptShowCellularSetupDialog_(event.detail.pageName);
  },

  /**
   * Opens the cellular setup dialog if pageName is PSIM_FLOW_UI, or if pageName
   * is ESIM_FLOW_UI and isConnectedToNonCellularNetwork_ is true. If
   * isConnectedToNonCellularNetwork_ is false, shows an error toast.
   * @param {CellularSetupPageName} pageName
   * @private
   */
  attemptShowCellularSetupDialog_(pageName) {
    const cellularDeviceState =
        this.getDeviceState_(mojom.NetworkType.kCellular, this.deviceStates);
    if (!cellularDeviceState ||
        cellularDeviceState.deviceState !== mojom.DeviceStateType.kEnabled) {
      this.showErrorToast_(this.i18n('eSimMobileDataNotEnabledErrorToast'));
      return;
    }

    if (pageName === CellularSetupPageName.PSIM_FLOW_UI) {
      this.showCellularSetupDialog_ = true;
      this.cellularSetupDialogPageName_ = pageName;
    } else {
      this.attemptShowESimSetupDialog_();
    }
  },

  /** @private */
  async attemptShowESimSetupDialog_() {
    const numProfiles = await getNumESimProfiles();
    if (numProfiles >= ESIM_PROFILE_LIMIT) {
      this.showErrorToast_(
          this.i18n('eSimProfileLimitReachedErrorToast', ESIM_PROFILE_LIMIT));
      return;
    }
    // isConnectedToNonCellularNetwork_ may
    // not be fetched yet if the page just opened, fetch it
    // explicitly.
    this.updateIsConnectedToNonCellularNetwork_().then(
        ((isConnected) => {
          this.showCellularSetupDialog_ =
              isConnected || loadTimeData.getBoolean('bypassConnectivityCheck');
          if (!this.showCellularSetupDialog_) {
            this.showErrorToast_(this.i18n('eSimNoConnectionErrorToast'));
            return;
          }
          this.cellularSetupDialogPageName_ =
              CellularSetupPageName.ESIM_FLOW_UI;
        }).bind(this));
  },

  /**
   * @param {!CustomEvent<string>} event
   * @private
   */
  onShowErrorToast_(event) {
    this.showErrorToast_(event.detail);
  },

  /**
   * @param {string} message
   * @private
   */
  showErrorToast_(message) {
    this.errorToastMessage_ = message;
    this.$.errorToast.show();
  },

  /** @private */
  onCloseCellularSetupDialog_() {
    this.showCellularSetupDialog_ = false;
  },

  /**
   * @param {boolean} configAndConnect
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @param {?string=} opt_guid
   * @param {?string=} opt_name
   * @private
   */
  showConfig_(configAndConnect, type, opt_guid, opt_name) {
    assert(
        type !== chromeos.networkConfig.mojom.NetworkType.kCellular &&
        type !== chromeos.networkConfig.mojom.NetworkType.kTether);
    if (this.showInternetConfig_) {
      return;
    }
    this.showInternetConfig_ = true;
    // Async call to ensure dialog is stamped.
    setTimeout(() => {
      const configDialog =
          /** @type {!InternetConfigElement} */ (this.$$('#configDialog'));
      assert(!!configDialog);
      configDialog.type = OncMojo.getNetworkTypeString(type);
      configDialog.guid = opt_guid || '';
      configDialog.name = opt_name || '';
      configDialog.showConnect = configAndConnect;
      configDialog.open();
    });
  },

  /** @private */
  onInternetConfigClose_() {
    this.showInternetConfig_ = false;
  },

  /**
   * @param {!CustomEvent<!OncMojo.NetworkStateProperties>} event
   * @private
   */
  onShowDetail_(event) {
    const networkState = event.detail;
    this.detailType_ = networkState.type;
    const params = new URLSearchParams();
    params.append('guid', networkState.guid);
    params.append('type', OncMojo.getNetworkTypeString(networkState.type));
    params.append('name', OncMojo.getNetworkStateDisplayName(networkState));
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
  },

  /**
   * @param {!CustomEvent<!{networkState:
   *     chromeos.networkConfig.mojom.NetworkStateProperties}>} event
   * @private
   */
  onShowESimProfileRenameDialog_(event) {
    this.eSimNetworkState_ = event.detail.networkState;
    this.showESimProfileRenameDialog_ = true;
  },

  /** @private */
  onCloseESimProfileRenameDialog_() {
    this.showESimProfileRenameDialog_ = false;
  },

  /**
   * @param {!CustomEvent<!{networkState:
   *     chromeos.networkConfig.mojom.NetworkStateProperties}>} event
   * @private
   */
  onShowESimRemoveProfileDialog_(event) {
    this.eSimNetworkState_ = event.detail.networkState;
    this.showESimRemoveProfileDialog_ = true;
  },

  /** @private */
  onCloseESimRemoveProfileDialog_() {
    this.showESimRemoveProfileDialog_ = false;
  },

  /**
   * @param {!CustomEvent<chromeos.networkConfig.mojom.NetworkType>} event
   * @private
   */
  onShowNetworks_(event) {
    this.showNetworksSubpage_(event.detail);
  },

  /**
   * @return {string}
   * @private
   */
  getNetworksPageTitle_() {
    // The shared Cellular/Tether subpage is referred to as "Mobile".
    // TODO(khorimoto): Remove once Cellular/Tether are split into their own
    // sections.
    if (this.subpageType_ === mojom.NetworkType.kCellular ||
        this.subpageType_ === mojom.NetworkType.kTether) {
      return this.i18n('OncTypeMobile');
    }
    return this.i18n(
        'OncType' + OncMojo.getNetworkTypeString(this.subpageType_));
  },

  /**
   * @param {chromeos.networkConfig.mojom.NetworkType} subpageType
   * @param {!Object<!OncMojo.DeviceStateProperties>|undefined} deviceStates
   * @return {!OncMojo.DeviceStateProperties|undefined}
   * @private
   */
  getDeviceState_(subpageType, deviceStates) {
    if (subpageType === undefined) {
      return undefined;
    }
    // If both Tether and Cellular are enabled, use the Cellular device state
    // when directly navigating to the Tether page.
    if (subpageType === mojom.NetworkType.kTether &&
        this.deviceStates[mojom.NetworkType.kCellular]) {
      subpageType = mojom.NetworkType.kCellular;
    }
    return deviceStates[subpageType];
  },

  /**
   * @param {!Object<!OncMojo.DeviceStateProperties>|undefined} deviceStates
   * @return {!OncMojo.DeviceStateProperties|undefined}
   * @private
   */
  getTetherDeviceState_(deviceStates) {
    return deviceStates[mojom.NetworkType.kTether];
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} newValue
   * @param {!OncMojo.DeviceStateProperties|undefined} oldValue
   * @private
   */
  onDeviceStatesChanged_(newValue, oldValue) {
    const wifiDeviceState =
        this.getDeviceState_(mojom.NetworkType.kWiFi, newValue);
    let managedNetworkAvailable = false;
    if (wifiDeviceState) {
      managedNetworkAvailable = !!wifiDeviceState.managedNetworkAvailable;
    }

    if (this.managedNetworkAvailable !== managedNetworkAvailable) {
      this.managedNetworkAvailable = managedNetworkAvailable;
    }

    const vpn = this.deviceStates[mojom.NetworkType.kVPN];
    this.vpnIsProhibited_ = !!vpn &&
        vpn.deviceState ===
            chromeos.networkConfig.mojom.DeviceStateType.kProhibited;

    if (this.detailType_ && !this.deviceStates[this.detailType_]) {
      // If the device type associated with the current network has been
      // removed (e.g., due to unplugging a Cellular dongle), the details page,
      // if visible, displays controls which are no longer functional. If this
      // case occurs, close the details page.
      const detailPage = this.$$('settings-internet-detail-page');
      if (detailPage) {
        detailPage.close();
      }
    }

    if (this.pendingShowCellularSetupDialogAttemptPageName_) {
      this.attemptShowCellularSetupDialog_(
          this.pendingShowCellularSetupDialogAttemptPageName_);
      this.pendingShowCellularSetupDialogAttemptPageName_ = null;
    }

    if (this.pendingShowSimLockDialog_) {
      this.showSimLockDialog_ = true;
      this.pendingShowSimLockDialog_ = false;
    }
  },

  /**
   * @param {!CustomEvent<chromeos.networkConfig.mojom.NetworkType>} event
   * @private
   */
  onShowKnownNetworks_(event) {
    const type = event.detail;
    this.detailType_ = type;
    this.knownNetworksType_ = type;
    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(type));
    Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
  },

  /** @private */
  onAddWiFiTap_() {
    this.showConfig_(
        true /* configAndConnect */,
        chromeos.networkConfig.mojom.NetworkType.kWiFi);
  },

  /** @private */
  onAddVPNTap_() {
    if (!this.vpnIsProhibited_) {
      this.showConfig_(
          true /* configAndConnect */,
          chromeos.networkConfig.mojom.NetworkType.kVPN);
    }
  },

  /**
   * @param {!{model: !{item: !mojom.VpnProvider}}} event
   * @private
   */
  onAddThirdPartyVpnTap_(event) {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.appId);
    recordSettingChange();
  },

  /**
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @private
   */
  showNetworksSubpage_(type) {
    this.detailType_ = type;
    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(type));
    this.subpageType_ = type;
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
  },

  /**
   * @param {!mojom.VpnProvider} vpnProvider1
   * @param {!mojom.VpnProvider} vpnProvider2
   * @return {number}
   */
  compareVpnProviders_(vpnProvider1, vpnProvider2) {
    // Show Extension VPNs before Arc VPNs.
    if (vpnProvider1.type < vpnProvider2.type) {
      return -1;
    }
    if (vpnProvider1.type > vpnProvider2.type) {
      return 1;
    }
    // Show VPNs of the same type by lastLaunchTime.
    if (vpnProvider1.lastLaunchTime.internalValue >
        vpnProvider2.lastLaunchTime.internalValue) {
      return -1;
    }
    if (vpnProvider1.lastLaunchTime.internalValue <
        vpnProvider2.lastLaunchTime.internalValue) {
      return 1;
    }
    return 0;
  },

  /**
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStates
   * @return {boolean}
   * @private
   */
  wifiIsEnabled_(deviceStates) {
    const wifi = deviceStates[mojom.NetworkType.kWiFi];
    return !!wifi &&
        wifi.deviceState ===
        chromeos.networkConfig.mojom.DeviceStateType.kEnabled;
  },

  /**
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStates
   * @return {boolean}
   * @private
   */
  shouldShowAddWiFiRow_(globalPolicy, managedNetworkAvailable, deviceStates) {
    return this.allowAddWiFiConnection_(
               globalPolicy, managedNetworkAvailable) &&
        this.wifiIsEnabled_(deviceStates);
  },

  /**
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   */
  allowAddWiFiConnection_(globalPolicy, managedNetworkAvailable) {
    if (!globalPolicy) {
      return true;
    }

    return !globalPolicy.allowOnlyPolicyWifiNetworksToConnect &&
        (!globalPolicy.allowOnlyPolicyWifiNetworksToConnectIfAvailable ||
         !managedNetworkAvailable);
  },

  /**
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   */
  allowAddConnection_(globalPolicy, managedNetworkAvailable) {
    if (!this.vpnIsProhibited_) {
      return true;
    }
    return this.allowAddWiFiConnection_(globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {!mojom.VpnProvider} provider
   * @return {string}
   */
  getAddThirdPartyVpnLabel_(provider) {
    return this.i18n('internetAddThirdPartyVPN', provider.providerName || '');
  },

  /**
   * Handles UI requests to connect to a network.
   * TODO(stevenjb): Handle Cellular activation, etc.
   * @param {!CustomEvent<!{
   *     networkState: !OncMojo.NetworkStateProperties,
   *     bypassConnectionDialog: (boolean|undefined)
   * }>} event
   * @private
   */
  onNetworkConnect_(event) {
    const networkState = event.detail.networkState;
    const type = networkState.type;
    const displayName = OncMojo.getNetworkStateDisplayName(networkState);

    if (!event.detail.bypassConnectionDialog &&
        type === mojom.NetworkType.kTether &&
        !networkState.typeState.tether.hasConnectedToHost) {
      const params = new URLSearchParams();
      params.append('guid', networkState.guid);
      params.append('type', OncMojo.getNetworkTypeString(type));
      params.append('name', displayName);
      params.append('showConfigure', true.toString());

      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
      return;
    }

    if (OncMojo.networkTypeHasConfigurationFlow(type) &&
        (!OncMojo.isNetworkConnectable(networkState) ||
         !!networkState.errorState)) {
      this.showConfig_(
          true /* configAndConnect */, type, networkState.guid, displayName);
      return;
    }

    this.networkConfig_.startConnect(networkState.guid).then(response => {
      switch (response.result) {
        case mojom.StartConnectResult.kSuccess:
          return;
        case mojom.StartConnectResult.kInvalidGuid:
        case mojom.StartConnectResult.kInvalidState:
        case mojom.StartConnectResult.kCanceled:
          // TODO(stevenjb/khorimoto): Consider handling these cases.
          return;
        case mojom.StartConnectResult.kNotConfigured:
          if (OncMojo.networkTypeHasConfigurationFlow(type)) {
            this.showConfig_(
                true /* configAndConnect */, type, networkState.guid,
                displayName);
          }
          return;
        case mojom.StartConnectResult.kBlocked:
          // This shouldn't happen, the UI should prevent this, fall through and
          // show the error.
        case mojom.StartConnectResult.kUnknown:
          console.error(
              'startConnect failed for: ' + networkState.guid +
              ' Error: ' + response.message);
          return;
      }
      assertNotReached();
    });
  },
});
