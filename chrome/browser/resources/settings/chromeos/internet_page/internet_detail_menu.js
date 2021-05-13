// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-internet-detail-menu' is a menu that provides
 * additional actions for a network in the network detail page.
 */
Polymer({
  is: 'settings-internet-detail-menu',

  // TODO(crbug.com/1093185): Implement DeepLinkingBehavior and override methods
  // to show the actions for search result.
  behaviors: [
    settings.RouteObserverBehavior,
    ESimManagerListenerBehavior,
  ],

  properties: {
    /**
     * Device state for the network type.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    deviceState: Object,

    /**
     * Null if current network on network detail page is not an eSIM network.
     * @private {?OncMojo.NetworkStateProperties}
     */
    eSimNetworkState_: {
      type: Object,
      value: null,
    },

    /** @private */
    isUpdatedCellularUiEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('updatedCellularActivationUi');
      }
    },

    /** @private */
    isGuest_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isGuest');
      },
    },

    /** @private*/
    guid_: {
      type: String,
      value: '',
    }
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    this.eSimNetworkState_ = null;
    this.guid_ = '';
    if (route !== settings.routes.NETWORK_DETAIL ||
        !this.isUpdatedCellularUiEnabled_) {
      return;
    }

    // Check if the current network is Cellular using the GUID in the
    // current route. We can't use the 'type' parameter in the url
    // directly because Cellular and Tethering share the same subpage and have
    // the same 'type' in the route.
    const queryParams = settings.Router.getInstance().getQueryParameters();
    const guid = queryParams.get('guid') || '';
    if (!guid) {
      console.error('No guid specified for page:' + route);
      return;
    }
    this.guid_ = guid;

    // Needed to set initial eSimNetworkState_.
    this.setESimNetworkState_();
  },

  /**
   * ESimManagerListenerBehavior override
   * @param {!chromeos.cellularSetup.mojom.ESimProfileRemote} profile
   */
  onProfileChanged(profile) {
    this.setESimNetworkState_();
  },

  /**
   * Gets and sets current eSIM network state.
   * @private
   */
  setESimNetworkState_() {
    const networkConfig = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
    networkConfig.getNetworkState(this.guid_).then(response => {
      if (response.result.type !==
              chromeos.networkConfig.mojom.NetworkType.kCellular ||
          !response.result.typeState.cellular.eid ||
          !response.result.typeState.cellular.iccid) {
        return;
      }
      this.eSimNetworkState_ = response.result;
    });
  },

  /**
   * @param {!Event} e
   * @private
   */
  onDotsClick_(e) {
    const menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    menu.showAt(/** @type {!Element} */ (e.target));
  },

  /**
   * @returns {boolean}
   * @private
   */
  shouldShowDotsMenuButton_() {
    // Only shown if the flag is enabled.
    if (!this.isUpdatedCellularUiEnabled_) {
      return false;
    }

    // Not shown in guest mode.
    if (this.isGuest_) {
      return false;
    }

    // Show if |this.eSimNetworkState_| has been fetched. Note that this only
    // occurs if this is a cellular network with an ICCID.
    return !!this.eSimNetworkState_;
  },

  /**
   * @return {boolean}
   * @private
   */
  isDotsMenuButtonDisabled_() {
    if (!this.deviceState || !this.isUpdatedCellularUiEnabled_) {
      return false;
    }
    return OncMojo.deviceIsInhibited(this.deviceState);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRenameESimProfileTap_(e) {
    this.closeMenu_();
    this.fire(
        'show-esim-profile-rename-dialog',
        {networkState: this.eSimNetworkState_});
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRemoveESimProfileTap_(e) {
    this.closeMenu_();
    this.fire(
        'show-esim-remove-profile-dialog',
        {networkState: this.eSimNetworkState_});
  },

  /** @private */
  closeMenu_() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'));
    actionMenu.close();
  },
});