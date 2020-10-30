// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-known-networks' is the settings subpage listing the
 * known networks for a type (currently always WiFi).
 */
Polymer({
  is: 'settings-internet-known-networks-page',

  behaviors: [
    DeepLinkingBehavior,
    NetworkListenerBehavior,
    CrPolicyNetworkBehaviorMojo,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * The type of networks to list.
     * @type {chromeos.networkConfig.mojom.NetworkType|undefined}
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
      }
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
     * @private {?chromeos.settings.mojom.Setting}
     */
    pendingSettingId_: {
      type: Number,
      value: null,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kPreferWifiNetwork,
        chromeos.settings.mojom.Setting.kForgetWifiNetwork,
      ]),
    },
  },

  /** @private {string} */
  selectedGuid_: '',

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.KNOWN_NETWORKS) {
      return;
    }

    this.attemptDeepLink().then(result => {
      if (!result.deepLinkShown && result.pendingSettingId) {
        // Store any deep link settingId that wasn't shown so we can try again
        // in refreshNetworks.
        this.pendingSettingId_ = result.pendingSettingId;
      }
    });
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged() {
    this.refreshNetworks_();
  },

  /** @private */
  networkTypeChanged_() {
    this.refreshNetworks_();
  },

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
      filter: chromeos.networkConfig.mojom.FilterType.kConfigured,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
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
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @return {boolean}
   * @private
   */
  networkIsPreferred_(networkState) {
    // Currently we treat NetworkStateProperties.Priority as a boolean.
    return networkState.priority > 0;
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @return {boolean}
   * @private
   */
  networkIsNotPreferred_(networkState) {
    return networkState.priority === 0;
  },

  /**
   * @return {boolean}
   * @private
   */
  havePreferred_() {
    return this.networkStateList_.find(
               state => this.networkIsPreferred_(state)) !== undefined;
  },

  /**
   * @return {boolean}
   * @private
   */
  haveNotPreferred_() {
    return this.networkStateList_.find(
               state => this.networkIsNotPreferred_(state)) !== undefined;
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @return {string}
   * @private
   */
  getNetworkDisplayName_(networkState) {
    return OncMojo.getNetworkStateDisplayName(networkState);
  },

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
            console.error('Properties not found for: ' + this.selectedGuid_);
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
              .showAt(/** @type {!Element} */ (button));
        });
    event.stopPropagation();
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ConfigProperties} config
   * @private
   */
  setProperties_(config) {
    this.networkConfig_.setProperties(this.selectedGuid_, config)
        .then(response => {
          if (!response.success) {
            console.error(
                'Unable to set properties for: ' + this.selectedGuid_ + ': ' +
                JSON.stringify(config));
          }
        });
    settings.recordSettingChange();
  },

  /** @private */
  onRemovePreferredTap_() {
    assert(this.networkType !== undefined);
    const config = OncMojo.getDefaultConfigProperties(this.networkType);
    config.priority = {value: 0};
    this.setProperties_(config);
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /** @private */
  onAddPreferredTap_() {
    assert(this.networkType !== undefined);
    const config = OncMojo.getDefaultConfigProperties(this.networkType);
    config.priority = {value: 1};
    this.setProperties_(config);
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /** @private */
  onForgetTap_() {
    this.networkConfig_.forgetNetwork(this.selectedGuid_).then(response => {
      if (!response.success) {
        console.error('Froget network failed for: ' + this.selectedGuid_);
      }
    });
    settings.recordSettingChange();
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /**
   * Fires a 'show-detail' event with an item containing a |networkStateList_|
   * entry in the event model.
   * @param {!Event} event
   * @private
   */
  fireShowDetails_(event) {
    const networkState =
        /** @type {!OncMojo.NetworkStateProperties} */ (event.model.item);
    this.fire('show-detail', networkState);
    event.stopPropagation();
  },

  /**
   * Make sure events in embedded components do not propagate to onDetailsTap_.
   * @param {!Event} event
   * @private
   */
  doNothing_(event) {
    event.stopPropagation();
  },
});
