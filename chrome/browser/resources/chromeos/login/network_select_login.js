// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping network-select for login/oobe.
 */

{
  const mojom = chromeos.networkConfig.mojom;

  /**
   * Custom data that is stored with network element to trigger action.
   * @typedef {{onTap: !function()}}
   */
  let networkCustomItemCustomData;

  Polymer({
    is: 'network-select-login',

    properties: {
      /**
         Whether network selection is shown as a part of offline demo mode setup
         flow.
       */
      isOfflineDemoModeSetup: {
        type: Boolean,
        value: false,
        observer: 'onIsOfflineDemoModeSetupChanged_',
      },

      /**
       * True when connected to a network.
       * @private
       */
      isConnected: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * If true, when a connected network is selected the configure UI will be
       * requested instead of sending 'userActed' + 'continue'.
       * @private
       */
      configureConnected: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether to show technology badge on mobile network icons.
       * @private
       */
      showTechnologyBadge_: {
        type: Boolean,
        value: function() {
          return loadTimeData.valueExists('showTechnologyBadge') &&
              loadTimeData.getBoolean('showTechnologyBadge');
        }
      }
    },

    /**
     * GUID of the user-selected network. It is remembered after user taps on
     * network entry. After we receive event "connected" on this network,
     * OOBE will proceed.
     * @private {string}
     */
    networkLastSelectedGuid_: '',

    /**
     * Flag that ensures that OOBE configuration is applied only once.
     * @private {boolean}
     */
    configuration_applied_: false,

    /**
     * Flag that reflects if this element is currently shown.
     * @private {boolean}
     */
    is_shown_: false,

    /** Refreshes the list of the networks. */
    refresh: function() {
      /** @type {!NetworkSelectElement} */ (this.$.networkSelect)
          .refreshNetworks();
      this.networkLastSelectedGuid_ = '';
    },

    focus: function() {
      this.$.networkSelect.focus();
    },

    /** Called when dialog is shown. */
    onBeforeShow: function() {
      this.is_shown_ = true;
      this.attemptApplyConfiguration_();
    },

    /** Called when dialog is hidden. */
    onBeforeHide: function() {
      this.is_shown_ = false;
    },

    /**
     * Call after strings are loaded to set CrOncStrings for network-select.
     */
    setOncStrings: function() {
      CrOncStrings = {
        OncTypeCellular: loadTimeData.getString('OncTypeCellular'),
        OncTypeEthernet: loadTimeData.getString('OncTypeEthernet'),
        OncTypeMobile: loadTimeData.getString('OncTypeMobile'),
        OncTypeTether: loadTimeData.getString('OncTypeTether'),
        OncTypeVPN: loadTimeData.getString('OncTypeVPN'),
        OncTypeWiFi: loadTimeData.getString('OncTypeWiFi'),
        networkListItemConnected:
            loadTimeData.getString('networkListItemConnected'),
        networkListItemConnecting:
            loadTimeData.getString('networkListItemConnecting'),
        networkListItemConnectingTo:
            loadTimeData.getString('networkListItemConnectingTo'),
        networkListItemInitializing:
            loadTimeData.getString('networkListItemInitializing'),
        networkListItemLabelTemplate:
            loadTimeData.getString('networkListItemLabelTemplate'),
        networkListItemNotAvailable:
            loadTimeData.getString('networkListItemNotAvailable'),
        networkListItemScanning:
            loadTimeData.getString('networkListItemScanning'),
        networkListItemSimCardLocked:
            loadTimeData.getString('networkListItemSimCardLocked'),
        networkListItemNotConnected:
            loadTimeData.getString('networkListItemNotConnected'),
        networkListItemNoNetwork:
            loadTimeData.getString('networkListItemNoNetwork'),
        vpnNameTemplate: loadTimeData.getString('vpnNameTemplate'),

        // Additional strings for custom items.
        addWiFiListItemName: loadTimeData.getString('addWiFiListItemName'),
        proxySettingsListItemName:
            loadTimeData.getString('proxySettingsListItemName'),
        offlineDemoSetupListItemName:
            loadTimeData.getString('offlineDemoSetupListItemName'),
      };
    },

    /**
     * Returns custom items for network selector. Shows 'Proxy settings' only
     * when connected to a network.
     * @private
     */
    getNetworkCustomItems_: function() {
      var self = this;
      var items = [];
      if (this.isOfflineDemoModeSetup) {
        items.push({
          customItemName: 'offlineDemoSetupListItemName',
          polymerIcon: 'oobe-network-20:offline-demo-setup',
          showBeforeNetworksList: true,
          customData: {
            onTap: this.onOfflineDemoSetupClicked_.bind(this),
          },
        });
      }
      if (this.isConnected) {
        items.push({
          customItemName: 'proxySettingsListItemName',
          polymerIcon: 'oobe-network-20:add-proxy',
          showBeforeNetworksList: false,
          customData: {
            onTap: this.openInternetDetailDialog_.bind(this),
          },
        });
      }
      items.push({
        customItemName: 'addWiFiListItemName',
        polymerIcon: 'oobe-network-20:add-wifi',
        showBeforeNetworksList: false,
        customData: {
          onTap: this.openAddWiFiNetworkDialog_.bind(this),
        },
      });
      return items;
    },

    /**
     * Handle Network Setup screen "Proxy settings" button.
     *
     * @private
     */
    openInternetDetailDialog_: function(item) {
      chrome.send('launchInternetDetailDialog');
    },

    /**
     * Handle Network Setup screen "Add WiFi network" button.
     *
     * @private
     */
    openAddWiFiNetworkDialog_: function(item) {
      chrome.send('launchAddWiFiNetworkDialog');
    },

    /**
     * Offline demo setup button handler.
     * @private
     */
    onOfflineDemoSetupClicked_: function(item) {
      chrome.send('login.NetworkScreen.userActed', ['offline-demo-setup']);
    },

    /**
     * Called when network setup is done. Notifies parent that network setup is
     * done.
     * @private
     */
    onSelectedNetworkConnected_: function() {
      this.networkLastSelectedGuid_ = '';
      this.fire('selected-network-connected');
    },

    /**
     * Event triggered when the default network state may have changed.
     * @param {!CustomEvent<OncMojo.NetworkStateProperties>} event
     * @private
     */
    onDefaultNetworkChanged_: function(event) {
      // Note: event.detail will be {} if there is no default network.
      var networkState = event.detail.type ? event.detail : undefined;
      this.isConnected = !!networkState &&
          OncMojo.connectionStateIsConnected(networkState.connectionState);
      if (!this.isConnected || !this.is_shown_)
        return;
      this.attemptApplyConfiguration_();
    },

    /**
     * Event triggered when a network-list-item connection state changes.
     * @param {!CustomEvent<!OncMojo.NetworkStateProperties>} event
     * @private
     */
    onNetworkConnectChanged_: function(event) {
      var networkState = event.detail;
      if (networkState && networkState.guid == this.networkLastSelectedGuid_ &&
          OncMojo.connectionStateIsConnected(networkState.connectionState)) {
        this.onSelectedNetworkConnected_();
      }
    },

    /**
     * Event triggered when a list of networks get changed.
     * @param {!CustomEvent<!Array<!OncMojo.NetworkStateProperties>>} event
     * @private
     */
    onNetworkListChanged_: function(event) {
      if (!this.is_shown_)
        return;
      this.attemptApplyConfiguration_();
    },

    /**
     * Tries to apply OOBE configuration on current list of networks.
     * @private
     */
    attemptApplyConfiguration_: function() {
      if (this.configuration_applied_)
        return;
      var configuration = Oobe.getInstance().getOobeConfiguration();
      if (!configuration)
        return;
      if (configuration.networkOfflineDemo && this.isOfflineDemoModeSetup) {
        window.setTimeout(this.onOfflineDemoSetupClicked_.bind(this), 0);
        this.configuration_applied_ = true;
        return;
      }
      var defaultNetwork = this.$.networkSelect.getDefaultNetwork();
      if (configuration.networkUseConnected && defaultNetwork &&
          OncMojo.connectionStateIsConnected(defaultNetwork.connectionState)) {
        window.setTimeout(
            this.handleNetworkSelection_.bind(this, defaultNetwork), 0);
        this.configuration_applied_ = true;
        return;
      }
      if (configuration.networkSelectGuid) {
        var network =
            this.$.networkSelect.getNetwork(configuration.networkSelectGuid);
        if (network) {
          window.setTimeout(
              this.handleNetworkSelection_.bind(this, network), 0);
          this.configuration_applied_ = true;
          return;
        }
      }
    },

    /**
     * This is called when user taps on network entry in networks list.
     * @param {!CustomEvent<!OncMojo.NetworkStateProperties>} event
     * @private
     */
    onNetworkListNetworkItemSelected_: function(event) {
      this.handleNetworkSelection_(event.detail);
    },

    /**
     * Handles selection of particular network.
     * @param {!OncMojo.NetworkStateProperties} networkState
     * @private
     */
    handleNetworkSelection_: function(networkState) {
      assert(networkState);

      var isConnected =
          OncMojo.connectionStateIsConnected(networkState.connectionState);

      // If |configureConnected| is false and a connected network is selected,
      // continue to the next screen.
      if (!this.configureConnected && isConnected) {
        this.onSelectedNetworkConnected_();
        return;
      }

      // If user has previously selected another network, there
      // is pending connection attempt. So even if new selection is currently
      // connected, it may get disconnected at any time.
      // So just send one more connection request to cancel current attempts.
      this.networkLastSelectedGuid_ = networkState.guid;

      var self = this;
      var oncType = OncMojo.getNetworkTypeString(networkState.type);
      var guid = networkState.guid;

      var shouldShowNetworkDetails = isConnected ||
          networkState.connectionState ==
              chromeos.networkConfig.mojom.ConnectionStateType.kConnecting;
      // Cellular should normally auto connect. If it is selected, show the
      // details UI since there is no configuration UI for Cellular.
      shouldShowNetworkDetails |= networkState.type ==
          chromeos.networkConfig.mojom.NetworkType.kCellular;

      if (shouldShowNetworkDetails) {
        chrome.send('showNetworkDetails', [oncType, guid]);
        return;
      }

      if (!networkState.connectable || networkState.errorState) {
        chrome.send('showNetworkConfig', [guid]);
        return;
      }

      const networkConfig =
          network_config.MojoInterfaceProviderImpl.getInstance()
              .getMojoServiceRemote();

      networkConfig.startConnect(guid).then(response => {
        switch (response.result) {
          case mojom.StartConnectResult.kSuccess:
            return;
          case mojom.StartConnectResult.kInvalidGuid:
          case mojom.StartConnectResult.kInvalidState:
          case mojom.StartConnectResult.kCanceled:
            // TODO(stevenjb/khorimoto): Consider handling these cases.
            return;
          case mojom.StartConnectResult.kNotConfigured:
            if (!OncMojo.networkTypeIsMobile(networkState.type)) {
              chrome.send('showNetworkConfig', [guid]);
            } else {
              console.error('Cellular network is not configured: ' + guid);
            }
            return;
          case mojom.StartConnectResult.kBlocked:
          case mojom.StartConnectResult.kUnknown:
            console.error(
                'startConnect failed for: ' + guid + ': ' + response.message);
            return;
        }
      });
    },

    /**
     * @param {!CustomEvent<{customData:!networkCustomItemCustomData}>} event
     * @private
     */
    onNetworkListCustomItemSelected_: function(event) {
      var itemState = event.detail;
      itemState.customData.onTap();
    },

    /**
     * Updates custom items when property that indicates if dialog is shown as a
     * part of offline demo mode setup changes.
     * @private
     */
    onIsOfflineDemoModeSetupChanged_: function() {
      this.$.networkSelect.customItems = this.getNetworkCustomItems_();
    },
  });
}
