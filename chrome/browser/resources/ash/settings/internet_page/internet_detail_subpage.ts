// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-detail' is the settings subpage containing details
 * for a network.
 */

import 'chrome://resources/ash/common/network/cr_policy_network_indicator_mojo.js';
import 'chrome://resources/ash/common/network/network_apnlist.js';
import 'chrome://resources/ash/common/network/network_choose_mobile.js';
import 'chrome://resources/ash/common/network/network_config_toggle.js';
import 'chrome://resources/ash/common/network/network_icon.js';
import 'chrome://resources/ash/common/network/network_ip_config.js';
import 'chrome://resources/ash/common/network/network_nameservers.js';
import 'chrome://resources/ash/common/network/network_property_list_mojo.js';
import 'chrome://resources/ash/common/network/network_siminfo.js';
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../controls/controlled_button.js';
import '../controls/settings_toggle_button.js';
import './cellular_roaming_toggle_button.js';
import './internet_shared.css.js';
import './network_proxy_section.js';
import './passpoint_remove_dialog.js';
import './settings_traffic_counters.js';
import './tether_connection_dialog.js';

import {PrefsMixin, PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {PasspointServiceInterface, PasspointSubscription} from 'chrome://resources/ash/common/connectivity/passpoint.mojom-webui.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {getApnDisplayName, isActiveSim, isCarrierLockedActiveSim, processDeviceState, shouldDisallowNetworkModifications} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {TrafficCountersAdapter} from 'chrome://resources/ash/common/traffic_counters/traffic_counters_adapter.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ActivationStateType, ApnProperties, ConfigProperties, CrosNetworkConfigInterface, GlobalPolicy, HiddenSsidMode, IPConfigProperties, ManagedProperties, MatchType, NetworkStateProperties, ProxySettings, SecurityType, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, IPConfigType, NetworkType, OncSource, PolicySource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, flush, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists, castExists} from '../assert_extras.js';
import {DeepLinkingMixin, DeepLinkingMixinInterface} from '../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../common/route_observer_mixin.js';
import {Constructor} from '../common/types.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {OsSyncBrowserProxy, OsSyncBrowserProxyImpl, OsSyncPrefs} from '../os_people_page/os_sync_browser_proxy.js';
import {OsSettingsSubpageElement} from '../os_settings_page/os_settings_subpage.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './internet_detail_subpage.html.js';
import {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page_browser_proxy.js';
import {PasspointRemoveDialogElement} from './passpoint_remove_dialog.js';
import {TetherConnectionDialogElement} from './tether_connection_dialog.js';

const SettingsInternetDetailPageElementBase =
    mixinBehaviors(
        [
          NetworkListenerBehavior,
          CrPolicyNetworkBehaviorMojo,
        ],
        DeepLinkingMixin(PrefsMixin(RouteObserverMixin(
            WebUiListenerMixin(I18nMixin(PolymerElement)))))) as
    Constructor<PolymerElement&I18nMixinInterface&WebUiListenerMixinInterface&
                RouteObserverMixinInterface&PrefsMixinInterface&
                DeepLinkingMixinInterface&NetworkListenerBehaviorInterface&
                CrPolicyNetworkBehaviorMojoInterface>;

export class SettingsInternetDetailPageElement extends
    SettingsInternetDetailPageElementBase {
  static get is() {
    return 'settings-internet-detail-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The network GUID to display details for. */
      guid: String,

      /**
       * Whether network configuration properties sections should be shown. The
       * advanced section is not controlled by this property.
       */
      showConfigurableSections_: {
        type: Boolean,
        value: true,
        computed:
            'computeShowConfigurableSections_(deviceState_, managedProperties_)',
      },

      isWifiSyncEnabled_: Boolean,

      managedProperties_: {
        type: Object,
        observer: 'managedPropertiesChanged_',
      },

      deviceState_: {
        type: Object,
        value: null,
      },

      isSecondaryUser_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSecondaryUser');
        },
        readOnly: true,
      },

      primaryUserEmail_: {
        type: String,
        value() {
          return loadTimeData.getBoolean('isSecondaryUser') ?
              loadTimeData.getString('primaryUserEmail') :
              '';
        },
        readOnly: true,
      },

      /**
       * Whether the network has been lost (e.g., has gone out of range). A
       * network is considered to be lost when a OnNetworkStateListChanged
       * is signaled and the new network list does not contain the GUID of the
       * current network.
       */
      outOfRange_: {
        type: Boolean,
        value: false,
      },

      /**
       * Highest priority connected network or null.
       */
      defaultNetwork: {
        type: Object,
        value: null,
      },

      globalPolicy: Object,

      /**
       * Whether a managed network is available in the visible network list.
       */
      managedNetworkAvailable: {
        type: Boolean,
        value: false,
      },

      /**
       * The network AutoConnect state as a fake preference object.
       */
      autoConnectPref_: {
        type: Object,
        observer: 'autoConnectPrefChanged_',
        value() {
          return {
            key: 'fakeAutoConnectPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      /**
       * The network hidden state as a fake preference object.
       */
      hiddenPref_: {
        type: Object,
        observer: 'hiddenPrefChanged_',
        value() {
          return {
            key: 'fakeHiddenPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      /**
       * The always-on VPN state as a fake preference object.
       */
      alwaysOnVpn_: {
        type: Object,
        observer: 'alwaysOnVpnChanged_',
        value() {
          return {
            key: 'fakeAlwaysOnPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      /**
       * This gets initialized to managedProperties_.metered.activeValue.
       * When this is changed from the UI, a change event will update the
       * property and setMojoNetworkProperties will be called.
       */
      meteredOverride_: {
        type: Boolean,
        value: false,
      },

      /**
       * The network preferred state.
       */
      preferNetwork_: {
        type: Boolean,
        value: false,
        observer: 'preferNetworkChanged_',
      },

      /**
       * The network IP Address.
       */
      ipAddress_: {
        type: String,
        value: '',
      },

      /**
       * Whether to show technology badge on mobile network icons.
       */
      showTechnologyBadge_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showTechnologyBadge') &&
              loadTimeData.getBoolean('showTechnologyBadge');
        },
      },

      showMeteredToggle_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showMeteredToggle') &&
              loadTimeData.getBoolean('showMeteredToggle');
        },
      },

      /**
       * Whether to show the Hidden toggle on configured wifi networks (flag).
       */
      showHiddenToggle_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showHiddenToggle') &&
              loadTimeData.getBoolean('showHiddenToggle');
        },
      },

      isTrafficCountersEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('trafficCountersEnabled') &&
              loadTimeData.getBoolean('trafficCountersEnabled');
        },
      },

      isTrafficCountersForWifiTestingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('trafficCountersForWifiTesting') &&
              loadTimeData.getBoolean('trafficCountersForWifiTesting');
        },
      },

      /**
       * Tracks whether traffic counter info should be shown.
       */
      trafficCountersAvailable_: {
        type: Boolean,
        value: false,
      },

      /**
       * When true, all inputs that allow state to be changed (e.g., toggles,
       * inputs) are disabled.
       */
      disabled_: {
        type: Boolean,
        value: false,
        computed: 'computeDisabled_(deviceState_.*)',
      },

      isApnRevampEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isApnRevampEnabled') &&
              loadTimeData.getBoolean('isApnRevampEnabled');
        },
      },

      isApnRevampAndAllowApnModificationPolicyEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists(
                     'isApnRevampAndAllowApnModificationPolicyEnabled') &&
              loadTimeData.getBoolean(
                  'isApnRevampAndAllowApnModificationPolicyEnabled');
        },
      },

      passpointSubscription_: {
        type: Object,
        notify: true,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
      },

      advancedExpanded_: Boolean,

      networkExpanded_: Boolean,

      proxyExpanded_: Boolean,

      dataUsageExpanded_: Boolean,

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kConfigureEthernet,
          Setting.kEthernetAutoConfigureIp,
          Setting.kEthernetDns,
          Setting.kEthernetProxy,
          Setting.kDisconnectWifiNetwork,
          Setting.kPreferWifiNetwork,
          Setting.kForgetWifiNetwork,
          Setting.kWifiAutoConfigureIp,
          Setting.kWifiDns,
          Setting.kWifiHidden,
          Setting.kWifiProxy,
          Setting.kWifiAutoConnectToNetwork,
          Setting.kCellularSimLock,
          Setting.kCellularRoaming,
          Setting.kCellularApn,
          Setting.kDisconnectCellularNetwork,
          Setting.kCellularAutoConfigureIp,
          Setting.kCellularDns,
          Setting.kCellularProxy,
          Setting.kCellularAutoConnectToNetwork,
          Setting.kDisconnectTetherNetwork,
          Setting.kWifiMetered,
          Setting.kCellularMetered,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'updateAlwaysOnVpnPrefValue_(prefs.arc.vpn.always_on.*)',
      'updateAlwaysOnVpnPrefEnforcement_(managedProperties_,' +
          'prefs.vpn_config_allowed.*)',
      'updateAutoConnectPref_(globalPolicy)',
      'autoConnectPrefChanged_(autoConnectPref_.*)',
      'alwaysOnVpnChanged_(alwaysOnVpn_.*)',
      'hiddenPrefChanged_(hiddenPref_.*)',
    ];
  }

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  CR_EXPAND_BUTTON_TAG: string;
  defaultNetwork: OncMojo.NetworkStateProperties|null;
  globalPolicy?: GlobalPolicy;
  guid: string;
  managedNetworkAvailable: boolean;
  private advancedExpanded_: boolean;
  private alwaysOnVpn_: chrome.settingsPrivate.PrefObject<boolean>;
  private applyingChanges_: boolean;
  private autoConnectPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private browserProxy_: InternetPageBrowserProxy;
  private dataUsageExpanded_: boolean;
  private deviceState_: OncMojo.DeviceStateProperties|null;
  private didSetFocus_: boolean;
  private disabled_: boolean;
  private hiddenPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private ipAddress_: string;
  private isApnRevampEnabled_: boolean;
  private suppressTextMessagesOverride_: boolean;
  private isApnRevampAndAllowApnModificationPolicyEnabled_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private isSecondaryUser_: boolean;
  private isTrafficCountersEnabled_: boolean;
  private isTrafficCountersForWifiTestingEnabled_: boolean;
  private isWifiSyncEnabled_: boolean;
  private managedProperties_: ManagedProperties|undefined;
  private meteredOverride_: boolean;
  private networkConfig_: CrosNetworkConfigInterface;
  private networkExpanded_: boolean;
  private osSyncBrowserProxy_: OsSyncBrowserProxy;
  private outOfRange_: boolean;
  private passpointService_: PasspointServiceInterface;
  private passpointSubscription_: PasspointSubscription|null;
  private pendingSimLockDeepLink_: boolean;
  private preferNetwork_: boolean;
  private primaryUserEmail_: string;
  private propertiesReceived_: boolean;
  private proxyExpanded_: boolean;
  private shouldShowConfigureWhenNetworkLoaded_: boolean;
  private showConfigurableSections_: boolean;
  private showHiddenToggle_: boolean;
  private showMeteredToggle_: boolean;
  private showTechnologyBadge_: string;
  private trafficCountersAdapter_: TrafficCountersAdapter;
  private trafficCountersAvailable_: boolean;

  constructor() {
    super();

    this.CR_EXPAND_BUTTON_TAG = 'CR-EXPAND-BUTTON';

    this.didSetFocus_ = false;

    /**
     * Set to true to once the initial properties have been received. This
     * prevents setProperties from being called when setting default properties.
     */
    this.propertiesReceived_ = false;

    /**
     * Set in currentRouteChanged() if the showConfigure URL query
     * parameter is set to true. The dialog cannot be shown until the
     * network properties have been fetched in managedPropertiesChanged_().
     */
    this.shouldShowConfigureWhenNetworkLoaded_ = false;

    /**
     * Prevents re-saving incoming changes.
     */
    this.applyingChanges_ = false;

    /**
     * Flag, if true, indicating that the next deviceState_ update
     * should call deepLinkToSimLockElement_().
     */
    this.pendingSimLockDeepLink_ = false;

    this.browserProxy_ = InternetPageBrowserProxyImpl.getInstance();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();

    this.passpointService_ =
        MojoConnectivityProvider.getInstance().getPasspointService();

    this.osSyncBrowserProxy_ = OsSyncBrowserProxyImpl.getInstance();

    this.trafficCountersAdapter_ = new TrafficCountersAdapter();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'os-sync-prefs-changed', this.handleOsSyncPrefsChanged_.bind(this));
    this.osSyncBrowserProxy_.sendOsSyncPrefsChanged();
    this.computeTrafficCountersAvailable_();
  }

  private afterRenderShowDeepLink_(
      settingId: Setting, elementCallback: () => HTMLElement | null): void {
    // Wait for element to load.
    afterNextRender(this, () => {
      const deepLinkElement = elementCallback();
      if (!deepLinkElement || deepLinkElement.hidden) {
        console.warn(`Element with deep link id ${settingId} not focusable.`);
        return;
      }
      this.showDeepLinkElement(deepLinkElement);
    });
  }

  /**
   * Overridden from DeepLinkingMixin.
   */
  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    // Manually show the deep links for settings in shared elements.
    if (settingId === Setting.kCellularRoaming) {
      this.afterRenderShowDeepLink_(
          settingId,
          () =>
              this.shadowRoot!.querySelector('cellular-roaming-toggle-button')!
                  .getCellularRoamingToggle());
      // Stop deep link attempt since we completed it manually.
      return false;
    }

    if (settingId === Setting.kCellularApn) {
      this.networkExpanded_ = true;
      this.afterRenderShowDeepLink_(
          settingId,
          () => this.shadowRoot!.querySelector(
                                    'network-apnlist')!.getApnSelect());
      return false;
    }

    if (settingId === Setting.kEthernetAutoConfigureIp ||
        settingId === Setting.kWifiAutoConfigureIp ||
        settingId === Setting.kCellularAutoConfigureIp) {
      this.networkExpanded_ = true;
      this.afterRenderShowDeepLink_(
          settingId,
          () => this.shadowRoot!.querySelector('network-ip-config')!
                    .getAutoConfigIpToggle());
      return false;
    }

    if (settingId === Setting.kEthernetDns || settingId === Setting.kWifiDns ||
        settingId === Setting.kCellularDns) {
      this.networkExpanded_ = true;
      this.afterRenderShowDeepLink_(
          settingId,
          () => this.shadowRoot!.querySelector('network-nameservers')!
                    .getNameserverRadioButtons());
      return false;
    }

    if (settingId === Setting.kEthernetProxy ||
        settingId === Setting.kWifiProxy ||
        settingId === Setting.kCellularProxy) {
      this.proxyExpanded_ = true;
      this.afterRenderShowDeepLink_(
          settingId,
          () => this.shadowRoot!.querySelector('network-proxy-section')!
                    .getAllowSharedToggle());
      return false;
    }

    if (settingId === Setting.kWifiMetered ||
        settingId === Setting.kCellularMetered) {
      this.advancedExpanded_ = true;
      // Continue with automatically showing these deep links.
      return true;
    }

    if (settingId === Setting.kForgetWifiNetwork) {
      this.afterRenderShowDeepLink_(settingId, () => {
        const forgetButton = this.shadowRoot!.getElementById('forgetButton');
        if (forgetButton && !forgetButton.hidden) {
          return forgetButton;
        }
        // If forget button is hidden, show disconnect button instead.
        return this.shadowRoot!.getElementById('connectDisconnect');
      });
      return false;
    }

    if (settingId === Setting.kCellularSimLock) {
      this.advancedExpanded_ = true;

      // If the page just loaded, deviceState_ will not be fully initialized
      // yet, so we won't know which SIM info element to focus. Set
      // pendingSimLockDeepLink_ to indicate that a SIM info element should be
      // focused next deviceState_ update.
      this.pendingSimLockDeepLink_ = true;
      return false;
    }

    // Otherwise, should continue with deep link attempt.
    return true;
  }

  /**
   * RouteObserverMixin override
   */
  override currentRouteChanged(route: Route, oldRoute?: Route): void {
    if (route !== routes.NETWORK_DETAIL) {
      return;
    }

    const queryParams = Router.getInstance().getQueryParameters();
    const guid = queryParams.get('guid') || '';
    if (!guid) {
      console.warn('No guid specified for page:' + route);
      this.close();
    }

    this.shouldShowConfigureWhenNetworkLoaded_ =
        queryParams.get('showConfigure') === 'true';
    const type = queryParams.get('type') || 'WiFi';
    const name = queryParams.get('name') || type;
    this.init(guid, type, name);

    // If we are getting back from APN subpage set focus to the APN subpage
    // row.
    if (oldRoute === routes.APN &&
        Router.getInstance().lastRouteChangeWasPopstate()) {
      this.didSetFocus_ = true;
      afterNextRender(this, () => {
        const element = this.shadowRoot!.getElementById('apnSubpageButton');
        if (element) {
          element.focus();
        }
      });
    }
    this.attemptDeepLink();
  }

  /**
   * Handler for when os sync preferences are updated.
   */
  private handleOsSyncPrefsChanged_(osSyncPrefs: OsSyncPrefs): void {
    this.isWifiSyncEnabled_ =
        !!osSyncPrefs && osSyncPrefs.osWifiConfigurationsSynced;
  }

  init(guid: string, type: string, name: string): void {
    this.guid = guid;
    // Set default properties until they are loaded.
    this.propertiesReceived_ = false;
    this.deviceState_ = null;
    this.managedProperties_ = OncMojo.getDefaultManagedProperties(
        OncMojo.getNetworkTypeFromString(type), this.guid, name);
    this.didSetFocus_ = false;
    this.getNetworkDetails_();
  }

  close(): void {
    // If the page is already closed, return early to avoid navigating backward
    // erroneously.
    if (!this.guid) {
      return;
    }

    this.guid = '';

    // Delay navigating to allow other subpages to load first.
    requestAnimationFrame(() => {
      // Clear network properties before navigating away to ensure that a future
      // navigation back to the details page does not show a flicker of
      // incorrect text. See https://crbug.com/905986.
      this.managedProperties_ = undefined;
      this.propertiesReceived_ = false;

      if (Router.getInstance().currentRoute === routes.NETWORK_DETAIL) {
        Router.getInstance().navigateToPreviousRoute();
      }
    });
  }

  /** CrosNetworkConfigObserver impl */
  override onActiveNetworksChanged(networks: OncMojo.NetworkStateProperties[]):
      void {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    // If the network was or is active, request an update.
    if (this.managedProperties_.connectionState !==
            ConnectionStateType.kNotConnected ||
        networks.find(network => network.guid === this.guid)) {
      this.getNetworkDetails_();
    }
  }

  /** CrosNetworkConfigObserver impl */
  override onNetworkStateChanged(network: NetworkStateProperties): void {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    if (network.guid === this.guid) {
      this.getNetworkDetails_();
    }
  }

  /** CrosNetworkConfigObserver impl */
  override onNetworkStateListChanged(): void {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    this.checkNetworkExists_();
  }

  /** CrosNetworkConfigObserver impl */
  override onDeviceStateListChanged(): void {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    this.getDeviceState_();
  }

  private managedPropertiesChanged_(): void {
    if (!this.managedProperties_) {
      return;
    }
    this.updateAutoConnectPref_();
    this.updateHiddenPref_();

    if (this.isCellular_(this.managedProperties_) &&
        this.managedProperties_!.typeProperties.cellular!.allowTextMessages) {
      this.suppressTextMessagesOverride_ = !!OncMojo.getActiveValue(
          this.managedProperties_!.typeProperties.cellular!.allowTextMessages);
    }

    const metered = this.managedProperties_.metered;
    if (metered && metered.activeValue !== this.meteredOverride_) {
      this.meteredOverride_ = metered.activeValue;
    }

    const priority = this.managedProperties_.priority;
    if (priority) {
      const preferNetwork = priority.activeValue > 0;
      if (preferNetwork !== this.preferNetwork_) {
        this.preferNetwork_ = preferNetwork;
      }
    }

    // Set the IPAddress property to the IPv4 Address.
    const ipv4 =
        OncMojo.getIPConfigForType(this.managedProperties_, IPConfigType.kIPv4);
    this.ipAddress_ = (ipv4 && ipv4.ipAddress) || '';

    // Update the detail page title.
    const networkName = OncMojo.getNetworkNameUnsafe(this.managedProperties_);
    (this.parentNode as OsSettingsSubpageElement).pageTitle = networkName;
    flush();

    if (!this.didSetFocus_ &&
        !Router.getInstance().getQueryParameters().has('search') &&
        !this.getDeepLinkSettingId()) {
      // Unless the page was navigated to via search or has a deep linked
      // setting, focus a button once the initial state is set.
      this.didSetFocus_ = true;
      const button = this.shadowRoot!.querySelector<HTMLButtonElement>(
          '#titleDiv .action-button:not([hidden])');
      if (button) {
        afterNextRender(this, () => button.focus());
      }
    }

    if (this.shouldShowConfigureWhenNetworkLoaded_ &&
        this.managedProperties_.type === NetworkType.kTether) {
      // Set |this.shouldShowConfigureWhenNetworkLoaded_| back to false to
      // ensure that the Tether dialog is only shown once.
      this.shouldShowConfigureWhenNetworkLoaded_ = false;
      // Async call to ensure dialog is stamped.
      setTimeout(() => this.showTetherDialog_());
    }
  }

  private async getDeviceState_(): Promise<void> {
    if (!this.managedProperties_) {
      return;
    }
    const type = this.managedProperties_.type;
    const response = await this.networkConfig_.getDeviceStateList();

    // If there is no GUID, the page was closed between requesting the device
    // state and receiving it. If this occurs, there is no need to process the
    // response. Note that if this subpage is reopened later, we'll request
    // this data again.
    if (!this.guid) {
      return;
    }

    const {deviceState, shouldGetNetworkDetails} =
        processDeviceState(type, response.result, this.deviceState_);
    this.deviceState_ = deviceState;
    if (shouldGetNetworkDetails) {
      this.getNetworkDetails_();
    }
    if (this.pendingSimLockDeepLink_) {
      this.pendingSimLockDeepLink_ = false;
      this.deepLinkToSimLockElement_();
    }
  }

  private deepLinkToSimLockElement_(): void {
    const settingId = Setting.kCellularSimLock;
    const simLockStatus = this.deviceState_!.simLockStatus;

    // In this rare case, element not focusable until after a second wait.
    // This is slightly preferable to requestAnimationFrame used within
    // network-siminfo to focus elements since it can be reproduced in
    // testing.
    afterNextRender(this, () => {
      if (simLockStatus && !!simLockStatus.lockType) {
        this.afterRenderShowDeepLink_(
            settingId,
            () => this.shadowRoot!.querySelector(
                                      'network-siminfo')!.getUnlockButton());
        return;
      }
      this.afterRenderShowDeepLink_(
          settingId,
          () => this.shadowRoot!.querySelector(
                                    'network-siminfo')!.getSimLockToggle());
    });
  }

  private autoConnectPrefChanged_(): void {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.autoConnect = {value: !!this.autoConnectPref_.value};
    this.setMojoNetworkProperties_(config);
  }

  private hiddenPrefChanged_(): void {
    if (!this.propertiesReceived_) {
      return;
    }
    recordSettingChange(
        Setting.kWifiHidden, {boolValue: !!this.hiddenPref_.value});
    const config = this.getDefaultConfigProperties_();
    config.typeConfig.wifi!.hiddenSsid = this.hiddenPref_.value ?
        HiddenSsidMode.kEnabled :
        HiddenSsidMode.kDisabled;
    this.setMojoNetworkProperties_(config);
  }


  private getPolicyEnforcement_(policySource: PolicySource):
      chrome.settingsPrivate.Enforcement|undefined {
    switch (policySource) {
      case PolicySource.kUserPolicyEnforced:
      case PolicySource.kDevicePolicyEnforced:
        return chrome.settingsPrivate.Enforcement.ENFORCED;

      case PolicySource.kUserPolicyRecommended:
      case PolicySource.kDevicePolicyRecommended:
        return chrome.settingsPrivate.Enforcement.RECOMMENDED;

      default:
        return undefined;
    }
  }

  private getPolicyController_(policySource: PolicySource):
      chrome.settingsPrivate.ControlledBy|undefined {
    switch (policySource) {
      case PolicySource.kDevicePolicyEnforced:
      case PolicySource.kDevicePolicyRecommended:
        return chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;

      case PolicySource.kUserPolicyEnforced:
      case PolicySource.kUserPolicyRecommended:
        return chrome.settingsPrivate.ControlledBy.USER_POLICY;

      default:
        return undefined;
    }
  }

  /**
   * Updates auto-connect pref value.
   */
  private updateAutoConnectPref_(): void {
    if (!this.managedProperties_) {
      return;
    }
    const autoConnect = OncMojo.getManagedAutoConnect(this.managedProperties_);
    if (!autoConnect) {
      return;
    }

    let enforcement: chrome.settingsPrivate.Enforcement|undefined;
    let controlledBy: chrome.settingsPrivate.ControlledBy|undefined;

    if (this.globalPolicy &&
        this.globalPolicy.allowOnlyPolicyNetworksToAutoconnect) {
      enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
    } else {
      enforcement = this.getPolicyEnforcement_(autoConnect.policySource);
      controlledBy = this.getPolicyController_(autoConnect.policySource);
    }

    if (this.autoConnectPref_.value === autoConnect.activeValue &&
        enforcement === this.autoConnectPref_.enforcement &&
        controlledBy === this.autoConnectPref_.controlledBy) {
      return;
    }

    const newPrefValue: chrome.settingsPrivate.PrefObject<boolean> = {
      key: 'fakeAutoConnectPref',
      value: autoConnect.activeValue,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    if (enforcement) {
      newPrefValue.enforcement = enforcement;
      newPrefValue.controlledBy = controlledBy;
    }

    this.autoConnectPref_ = newPrefValue;
  }

  private updateHiddenPref_(): void {
    if (!this.managedProperties_) {
      return;
    }

    if (this.managedProperties_.type !== NetworkType.kWiFi) {
      return;
    }

    const hidden = this.managedProperties_.typeProperties.wifi!.hiddenSsid;
    if (!hidden) {
      return;
    }

    const enforcement = this.getPolicyEnforcement_(hidden.policySource);
    const controlledBy = this.getPolicyController_(hidden.policySource);
    if (this.hiddenPref_.value === hidden.activeValue &&
        enforcement === this.hiddenPref_.enforcement &&
        controlledBy === this.hiddenPref_.controlledBy) {
      return;
    }

    const newPrefValue: chrome.settingsPrivate.PrefObject<boolean> = {
      key: 'fakeHiddenPref',
      value: hidden.activeValue,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    if (enforcement) {
      newPrefValue.enforcement = enforcement;
      newPrefValue.controlledBy = controlledBy;
    }

    this.hiddenPref_ = newPrefValue;
  }

  private suppressTextMessagesChanged_(e: CustomEvent<{value: boolean}>): void {
    if (!this.propertiesReceived_ ||
        !this.isCellular_(this.managedProperties_) ||
        !this.managedProperties_!.typeProperties.cellular!.allowTextMessages) {
      return;
    }
    const config =
        OncMojo.getDefaultConfigProperties(this.managedProperties_!.type);
    config.typeConfig.cellular = {
      textMessageAllowState: {
        allowTextMessages: e.detail.value,
      },
      roaming: undefined,
      apn: undefined,
    };
    this.networkConfig_.setProperties(this.guid, config).then(response => {
      if (!response.success) {
        console.warn('Unable to set properties: ' + JSON.stringify(config));
      }
    });
  }

  private meteredChanged_(e: CustomEvent<{value: boolean}>): void {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.metered = {value: e.detail.value};
    this.setMojoNetworkProperties_(config);
  }

  private preferNetworkChanged_(): void {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.priority = {value: this.preferNetwork_ ? 1 : 0};
    this.setMojoNetworkProperties_(config);
  }

  private async checkNetworkExists_(): Promise<void> {
    const response = await this.networkConfig_.getNetworkState(this.guid);
    if (response.result) {
      // Don't update the state, a change event will trigger the update.
      return;
    }
    this.outOfRange_ = true;
    if (this.managedProperties_) {
      // Set the connection state since we won't receive an update for a non
      // existent network.
      this.managedProperties_.connectionState =
          ConnectionStateType.kNotConnected;
    }
  }

  private checkWifiOutOfRange_(networkState: OncMojo.NetworkStateProperties|
                               null): void {
    if (!networkState) {
      return;
    }
    if (networkState.type !== NetworkType.kWiFi) {
      this.outOfRange_ = false;
      return;
    }

    // A hidden network should always have the connect button regardless of
    // whether it's visible or not.
    this.outOfRange_ = !networkState.typeState.wifi!.hiddenSsid &&
        !networkState.typeState.wifi!.visible;
  }

  private async getNetworkDetails_(): Promise<void> {
    assertExists(this.guid);

    const networkStateResponse =
        await this.networkConfig_.getNetworkState(this.guid);
    this.checkWifiOutOfRange_(networkStateResponse.result);

    if (this.isSecondaryUser_) {
      this.getStateCallback_(networkStateResponse.result);
      return;
    }

    const response = await this.networkConfig_.getManagedProperties(this.guid);
    this.getPropertiesCallback_(response.result);
    if (this.isPasspointWifi_(this.managedProperties_)) {
      const response = await this.passpointService_.getPasspointSubscription(
          this.managedProperties_!.typeProperties.wifi!.passpointId!);
      this.passpointSubscription_ = response.result;
    }
  }

  private getPropertiesCallback_(properties: ManagedProperties|null): void {
    // Details page was closed while request was in progress, ignore the result.
    if (!this.guid) {
      return;
    }

    if (!properties) {
      // Close the page if the network was removed and no longer exists.
      this.close();
      return;
    }

    this.updateManagedProperties_(properties);

    // Detail page should not be shown when Arc VPN is not connected.
    if (this.isArcVpn_(this.managedProperties_) &&
        !this.isConnectedState_(this.managedProperties_)) {
      this.guid = '';
      this.close();
    }
    this.propertiesReceived_ = true;
    if (!this.deviceState_) {
      this.getDeviceState_();
    }
  }

  private updateManagedProperties_(properties: ManagedProperties): void {
    this.applyingChanges_ = true;
    if (this.managedProperties_ &&
        this.managedProperties_.type === NetworkType.kCellular &&
        this.deviceState_ && this.deviceState_.scanning) {
      // Cellular properties may be invalid while scanning, so keep the existing
      // properties instead.
      properties.typeProperties.cellular =
          this.managedProperties_.typeProperties.cellular;
    }
    this.managedProperties_ = properties;
    afterNextRender(this, () => {
      this.applyingChanges_ = false;
    });
  }

  private getStateCallback_(networkState: OncMojo.NetworkStateProperties|
                            null): void {
    if (!networkState) {
      // Edge case, may occur when disabling. Close this.
      this.close();
      return;
    }

    const managedProperties = OncMojo.getDefaultManagedProperties(
        networkState.type, networkState.guid, networkState.name);
    managedProperties.connectable = networkState.connectable;
    managedProperties.connectionState = networkState.connectionState;
    switch (networkState.type) {
      case NetworkType.kCellular:
        managedProperties.typeProperties.cellular!.signalStrength =
            networkState.typeState.cellular!.signalStrength;
        managedProperties.typeProperties.cellular!.simLocked =
            networkState.typeState.cellular!.simLocked;
        break;
      case NetworkType.kTether:
        managedProperties.typeProperties.tether!.signalStrength =
            networkState.typeState.tether!.signalStrength;
        break;
      case NetworkType.kWiFi:
        managedProperties.typeProperties.wifi!.signalStrength =
            networkState.typeState.wifi!.signalStrength;
        break;
    }
    this.updateManagedProperties_(managedProperties);
    this.propertiesReceived_ = true;
  }

  private getNetworkState_(properties: ManagedProperties):
      OncMojo.NetworkStateProperties|undefined {
    if (!properties) {
      return undefined;
    }
    return OncMojo.managedPropertiesToNetworkState(properties);
  }

  private getDefaultConfigProperties_(): ConfigProperties {
    return OncMojo.getDefaultConfigProperties(this.managedProperties_!.type);
  }

  private async setMojoNetworkProperties_(config: ConfigProperties):
      Promise<void> {
    if (!this.propertiesReceived_ || !this.guid || this.applyingChanges_) {
      return;
    }
    // TODO(b/282233232) recordSettingChange() for updating network properties.
    const response = await this.networkConfig_.setProperties(this.guid, config);
    if (!response.success) {
      console.warn('Unable to set properties: ' + JSON.stringify(config));
      // An error typically indicates invalid input; request the properties
      // to update any invalid fields.
      this.getNetworkDetails_();
    }
  }

  private getStateText_(
      managedProperties: ManagedProperties, propertiesReceived: boolean,
      outOfRange: boolean,
      deviceState: OncMojo.DeviceStateProperties|null): string {
    if (!managedProperties || !propertiesReceived) {
      return '';
    }

    if (this.isOutOfRangeOrNotEnabled_(outOfRange, deviceState)) {
      return managedProperties.type === NetworkType.kTether ?
          this.i18n('tetherPhoneOutOfRange') :
          this.i18n('networkOutOfRange');
    }

    if (OncMojo.connectionStateIsConnected(managedProperties.connectionState)) {
      if (this.isPortalState_(managedProperties.portalState)) {
        if (managedProperties.type === NetworkType.kCellular) {
          return this.i18n('networkListItemCellularSignIn');
        }
        return this.i18n('networkListItemSignIn');
      }
      if (managedProperties.portalState === PortalState.kNoInternet) {
        return this.i18n('networkListItemConnectedNoConnectivity');
      }
    }

    if (isCarrierLockedActiveSim(managedProperties, deviceState)) {
      return this.i18n('networkMobileProviderLocked');
    }

    return this.i18n(
        OncMojo.getConnectionStateString(managedProperties.connectionState));
  }

  private getAutoConnectToggleLabel_(managedProperties: ManagedProperties):
      string {
    return this.isCellular_(managedProperties) ?
        this.i18n('networkAutoConnectCellular') :
        this.i18n('networkAutoConnect');
  }

  private isConnectedState_(managedProperties: ManagedProperties|
                            undefined): boolean {
    return !!managedProperties &&
        OncMojo.connectionStateIsConnected(managedProperties.connectionState);
  }

  private isRestrictedConnectivity_(managedProperties: ManagedProperties|
                                    undefined): boolean {
    return !!managedProperties &&
        OncMojo.isRestrictedConnectivity(managedProperties.portalState);
  }

  private showConnectedState_(managedProperties: ManagedProperties|
                              undefined): boolean {
    return this.isConnectedState_(managedProperties) &&
        !this.isRestrictedConnectivity_(managedProperties);
  }

  private showRestrictedConnectivity_(
      managedProperties: ManagedProperties|undefined,
      deviceState: OncMojo.DeviceStateProperties|null): boolean {
    if (!managedProperties) {
      return false;
    }

    // Display carrier locked network as warning
    if (isCarrierLockedActiveSim(managedProperties, deviceState)) {
      return true;
    }

    // State must be connected and restricted.
    return this.isConnectedState_(managedProperties) &&
        this.isRestrictedConnectivity_(managedProperties);
  }

  private isRemembered_(managedProperties: ManagedProperties|
                        undefined): boolean {
    return !!managedProperties && managedProperties.source !== OncSource.kNone;
  }

  private isRememberedOrConnected_(managedProperties: ManagedProperties|
                                   undefined): boolean {
    return this.isRemembered_(managedProperties) ||
        this.isConnectedState_(managedProperties);
  }

  private isCellular_(managedProperties: ManagedProperties|undefined): boolean {
    return !!managedProperties &&
        managedProperties.type === NetworkType.kCellular;
  }

  private isTether_(managedProperties: ManagedProperties|undefined): boolean {
    return !!managedProperties &&
        managedProperties.type === NetworkType.kTether;
  }

  private isWiFi_(managedProperties: ManagedProperties|undefined): boolean {
    return !!managedProperties && managedProperties.type === NetworkType.kWiFi;
  }

  private isWireGuard_(managedProperties: ManagedProperties|
                       undefined): boolean {
    if (!managedProperties) {
      return false;
    }
    if (managedProperties.type !== NetworkType.kVPN) {
      return false;
    }
    if (!managedProperties.typeProperties.vpn) {
      return false;
    }
    return managedProperties.typeProperties.vpn.type === VpnType.kWireGuard;
  }

  private isBlockedByPolicy_(
      managedProperties: ManagedProperties|undefined,
      globalPolicy: GlobalPolicy|undefined,
      managedNetworkAvailable: boolean): boolean {
    if (!managedProperties || !globalPolicy ||
        this.isPolicySource(managedProperties.source)) {
      return false;
    }

    if (managedProperties.type === NetworkType.kCellular &&
        !!globalPolicy.allowOnlyPolicyCellularNetworks) {
      return true;
    }

    if (managedProperties.type !== NetworkType.kWiFi) {
      return false;
    }
    const hexSsid =
        OncMojo.getActiveString(managedProperties.typeProperties.wifi!.hexSsid);
    return !!globalPolicy.allowOnlyPolicyWifiNetworksToConnect ||
        (!!globalPolicy.allowOnlyPolicyWifiNetworksToConnectIfAvailable &&
         !!managedNetworkAvailable) ||
        (!!hexSsid && !!globalPolicy.blockedHexSsids &&
         globalPolicy.blockedHexSsids.includes(hexSsid));
  }

  private shouldShowApnRow_(): boolean {
    return this.isApnRevampEnabled_ &&
        this.isCellular_(this.managedProperties_);
  }

  private isApnManaged_(globalPolicy: GlobalPolicy|undefined): boolean {
    if (!this.isApnRevampAndAllowApnModificationPolicyEnabled_) {
      return false;
    }
    if (!globalPolicy) {
      return false;
    }
    return !globalPolicy.allowApnModification;
  }

  private shouldShowApnList_(): boolean {
    return !this.isApnRevampEnabled_ &&
        this.isCellular_(this.managedProperties_);
  }

  private shouldShowSuppressTextMessagesToggle_(): boolean {
    if (!this.managedProperties_ || !this.deviceState_) {
      return false;
    }
    const networkState = this.getNetworkState_(this.managedProperties_);
    if (!networkState) {
      return false;
    }
    // Only show the toggle for the active SIM with the flag enabled.
    return this.isCellular_(this.managedProperties_) &&
        isActiveSim(networkState, this.deviceState_);
  }

  private showConnect_(
      managedProperties: ManagedProperties|undefined,
      globalPolicy: GlobalPolicy|undefined, managedNetworkAvailable: boolean,
      deviceState: OncMojo.DeviceStateProperties|null): boolean {
    if (!managedProperties) {
      return false;
    }

    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }

    // TODO(lgcheng@) support connect Arc VPN from UI once Android support API
    // to initiate a VPN session.
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }

    if (managedProperties.connectionState !==
        ConnectionStateType.kNotConnected) {
      return false;
    }

    if (deviceState && deviceState.deviceState !== DeviceStateType.kEnabled) {
      return false;
    }

    const isEthernet = managedProperties.type === NetworkType.kEthernet;

    // Note: Ethernet networks do not have an explicit "Connect" button in the
    // UI.
    return OncMojo.isNetworkConnectable(managedProperties) && !isEthernet;
  }

  private showDisconnect_(managedProperties: ManagedProperties|
                          undefined): boolean {
    if (!managedProperties ||
        managedProperties.type === NetworkType.kEthernet) {
      return false;
    }
    return managedProperties.connectionState !==
        ConnectionStateType.kNotConnected;
  }

  private showSignin_(managedProperties: ManagedProperties|undefined): boolean {
    if (!managedProperties) {
      return false;
    }
    if (OncMojo.connectionStateIsConnected(managedProperties.connectionState) &&
        this.isPortalState_(managedProperties.portalState)) {
      return true;
    }
    return false;
  }

  private showForget_(managedProperties: ManagedProperties): boolean {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    const type = managedProperties.type;
    if (type !== NetworkType.kWiFi && type !== NetworkType.kVPN) {
      return false;
    }
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }
    return !this.isPolicySource(managedProperties.source) &&
        this.isRemembered_(managedProperties);
  }

  private showActivate_(managedProperties: ManagedProperties): boolean {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    if (!this.isCellular_(managedProperties)) {
      return false;
    }

    // Only show the Activate button for unactivated pSIM networks.
    if (managedProperties.typeProperties.cellular!.eid) {
      return false;
    }

    const activation =
        managedProperties.typeProperties.cellular!.activationState;
    return activation === ActivationStateType.kNotActivated ||
        activation === ActivationStateType.kPartiallyActivated;
  }

  private showConfigure_(
      managedProperties: ManagedProperties, globalPolicy: GlobalPolicy,
      managedNetworkAvailable: boolean): boolean {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    const type = managedProperties.type;
    if (type === NetworkType.kCellular || type === NetworkType.kTether) {
      return false;
    }
    if (type === NetworkType.kWiFi &&
        managedProperties.typeProperties.wifi!.security ===
            SecurityType.kNone) {
      return false;
    }
    if (type === NetworkType.kWiFi &&
        (managedProperties.connectionState !==
         ConnectionStateType.kNotConnected)) {
      return false;
    }
    if (this.isPasspointWifi_(managedProperties)) {
      // Passpoint networks are automatically configured using Passpoint
      // subscriptions. We don't want the user to change the configuration
      // (b/282114074).
      return false;
    }
    if (this.isArcVpn_(managedProperties) &&
        !this.isConnectedState_(managedProperties)) {
      return false;
    }
    return true;
  }

  private disableSignin_(managedProperties: ManagedProperties|
                         undefined): boolean {
    if (this.disabled_ || !managedProperties) {
      return true;
    }
    if (!OncMojo.connectionStateIsConnected(
            managedProperties.connectionState)) {
      return true;
    }
    return !this.isPortalState_(managedProperties.portalState);
  }


  private disableForget_(
      managedProperties: ManagedProperties|undefined,
      vpnConfigAllowed: chrome.settingsPrivate.PrefObject<boolean>): boolean {
    if (this.disabled_ || !managedProperties) {
      return true;
    }
    return managedProperties.type === NetworkType.kVPN && vpnConfigAllowed &&
        !vpnConfigAllowed.value;
  }

  private disableConfigure_(
      managedProperties: ManagedProperties|undefined,
      vpnConfigAllowed: chrome.settingsPrivate.PrefObject<boolean>): boolean {
    if (this.disabled_ || !managedProperties) {
      return true;
    }
    if (managedProperties.type === NetworkType.kVPN && vpnConfigAllowed &&
        !vpnConfigAllowed.value) {
      return true;
    }
    return this.isPolicySource(managedProperties.source) &&
        !this.hasRecommendedFields_(managedProperties);
  }

  private hasRecommendedFields_(managedProperties: ManagedProperties): boolean {
    if (!managedProperties) {
      return false;
    }
    for (const value of Object.values(managedProperties)) {
      if (typeof value !== 'object' || value === null) {
        continue;
      }
      if ('activeValue' in value) {
        if (this.isNetworkPolicyRecommended(value)) {
          return true;
        }
      } else if (this.hasRecommendedFields_(value)) {
        return true;
      }
    }
    return false;
  }

  private showViewAccount_(managedProperties: ManagedProperties|
                           undefined): boolean {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }

    // Show either the 'Activate' or the 'View Account' button (Cellular only).
    if (!this.isCellular_(managedProperties) ||
        this.showActivate_(managedProperties)) {
      return false;
    }

    // If the network is eSIM, don't show.
    if (managedProperties.typeProperties.cellular!.eid) {
      return false;
    }

    const paymentPortal =
        managedProperties.typeProperties.cellular!.paymentPortal;
    if (!paymentPortal || !paymentPortal.url) {
      return false;
    }

    // Only show for connected networks or LTE networks with a valid MDN.
    if (!this.isConnectedState_(managedProperties)) {
      const technology =
          managedProperties.typeProperties.cellular!.networkTechnology;
      if (technology !== 'LTE' && technology !== 'LTEAdvanced') {
        return false;
      }
      if (!managedProperties.typeProperties.cellular!.mdn) {
        return false;
      }
    }

    return true;
  }

  private enableConnect_(
      managedProperties: ManagedProperties|undefined,
      defaultNetwork: OncMojo.NetworkStateProperties|null,
      propertiesReceived: boolean, outOfRange: boolean,
      globalPolicy: GlobalPolicy|undefined, managedNetworkAvailable: boolean,
      deviceState: OncMojo.DeviceStateProperties|null): boolean {
    if (!this.showConnect_(
            managedProperties, globalPolicy, managedNetworkAvailable,
            deviceState)) {
      return false;
    }

    if (!propertiesReceived || outOfRange) {
      return false;
    }

    assertExists(managedProperties);
    if (managedProperties.type === NetworkType.kVPN && !defaultNetwork) {
      return false;
    }

    // Cannot connect to a network which is SIM locked; the user must first
    // unlock the SIM before attempting a connection.
    if (managedProperties.type === NetworkType.kCellular &&
        managedProperties.typeProperties.cellular!.simLocked) {
      return false;
    }

    return true;
  }

  private updateAlwaysOnVpnPrefValue_(): void {
    this.alwaysOnVpn_.value = this.prefs.arc && this.prefs.arc.vpn &&
        this.prefs.arc.vpn.always_on && this.prefs.arc.vpn.always_on.lockdown &&
        this.prefs.arc.vpn.always_on.lockdown.value;
  }

  private getFakeVpnConfigPrefForEnforcement_():
      chrome.settingsPrivate.PrefObject<boolean> {
    const fakeAlwaysOnVpnEnforcementPref:
        chrome.settingsPrivate.PrefObject<boolean> = {
      key: 'fakeAlwaysOnPref',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    // Only mark VPN networks as enforced. This fake pref also controls the
    // policy indicator on the connect/disconnect buttons, so it shouldn't be
    // shown on non-VPN networks.
    if (this.managedProperties_ &&
        this.managedProperties_.type === NetworkType.kVPN && this.prefs &&
        this.prefs.vpn_config_allowed && !this.prefs.vpn_config_allowed.value) {
      fakeAlwaysOnVpnEnforcementPref.enforcement =
          chrome.settingsPrivate.Enforcement.ENFORCED;
      fakeAlwaysOnVpnEnforcementPref.controlledBy =
          this.prefs.vpn_config_allowed.controlledBy;
    }
    return fakeAlwaysOnVpnEnforcementPref;
  }

  private updateAlwaysOnVpnPrefEnforcement_(): void {
    const prefForEnforcement = this.getFakeVpnConfigPrefForEnforcement_();
    this.alwaysOnVpn_.enforcement = prefForEnforcement.enforcement;
    this.alwaysOnVpn_.controlledBy = prefForEnforcement.controlledBy;
  }

  private getTetherDialog_(): TetherConnectionDialogElement {
    return castExists(
        this.shadowRoot!.querySelector<TetherConnectionDialogElement>(
            '#tetherDialog'));
  }

  private getPasspointRemovalDialog_(): PasspointRemoveDialogElement {
    return castExists(
        this.shadowRoot!.querySelector<PasspointRemoveDialogElement>(
            '#passpointRemovalDialog'));
  }

  private handleConnectClick_(): void {
    assertExists(this.managedProperties_);
    if (this.managedProperties_.type === NetworkType.kTether &&
        (!this.managedProperties_.typeProperties.tether!.hasConnectedToHost)) {
      this.showTetherDialog_();
      return;
    }
    this.fireNetworkConnect_(/*bypassDialog=*/ false);
  }

  private onTetherConnect_(): void {
    this.getTetherDialog_().close();
    this.fireNetworkConnect_(/*bypassDialog=*/ true);
  }

  private fireNetworkConnect_(bypassDialog: boolean): void {
    assertExists(this.managedProperties_);
    const networkState =
        OncMojo.managedPropertiesToNetworkState(this.managedProperties_);
    const networkConnectEvent = new CustomEvent('network-connect', {
      bubbles: true,
      composed: true,
      detail:
          {networkState: networkState, bypassConnectionDialog: bypassDialog},
    });
    this.dispatchEvent(networkConnectEvent);
    // TODO(b/282233232) recordSettingChange() for connecting to network.
  }

  private async handleDisconnectClick_(): Promise<void> {
    const response = await this.networkConfig_.startDisconnect(this.guid);
    if (response.success) {
      recordSettingChange(Setting.kDisconnectWifiNetwork);
    } else {
      console.warn('Disconnect failed for: ' + this.guid);
    }
  }

  private onConnectDisconnectClick_(): void {
    if (this.enableConnect_(
            this.managedProperties_, this.defaultNetwork,
            this.propertiesReceived_, this.outOfRange_, this.globalPolicy,
            this.managedNetworkAvailable, this.deviceState_)) {
      this.handleConnectClick_();
      return;
    }

    if (this.showDisconnect_(this.managedProperties_)) {
      this.handleDisconnectClick_();
      return;
    }
  }

  private shouldConnectDisconnectButtonBeHidden_(): boolean {
    return !this.showConnect_(
               this.managedProperties_, this.globalPolicy,
               this.managedNetworkAvailable, this.deviceState_) &&
        !this.showDisconnect_(this.managedProperties_);
  }

  private shouldConnectDisconnectButtonBeDisabled_(): boolean {
    if (this.disabled_) {
      return true;
    }
    if (this.enableConnect_(
            this.managedProperties_, this.defaultNetwork,
            this.propertiesReceived_, this.outOfRange_, this.globalPolicy,
            this.managedNetworkAvailable, this.deviceState_)) {
      return false;
    }
    if (this.showDisconnect_(this.managedProperties_)) {
      return false;
    }
    return true;
  }

  private getConnectDisconnectButtonLabel_(): string {
    if (this.showConnect_(
            this.managedProperties_, this.globalPolicy,
            this.managedNetworkAvailable, this.deviceState_)) {
      return this.i18n('networkButtonConnect');
    }

    if (this.showDisconnect_(this.managedProperties_)) {
      return this.i18n('networkButtonDisconnect');
    }

    return '';
  }

  private async onForgetClick_(): Promise<void> {
    if (this.isPasspointWifi_(this.managedProperties_)) {
      // Ask user confirmation before removing a Passpoint Wi-Fi and the
      // associated subscription.
      this.getPasspointRemovalDialog_().open();
      return;
    }
    return this.forgetNetwork_();
  }

  private async forgetNetwork_(): Promise<void> {
    if (this.managedProperties_!.type === NetworkType.kWiFi) {
      recordSettingChange(Setting.kForgetWifiNetwork);
    } else {
      // TODO(b/282233232) recordSettingChange() for other network types.
    }

    const response = await this.networkConfig_.forgetNetwork(this.guid);
    if (!response.success) {
      console.warn('Forget network failed for: ' + this.guid);
    }
    // A forgotten network no longer has a valid GUID, close the subpage.
    this.close();
  }

  private onSigninClick_(): void {
    this.browserProxy_.showPortalSignin(this.guid);
  }

  private onActivateClick_(): void {
    this.browserProxy_.showCellularSetupUi(this.guid);
  }

  private onConfigureClick_(): void {
    if (this.managedProperties_ &&
        (this.isThirdPartyVpn_(this.managedProperties_) ||
         this.isArcVpn_(this.managedProperties_))) {
      this.browserProxy_.configureThirdPartyVpn(this.guid);
      // TODO(b/282233232) recordSettingChange() for third party VPN configure.
      return;
    }

    assertExists(this.managedProperties_);
    const showConfigEvent = new CustomEvent('show-config', {
      bubbles: true,
      composed: true,
      detail: {
        guid: this.guid,
        type: OncMojo.getNetworkTypeString(this.managedProperties_.type),
        name: OncMojo.getNetworkNameUnsafe(this.managedProperties_),
      },
    });
    this.dispatchEvent(showConfigEvent);
  }

  private onViewAccountClick_(): void {
    this.browserProxy_.showCarrierAccountDetail(this.guid);
  }

  private showTetherDialog_(): void {
    this.getTetherDialog_().open();
  }

  private showHiddenNetworkWarning_(): boolean {
    return loadTimeData.getBoolean('showHiddenNetworkWarning') &&
        !!this.autoConnectPref_.value && !!this.managedProperties_ &&
        this.managedProperties_.type === NetworkType.kWiFi &&
        !!OncMojo.getActiveValue(
            this.managedProperties_.typeProperties.wifi!.hiddenSsid);
  }

  /**
   * Event triggered for elements associated with network properties.
   */
  private onNetworkPropertyChange_(
      e: CustomEvent<{field: string, value: (string|number|boolean|string[])}>):
      void {
    if (!this.propertiesReceived_) {
      return;
    }
    const field = e.detail.field;
    const value = e.detail.value;
    const config = this.getDefaultConfigProperties_();
    const valueType = typeof value;
    if (valueType !== 'string' && valueType !== 'number' &&
        valueType !== 'boolean' && !Array.isArray(value)) {
      console.warn(
          'Unexpected property change event, Key: ' + field +
          ' Value: ' + JSON.stringify(value));
      return;
    }
    OncMojo.setConfigProperty(config, field, value);
    // Ensure that any required configuration properties for partial
    // configurations are set.
    const vpnConfig = config.typeConfig.vpn;
    if (vpnConfig) {
      if (vpnConfig.openVpn &&
          vpnConfig.openVpn.saveCredentials === undefined) {
        vpnConfig.openVpn.saveCredentials = false;
      }
      if (vpnConfig.l2tp && vpnConfig.l2tp.saveCredentials === undefined) {
        vpnConfig.l2tp.saveCredentials = false;
      }
    }
    this.setMojoNetworkProperties_(config);
  }

  private onApnChange_(event: CustomEvent<ApnProperties>): void {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    const apn = event.detail;
    config.typeConfig
        .cellular = {apn, roaming: undefined, textMessageAllowState: undefined};
    this.setMojoNetworkProperties_(config);
  }

  private getApnRowSubLabel_(): string {
    if (!this.isCellular_(this.managedProperties_) ||
        !this.managedProperties_!.typeProperties.cellular!.connectedApn) {
      return '';
    }

    return getApnDisplayName(
        this.i18n.bind(this),
        this.managedProperties_!.typeProperties.cellular!.connectedApn);
  }


  private onApnRowClicked_(): void {
    if (this.disabled_) {
      return;
    }

    if (!this.isCellular_(this.managedProperties_)) {
      console.error(
          'APN row should only be visible when cellular is available.');
      return;
    }
    const params = new URLSearchParams();
    params.append('guid', this.guid);
    Router.getInstance().navigateTo(routes.APN, params);
  }

  /**
   * Event triggered when the IP Config or NameServers element changes.
   */
  private onIpConfigChange_(
      event: CustomEvent<
          {field: string, value: (string|IPConfigProperties|string[])}>): void {
    if (!this.managedProperties_) {
      return;
    }
    const config = OncMojo.getUpdatedIPConfigProperties(
        this.managedProperties_, event.detail.field, event.detail.value);
    if (config) {
      this.setMojoNetworkProperties_(config);
    }
  }

  /**
   * Event triggered when the Proxy configuration element changes.
   */
  private onProxyChange_(event: CustomEvent<ProxySettings>): void {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.proxySettings = event.detail;
    this.setMojoNetworkProperties_(config);
  }

  private propertiesMissingOrBlockedByPolicy_(): boolean {
    return !this.managedProperties_ ||
        this.isBlockedByPolicy_(
            this.managedProperties_, this.globalPolicy,
            this.managedNetworkAvailable);
  }

  private sharedString_(managedProperties: ManagedProperties): string {
    if (!managedProperties.typeProperties.wifi) {
      return this.i18n('networkShared');
    } else if (managedProperties.typeProperties.wifi.isConfiguredByActiveUser) {
      return this.i18n('networkSharedOwner');
    } else {
      return this.i18n('networkSharedNotOwner');
    }
  }

  private syncedString_(managedProperties: ManagedProperties): string {
    if (!managedProperties.typeProperties.wifi) {
      return '';
    } else if (!managedProperties.typeProperties.wifi.isSyncable) {
      return this.i18nAdvanced('networkNotSynced').toString();
    } else if (managedProperties.source === OncSource.kUser) {
      return this.i18nAdvanced('networkSyncedUser').toString();
    } else {
      return this.i18nAdvanced('networkSyncedDevice').toString();
    }
  }

  /**
   * @return Returns 'continuation' class for shared networks.
   */
  private messagesDividerClass_(
      name: string, managedProperties: ManagedProperties,
      globalPolicy: GlobalPolicy, managedNetworkAvailable: boolean,
      isSecondaryUser: boolean, isWifiSyncEnabled: boolean,
      deviceState: OncMojo.DeviceStateProperties|null): string {
    let first = '';
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      first = 'policy';
    } else if (isSecondaryUser) {
      first = 'secondary';
    } else if (this.showShared_(
                   managedProperties, globalPolicy, managedNetworkAvailable,
                   deviceState)) {
      first = 'shared';
    } else if (this.showSynced_(
                   managedProperties, globalPolicy, managedNetworkAvailable,
                   isWifiSyncEnabled)) {
      first = 'synced';
    } else if (isCarrierLockedActiveSim(managedProperties, deviceState)) {
      first = 'carrierlocked';
    }

    return first === name ? 'continuation' : '';
  }

  private showSynced_(
      managedProperties: ManagedProperties, _globalPolicy: GlobalPolicy,
      _managedNetworkAvailable: boolean, isWifiSyncEnabled: boolean): boolean {
    return !this.propertiesMissingOrBlockedByPolicy_() && isWifiSyncEnabled &&
        !!managedProperties.typeProperties.wifi;
  }

  private showShared_(
      managedProperties: ManagedProperties, _globalPolicy: GlobalPolicy,
      _managedNetworkAvailable: boolean,
      deviceState: OncMojo.DeviceStateProperties|null): boolean {
    if (isCarrierLockedActiveSim(managedProperties, deviceState)) {
      return false;
    }

    return !this.propertiesMissingOrBlockedByPolicy_() &&
        (managedProperties.source === OncSource.kDevice ||
         managedProperties.source === OncSource.kDevicePolicy);
  }

  private isCarrierLockedActiveSim_(
      managedProperties: ManagedProperties|undefined,
      deviceState: OncMojo.DeviceStateProperties|null): boolean {
    return isCarrierLockedActiveSim(managedProperties, deviceState);
  }

  private showAutoConnect_(
      managedProperties: ManagedProperties, globalPolicy: GlobalPolicy,
      managedNetworkAvailable: boolean): boolean {
    return !!managedProperties &&
        managedProperties.type !== NetworkType.kEthernet &&
        this.isRemembered_(managedProperties) &&
        !this.isArcVpn_(managedProperties) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  }

  private showHiddenNetworkToggle_(): boolean {
    if (!this.showHiddenToggle_) {
      return false;
    }

    if (!this.managedProperties_) {
      return false;
    }

    if (this.managedProperties_.type !== NetworkType.kWiFi) {
      return false;
    }

    if (!this.isRemembered_(this.managedProperties_)) {
      return false;
    }

    if (this.isBlockedByPolicy_(
            this.managedProperties_, this.globalPolicy,
            this.managedNetworkAvailable)) {
      return false;
    }

    return true;
  }

  private showMetered_(): boolean {
    const managedProperties = this.managedProperties_;
    return this.showMeteredToggle_ && !!managedProperties &&
        this.isRemembered_(managedProperties) &&
        (managedProperties.type === NetworkType.kCellular ||
         managedProperties.type === NetworkType.kWiFi);
  }

  private showAlwaysOnVpn_(managedProperties: ManagedProperties): boolean {
    return this.isArcVpn_(managedProperties) && this.prefs.arc &&
        this.prefs.arc.vpn && this.prefs.arc.vpn.always_on &&
        this.prefs.arc.vpn.always_on.vpn_package &&
        OncMojo.getActiveValue(managedProperties.typeProperties.vpn!.host) ===
        this.prefs.arc.vpn.always_on.vpn_package.value;
  }

  private alwaysOnVpnChanged_(): void {
    if (this.prefs && this.prefs.arc && this.prefs.arc.vpn &&
        this.prefs.arc.vpn.always_on && this.prefs.arc.vpn.always_on.lockdown) {
      this.set(
          'prefs.arc.vpn.always_on.lockdown.value', this.alwaysOnVpn_.value);
    }
  }

  private showPreferNetwork_(
      managedProperties: ManagedProperties, globalPolicy: GlobalPolicy,
      managedNetworkAvailable: boolean): boolean {
    if (!managedProperties) {
      return false;
    }

    const type = managedProperties.type;
    if (type === NetworkType.kEthernet || type === NetworkType.kCellular ||
        this.isArcVpn_(managedProperties)) {
      return false;
    }

    return this.isRemembered_(managedProperties) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  }

  private shouldPreferNetworkToggleBeDisabled_(): boolean {
    return this.disabled_ ||
        this.isNetworkPolicyEnforced(this.managedProperties_!.priority);
  }

  private onPreferNetworkRowClicked_(event: Event): void {
    // Stop propagation because the toggle and policy indicator handle clicks
    // themselves.
    event.stopPropagation();
    const preferNetworkToggle =
        this.shadowRoot!.querySelector<CrToggleElement>('#preferNetworkToggle');
    if (!preferNetworkToggle || preferNetworkToggle.disabled) {
      return;
    }
    this.preferNetwork_ = !this.preferNetwork_;
  }

  private hasVisibleFields_(fields: string[]): boolean {
    for (let i = 0; i < fields.length; ++i) {
      const key = OncMojo.getManagedPropertyKey(fields[i]);
      const value = this.get(key, this.managedProperties_);
      if (value !== undefined && value !== null && value !== '') {
        return true;
      }
    }
    return false;
  }

  private hasInfoFields_(): boolean {
    const editFieldTypes = this.getInfoEditFieldTypes_();
    const infoFields = this.getInfoFields_();
    return Object.keys(editFieldTypes).length > 0 ||
        this.hasVisibleFields_(infoFields);
  }

  private getInfoFields_(): string[] {
    if (!this.managedProperties_) {
      return [];
    }

    const fields: string[] = [];
    switch (this.managedProperties_.type) {
      case NetworkType.kCellular:
        fields.push('cellular.servingOperator.name');
        break;
      case NetworkType.kTether:
        fields.push(
            'tether.batteryPercentage', 'tether.signalStrength',
            'tether.carrier');
        break;
      case NetworkType.kVPN:
        const vpnType = this.managedProperties_.typeProperties.vpn!.type;
        switch (vpnType) {
          case VpnType.kExtension:
            fields.push('vpn.providerName');
            break;
          case VpnType.kArc:
            fields.push('vpn.type');
            fields.push('vpn.providerName');
            break;
          case VpnType.kOpenVPN:
            fields.push(
                'vpn.type', 'vpn.host', 'vpn.openVpn.username',
                'vpn.openVpn.extraHosts');
            break;
          case VpnType.kL2TPIPsec:
            fields.push('vpn.type', 'vpn.host', 'vpn.l2tp.username');
            break;
        }
        break;
      case NetworkType.kWiFi:
        break;
    }
    if (OncMojo.isRestrictedConnectivity(this.managedProperties_.portalState)) {
      fields.push('portalState');
    }
    return fields;
  }

  /**
   * Provides the list of editable fields to <network-property-list>.
   * NOTE: Entries added to this list must be reflected in ConfigProperties in
   * chromeos.network_config.mojom and handled in the service implementation.
   * @return A dictionary of editable fields in the info section.
   */
  private getInfoEditFieldTypes_():
      Record<string, 'String'|'StringArray'|'Password'> {
    if (!this.managedProperties_) {
      return {};
    }

    const editFields: Record<string, 'String'|'StringArray'|'Password'> = {};
    const type = this.managedProperties_.type;
    if (type === NetworkType.kVPN) {
      const vpnType = this.managedProperties_.typeProperties.vpn!.type;
      if (vpnType !== VpnType.kExtension) {
        editFields['vpn.host'] = 'String';
      }
      if (vpnType === VpnType.kOpenVPN) {
        editFields['vpn.openVpn.username'] = 'String';
        editFields['vpn.openVpn.extraHosts'] = 'StringArray';
      }
    }
    return editFields;
  }

  private getAdvancedFields_(): string[] {
    if (!this.managedProperties_) {
      return [];
    }

    const fields: string[] = [];
    const type = this.managedProperties_.type;
    switch (type) {
      case NetworkType.kCellular:
        fields.push('cellular.activationState', 'cellular.networkTechnology');
        break;
      case NetworkType.kWiFi:
        fields.push(
            'wifi.ssid', 'wifi.bssid', 'wifi.signalStrength', 'wifi.security',
            'wifi.eap.outer', 'wifi.eap.inner', 'wifi.eap.domainSuffixMatch',
            'wifi.eap.subjectAltNameMatch', 'wifi.eap.subjectMatch',
            'wifi.eap.identity', 'wifi.eap.anonymousIdentity',
            'wifi.frequency');
        break;
      case NetworkType.kVPN:
        const vpnType = this.managedProperties_.typeProperties.vpn!.type;
        switch (vpnType) {
          case VpnType.kOpenVPN:
            if (this.isManagedByPolicy_()) {
              fields.push(
                  'vpn.openVpn.auth', 'vpn.openVpn.cipher',
                  'vpn.openVpn.compressionAlgorithm',
                  'vpn.openVpn.tlsAuthContents', 'vpn.openVpn.keyDirection');
            }
            break;
        }
        break;
    }
    return fields;
  }

  private getDeviceFields_(): string[] {
    if (!this.managedProperties_ ||
        this.managedProperties_.type !== NetworkType.kCellular) {
      return [];
    }

    const fields: string[] = [];
    const networkState =
        OncMojo.managedPropertiesToNetworkState(this.managedProperties_);
    if (isActiveSim(networkState, this.deviceState_)) {
      // These fields are only known for the SIM in the active slot.
      fields.push(
          'cellular.homeProvider.name', 'cellular.homeProvider.country');
    }
    fields.push(
        'cellular.firmwareRevision', 'cellular.hardwareRevision',
        'cellular.esn', 'cellular.iccid', 'cellular.imei', 'cellular.meid',
        'cellular.min');

    return fields;
  }

  private async computeTrafficCountersAvailable_(): Promise<void> {
    const networks = await this.trafficCountersAdapter_
                         .requestTrafficCountersForActiveNetworks();
    this.trafficCountersAvailable_ = networks.some(n => n.guid === this.guid);
  }

  private showDataUsage_(
      managedProperties: ManagedProperties|undefined,
      trafficCountersAvailable: boolean): boolean {
    if (!this.isTrafficCountersEnabled_) {
      return false;
    }
    if (!managedProperties || this.guid === '') {
      return false;
    }
    if (!this.isCellular_(managedProperties) &&
        !(this.isWiFi_(managedProperties) &&
          this.isTrafficCountersForWifiTestingEnabled_)) {
      return false;
    }
    if (!this.isConnectedState_(managedProperties)) {
      return false;
    }

    return trafficCountersAvailable;
  }

  private hasAdvancedSection_(): boolean {
    if (!this.managedProperties_ || !this.propertiesReceived_) {
      return false;
    }
    if (this.showMetered_()) {
      return true;
    }
    if (this.managedProperties_.type === NetworkType.kTether) {
      // These properties apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    return this.hasAdvancedFields_() || this.hasDeviceFields_();
  }

  private hasAdvancedFields_(): boolean {
    return this.hasVisibleFields_(this.getAdvancedFields_());
  }

  private hasDeviceFields_(): boolean {
    return this.hasVisibleFields_(this.getDeviceFields_());
  }

  private hasNetworkSection_(
      managedProperties: ManagedProperties, globalPolicy: GlobalPolicy,
      managedNetworkAvailable: boolean): boolean {
    if (!managedProperties || managedProperties.type === NetworkType.kTether) {
      // These settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    if (managedProperties.type === NetworkType.kCellular) {
      return true;
    }
    return this.isRememberedOrConnected_(managedProperties);
  }

  private hasProxySection_(
      managedProperties: ManagedProperties, globalPolicy: GlobalPolicy,
      managedNetworkAvailable: boolean): boolean {
    if (!managedProperties || managedProperties.type === NetworkType.kTether) {
      // Proxy settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    return this.isRememberedOrConnected_(managedProperties);
  }

  private isPasspointWifi_(managedProperties: ManagedProperties|
                           undefined): boolean {
    return !!managedProperties &&
        managedProperties.type === NetworkType.kWiFi &&
        managedProperties.typeProperties.wifi!.passpointId !== '' &&
        managedProperties.typeProperties.wifi!.passpointMatchType !==
        MatchType.kNoMatch;
  }

  private shouldShowPasspointProviderRow_(managedProperties: ManagedProperties|
                                          undefined): boolean {
    return this.isPasspointWifi_(managedProperties);
  }

  private getPasspointSubscriptionName_(subscription: PasspointSubscription|
                                        null): string {
    if (!subscription) {
      return '';
    }
    if (subscription.friendlyName && subscription.friendlyName !== '') {
      return subscription.friendlyName;
    }
    return subscription.domains[0];
  }

  private onPasspointRowClicked_(): void {
    const showPasspointEvent = new CustomEvent(
        'show-passpoint-detail',
        {bubbles: true, composed: true, detail: this.passpointSubscription_});
    this.dispatchEvent(showPasspointEvent);
  }

  private onPasspointRemovalDialogConfirm_(): void {
    this.getPasspointRemovalDialog_().close();
    // The removal dialog leads the user to the subscription page.
    this.onPasspointRowClicked_();
  }

  private showCellularChooseNetwork_(managedProperties: ManagedProperties):
      boolean {
    return !!managedProperties &&
        managedProperties.type === NetworkType.kCellular &&
        managedProperties.typeProperties.cellular!.supportNetworkScan;
  }

  private showScanningSpinner_(): boolean {
    if (!this.managedProperties_ ||
        this.managedProperties_.type !== NetworkType.kCellular) {
      return false;
    }
    return !!this.deviceState_ && this.deviceState_.scanning;
  }

  private showCellularSimUpdatedUi_(managedProperties: ManagedProperties):
      boolean {
    return !!managedProperties &&
        managedProperties.type === NetworkType.kCellular &&
        managedProperties.typeProperties.cellular!.family !== 'CDMA';
  }

  private isArcVpn_(managedProperties: ManagedProperties|undefined): boolean {
    return !!managedProperties && managedProperties.type === NetworkType.kVPN &&
        managedProperties.typeProperties.vpn!.type === VpnType.kArc;
  }

  private isThirdPartyVpn_(managedProperties: ManagedProperties|
                           undefined): boolean {
    return !!managedProperties && managedProperties.type === NetworkType.kVPN &&
        managedProperties.typeProperties.vpn!.type === VpnType.kExtension;
  }

  private showIpAddress_(
      ipAddress: string, managedProperties: ManagedProperties): boolean {
    // Arc Vpn does not currently pass IP configuration to ChromeOS. IP address
    // property holds an internal IP address Android uses to talk to ChromeOS.
    // TODO(lgcheng@) Show correct IP address when we implement IP configuration
    // correctly.
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }

    // Cellular IP addresses are shown under the network details section.
    if (this.isCellular_(managedProperties)) {
      return false;
    }

    return !!ipAddress && this.isConnectedState_(managedProperties);
  }

  private isOutOfRangeOrNotEnabled_(
      outOfRange: boolean,
      deviceState: OncMojo.DeviceStateProperties|null): boolean {
    return outOfRange ||
        (!!deviceState && deviceState.deviceState !== DeviceStateType.kEnabled);
  }

  private computeShowConfigurableSections_(): boolean {
    if (!this.managedProperties_ || !this.deviceState_) {
      return true;
    }

    const networkState =
        OncMojo.managedPropertiesToNetworkState(this.managedProperties_);
    assertExists(networkState);
    if (networkState.type !== NetworkType.kCellular) {
      return true;
    }
    return isActiveSim(networkState, this.deviceState_);
  }

  private computeDisabled_(): boolean {
    return shouldDisallowNetworkModifications(
        this.deviceState_, this.managedProperties_);
  }

  private shouldShowMacAddress_(): boolean {
    return !!this.getMacAddress_();
  }

  private getMacAddress_(): string {
    if (!this.deviceState_) {
      return '';
    }

    // 00:00:00:00:00:00 is provided when device MAC address cannot be
    // retrieved.
    const MISSING_MAC_ADDRESS = '00:00:00:00:00:00';
    if (this.deviceState_ && this.deviceState_.macAddress &&
        this.deviceState_.macAddress !== MISSING_MAC_ADDRESS) {
      return this.deviceState_.macAddress;
    }

    return '';
  }

  private isManagedByPolicy_(): boolean {
    return this.managedProperties_!.source === OncSource.kUserPolicy ||
        this.managedProperties_!.source === OncSource.kDevicePolicy;
  }

  private isPortalState_(portalState: PortalState): boolean {
    return portalState === PortalState.kPortal ||
        portalState === PortalState.kPortalSuspected;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsInternetDetailPageElement.is]: SettingsInternetDetailPageElement;
  }
}

customElements.define(
    SettingsInternetDetailPageElement.is, SettingsInternetDetailPageElement);
