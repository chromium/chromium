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
  ],

  properties: {
    /**
     * Set in currentRouteChanged() if the network's GUID corresponds
     * with a Cellular eSIM network.
     * @private
     */
    showDotsButton_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    this.showDotsButton_ = false;
    if (route !== settings.routes.NETWORK_DETAIL ||
        !loadTimeData.getBoolean('updatedCellularActivationUi')) {
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
    const networkConfig = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
    networkConfig.getNetworkState(guid).then(response => {
      // TODO(crbug.com/1093185): Add check for specifically eSIM when cellular
      // has an EID property.
      this.showDotsButton_ = response.result.type ===
          chromeos.networkConfig.mojom.NetworkType.kCellular;
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
});