// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-known-networks' is the settings subpage listing the
 * known networks for a type (currently always WiFi).
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../../settings_shared.css.js';
import './internet_shared.css.js';

import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {ConfigProperties, CrosNetworkConfigRemote, FilterType, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {DomRepeatEvent, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {assertExists} from '../assert_extras.js';
import {Constructor} from '../common/types.js';
import {DeepLinkingMixin, DeepLinkingMixinInterface} from '../deep_linking_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './internet_known_networks_page.html.js';

interface SettingsInternetKnownNetworksPageElement {
  $: {
    dotsMenu: CrActionMenuElement,
  };
}

const SettingsInternetKnownNetworksPageElementBase =
    mixinBehaviors(
        [
          NetworkListenerBehavior,
          CrPolicyNetworkBehaviorMojo,
        ],
        DeepLinkingMixin(RouteObserverMixin(I18nMixin(PolymerElement)))) as
    Constructor<PolymerElement&I18nMixinInterface&RouteObserverMixinInterface&
                DeepLinkingMixinInterface&NetworkListenerBehaviorInterface&
                CrPolicyNetworkBehaviorMojoInterface>;

class SettingsInternetKnownNetworksPageElement extends
    SettingsInternetKnownNetworksPageElementBase {
  static get is() {
    return 'settings-internet-known-networks-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The type of networks to list.
       */
      networkType: {
        type: Number,
        observer: 'networkTypeChanged_',
      },

      /**
       * List of all network state data for the network type.
       */
      networkStateList_: {
        type: Array,
        value() {
          return [];
        },
      },

      showAddPreferred_: Boolean,

      showRemovePreferred_: Boolean,

      /**
       * We always show 'Forget' since we do not know whether or not to enable
       * it until we fetch the managed properties, and we do not want an empty
       * menu.
       */
      enableForget_: Boolean,

      /**
       * Contains the settingId of any deep link that wasn't able to be shown,
       * null otherwise.
       */
      pendingSettingId_: {
        type: Number,
        value: null,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kPreferWifiNetwork,
          Setting.kForgetWifiNetwork,
        ]),
      },
    };
  }

  networkType: NetworkType|undefined;
  private enableForget_: boolean;
  private networkConfig_: CrosNetworkConfigRemote;
  private networkStateList_: OncMojo.NetworkStateProperties[];
  private pendingSettingId_: Setting|null;
  private selectedGuid_: string;
  private showAddPreferred_: boolean;
  private showRemovePreferred_: boolean;

  constructor() {
    super();

    this.selectedGuid_ = '';

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * RouteObserverMixin override
   */
  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.KNOWN_NETWORKS) {
      return;
    }

    this.attemptDeepLink().then(result => {
      if (!result.deepLinkShown && result.pendingSettingId) {
        // Store any deep link settingId that wasn't shown so we can try again
        // in refreshNetworks.
        this.pendingSettingId_ = result.pendingSettingId;
      }
    });
  }

  /** CrosNetworkConfigObserver impl */
  override onNetworkStateListChanged(): void {
    this.refreshNetworks_();
  }

  private networkTypeChanged_(): void {
    this.refreshNetworks_();
  }

  /**
   * Requests the list of network states from Chrome. Updates networkStates
   * once the results are returned from Chrome.
   */
  private async refreshNetworks_(): Promise<void> {
    if (this.networkType === undefined) {
      return;
    }
    const filter = {
      filter: FilterType.kConfigured,
      limit: NO_LIMIT,
      networkType: this.networkType,
    };
    const response = await this.networkConfig_.getNetworkStateList(filter);
    this.networkStateList_ = response.result;

    // Check if we have yet to focus a deep-linked element.
    if (!this.pendingSettingId_) {
      return;
    }

    const result = await this.showDeepLink(this.pendingSettingId_);
    if (result.deepLinkShown) {
      this.pendingSettingId_ = null;
    }
  }

  private networkIsPreferred_(networkState: OncMojo.NetworkStateProperties):
      boolean {
    // Currently we treat NetworkStateProperties.Priority as a boolean.
    return networkState.priority > 0;
  }

  private networkIsNotPreferred_(networkState: OncMojo.NetworkStateProperties):
      boolean {
    return networkState.priority === 0;
  }

  private havePreferred_(): boolean {
    return this.networkStateList_.find(
               state => this.networkIsPreferred_(state)) !== undefined;
  }

  private haveNotPreferred_(): boolean {
    return this.networkStateList_.find(
               state => this.networkIsNotPreferred_(state)) !== undefined;
  }

  private getNetworkDisplayName_(networkState: OncMojo.NetworkStateProperties):
      string {
    return OncMojo.getNetworkStateDisplayName(networkState);
  }

  private getEnterpriseIconAriaLabel_(
      networkState: OncMojo.NetworkStateProperties): string {
    return this.i18n(
        'networkA11yManagedByAdministrator',
        this.getNetworkDisplayName_(networkState));
  }

  private async onMenuButtonTap_(
      event: DomRepeatEvent<OncMojo.NetworkStateProperties>): Promise<void> {
    const button = event.target as HTMLButtonElement;
    const networkState = event.model.item;
    this.selectedGuid_ = networkState.guid;
    // We need to make a round trip to Chrome in order to retrieve the managed
    // properties for the network. The delay is not noticeable (~5ms) and is
    // preferable to initiating a query for every known network at load time.
    const response =
        await this.networkConfig_.getManagedProperties(this.selectedGuid_);
    const properties = response.result;
    if (!properties) {
      console.warn('Properties not found for: ' + this.selectedGuid_);
      return;
    }
    if (properties.priority &&
        this.isNetworkPolicyEnforced(properties.priority)) {
      this.showAddPreferred_ = false;
      this.showRemovePreferred_ = false;
    } else {
      const preferred = this.networkIsPreferred_(networkState);
      this.showAddPreferred_ = !preferred;
      this.showRemovePreferred_ = preferred;
    }
    this.enableForget_ = !this.isPolicySource(networkState.source);
    this.$.dotsMenu.showAt(button);
    event.stopPropagation();
  }

  private getMenuButtonTitle_(networkState: OncMojo.NetworkStateProperties):
      string {
    return this.i18n(
        'knownNetworksMenuButtonTitle',
        this.getNetworkDisplayName_(networkState));
  }

  private async setProperties_(config: ConfigProperties): Promise<void> {
    recordSettingChange();
    const response =
        await this.networkConfig_.setProperties(this.selectedGuid_, config);
    if (!response.success) {
      console.warn(
          'Unable to set properties for: ' + this.selectedGuid_ + ': ' +
          JSON.stringify(config));
    }
  }

  private onRemovePreferredTap_(): void {
    assertExists(this.networkType);
    const config = OncMojo.getDefaultConfigProperties(this.networkType);
    config.priority = {value: 0};
    this.setProperties_(config);
    this.$.dotsMenu.close();
  }

  private onAddPreferredTap_(): void {
    assertExists(this.networkType);
    const config = OncMojo.getDefaultConfigProperties(this.networkType);
    config.priority = {value: 1};
    this.setProperties_(config);
    this.$.dotsMenu.close();
  }

  private async onForgetTap_(): Promise<void> {
    if (this.networkType === NetworkType.kWiFi) {
      recordSettingChange(Setting.kForgetWifiNetwork);
    } else {
      recordSettingChange();
    }

    this.$.dotsMenu.close();

    const response =
        await this.networkConfig_.forgetNetwork(this.selectedGuid_);
    if (!response.success) {
      console.warn('Forget network failed for: ' + this.selectedGuid_);
    }
    this.refreshNetworks_();
  }

  /**
   * Fires a 'show-detail' event with an item containing a |networkStateList_|
   * entry in the event model.
   */
  private fireShowDetails_(
      event: DomRepeatEvent<OncMojo.NetworkStateProperties>): void {
    const networkState = event.model.item;
    const showDetailEvent = new CustomEvent(
        'show-detail', {bubbles: true, composed: true, detail: networkState});
    this.dispatchEvent(showDetailEvent);
    event.stopPropagation();
  }

  /**
   * Make sure events in embedded components do not propagate to onDetailsTap_.
   */
  private doNothing_(event: Event): void {
    event.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsInternetKnownNetworksPageElement.is]:
        SettingsInternetKnownNetworksPageElement;
  }
}

customElements.define(
    SettingsInternetKnownNetworksPageElement.is,
    SettingsInternetKnownNetworksPageElement);
