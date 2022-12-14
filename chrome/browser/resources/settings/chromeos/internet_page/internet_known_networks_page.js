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

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ConfigProperties, CrosNetworkConfigRemote, FilterType, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {Route} from '../router.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {NetworkListenerBehaviorInterface}
 * @implements {CrPolicyNetworkBehaviorMojoInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsInternetKnownNetworksPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      NetworkListenerBehavior,
      CrPolicyNetworkBehaviorMojo,
      RouteObserverBehavior,
      I18nBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsInternetKnownNetworksPageElement extends
    SettingsInternetKnownNetworksPageElementBase {
  static get is() {
    return 'settings-internet-known-networks-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The type of networks to list.
       * @type {NetworkType|undefined}
       */
      networkType: {
        type: Number,
        observer: 'networkTypeChanged_',
      },

      /**
       * List of all network state data for the network type.
       * @private {!Array<!OncMojo.NetworkStateProperties>}
       */
      networkStateList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** @private */
      showAddPreferred_: Boolean,

      /** @private */
      showRemovePreferred_: Boolean,

      /**
       * We always show 'Forget' since we do not know whether or not to enable
       * it until we fetch the managed properties, and we do not want an empty
       * menu.
       * @private
       */
      enableForget_: Boolean,

      /**
       * Contains the settingId of any deep link that wasn't able to be shown,
       * null otherwise.
       * @private {?Setting}
       */
      pendingSettingId_: {
        type: Number,
        value: null,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kPreferWifiNetwork,
          Setting.kForgetWifiNetwork,
        ]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {string} */
    this.selectedGuid_ = '';

    /** @private {!CrosNetworkConfigRemote} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
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
  onNetworkStateListChanged() {
    this.refreshNetworks_();
  }

  /** @private */
  networkTypeChanged_() {
    this.refreshNetworks_();
  }

  /**
   * Requests the list of network states from Chrome. Updates networkStates
   * once the results are returned from Chrome.
   * @private
   */
  refreshNetworks_() {
    if (this.networkType === undefined) {
      return;
    }
    const filter = {
      filter: FilterType.kConfigured,
      limit: NO_LIMIT,
      networkType: this.networkType,
    };
    this.networkConfig_.getNetworkStateList(filter).then(response => {
      this.networkStateList_ = response.result;

      // Check if we have yet to focus a deep-linked element.
      if (!this.pendingSettingId_) {
        return;
      }

      this.showDeepLink(this.pendingSettingId_).then(result => {
        if (result.deepLinkShown) {
          this.pendingSettingId_ = null;
        }
      });
    });
  }

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @return {boolean}
   * @private
   */
  networkIsPreferred_(networkState) {
    // Currently we treat NetworkStateProperties.Priority as a boolean.
    return networkState.priority > 0;
  }

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @return {boolean}
   * @private
   */
  networkIsNotPreferred_(networkState) {
    return networkState.priority === 0;
  }

  /**
   * @return {boolean}
   * @private
   */
  havePreferred_() {
    return this.networkStateList_.find(
               state => this.networkIsPreferred_(state)) !== undefined;
  }

  /**
   * @return {boolean}
   * @private
   */
  haveNotPreferred_() {
    return this.networkStateList_.find(
               state => this.networkIsNotPreferred_(state)) !== undefined;
  }

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @return {string}
   * @private
   */
  getNetworkDisplayName_(networkState) {
    return OncMojo.getNetworkStateDisplayName(networkState);
  }

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @return {string}
   * @private
   */
  getEnterpriseIconAriaLabel_(networkState) {
    return this.i18n(
        'networkA11yManagedByAdministrator',
        this.getNetworkDisplayName_(networkState));
  }

  /**
   * @param {!Event} event
   * @private
   */
  onMenuButtonTap_(event) {
    const button = event.target;
    const networkState =
        /** @type {!OncMojo.NetworkStateProperties} */ (event.model.item);
    this.selectedGuid_ = networkState.guid;
    // We need to make a round trip to Chrome in order to retrieve the managed
    // properties for the network. The delay is not noticeable (~5ms) and is
    // preferable to initiating a query for every known network at load time.
    this.networkConfig_.getManagedProperties(this.selectedGuid_)
        .then(response => {
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
          /** @type {!CrActionMenuElement} */ (this.$.dotsMenu)
              .showAt(/** @type {!HTMLElement} */ (button));
        });
    event.stopPropagation();
  }

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @return {string}
   * @protected
   */
  getMenuButtonTitle_(networkState) {
    return this.i18n(
        'knownNetworksMenuButtonTitle',
        this.getNetworkDisplayName_(networkState));
  }

  /**
   * @param {!ConfigProperties} config
   * @private
   */
  setProperties_(config) {
    this.networkConfig_.setProperties(this.selectedGuid_, config)
        .then(response => {
          if (!response.success) {
            console.warn(
                'Unable to set properties for: ' + this.selectedGuid_ + ': ' +
                JSON.stringify(config));
          }
        });
    recordSettingChange();
  }

  /** @private */
  onRemovePreferredTap_() {
    assert(this.networkType !== undefined);
    const config = OncMojo.getDefaultConfigProperties(this.networkType);
    config.priority = {value: 0};
    this.setProperties_(config);
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  }

  /** @private */
  onAddPreferredTap_() {
    assert(this.networkType !== undefined);
    const config = OncMojo.getDefaultConfigProperties(this.networkType);
    config.priority = {value: 1};
    this.setProperties_(config);
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  }

  /** @private */
  onForgetTap_() {
    this.networkConfig_.forgetNetwork(this.selectedGuid_).then(response => {
      if (!response.success) {
        console.warn('Forget network failed for: ' + this.selectedGuid_);
      }
      this.refreshNetworks_();
    });

    if (this.networkType === NetworkType.kWiFi) {
      recordSettingChange(Setting.kForgetWifiNetwork);
    } else {
      recordSettingChange();
    }

    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  }

  /**
   * Fires a 'show-detail' event with an item containing a |networkStateList_|
   * entry in the event model.
   * @param {!Event} event
   * @private
   */
  fireShowDetails_(event) {
    const networkState =
        /** @type {!OncMojo.NetworkStateProperties} */ (event.model.item);

    const showDetailEvent = new CustomEvent(
        'show-detail', {bubbles: true, composed: true, detail: networkState});
    this.dispatchEvent(showDetailEvent);
    event.stopPropagation();
  }

  /**
   * Make sure events in embedded components do not propagate to onDetailsTap_.
   * @param {!Event} event
   * @private
   */
  doNothing_(event) {
    event.stopPropagation();
  }
}

customElements.define(
    SettingsInternetKnownNetworksPageElement.is,
    SettingsInternetKnownNetworksPageElement);
