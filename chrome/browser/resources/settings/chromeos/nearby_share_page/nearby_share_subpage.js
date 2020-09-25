// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-nearby-share-subpage' is the settings subpage for managing the
 * Nearby Share feature.
 */
Polymer({
  is: 'settings-nearby-share-subpage',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
    nearby_share.NearbyShareSettingsBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    profileLabel_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    showDeviceNameDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showVisibilityDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showDataUsageDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showReceiveDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kNearbyShareOnOff,
      ]),
    },
  },

  attached() {
    // TODO(b/166779043): Check whether the Account Manager is enabled and fall
    // back to profile name, or just hide the row. This is not urgent because
    // the Account Manager should be available whenever Nearby Share is enabled.
    nearby_share.NearbyAccountManagerBrowserProxyImpl.getInstance()
        .getAccounts()
        .then(accounts => {
          if (accounts.length === 0) {
            return;
          }

          this.profileLabel_ = accounts[0].email;
        });
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_(event) {
    this.setPrefValue(
        'nearby_sharing.enabled',
        !this.getPref('nearby_sharing.enabled').value);
    event.stopPropagation();
  },

  /** @private */
  onDeviceNameTap_() {
    if (this.showDeviceNameDialog_) {
      return;
    }
    this.showDeviceNameDialog_ = true;
  },

  /** @private */
  onVisibilityTap_() {
    this.showVisibilityDialog_ = true;
  },

  /** @private */
  onDataUsageTap_() {
    this.showDataUsageDialog_ = true;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDeviceNameDialogClose_(event) {
    this.showDeviceNameDialog_ = false;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onVisibilityDialogClose_(event) {
    this.showVisibilityDialog_ = false;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDataUsageDialogClose_(event) {
    this.showDataUsageDialog_ = false;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onReceiveDialogClose_(event) {
    this.showReceiveDialog_ = false;
  },

  /**
   * @param {boolean} state boolean state that determines which string to show
   * @param {string} onstr string to show when state is true
   * @param {string} offstr string to show when state is false
   * @return {string} localized string
   * @private
   */
  getOnOffString_(state, onstr, offstr) {
    return state ? onstr : offstr;
  },

  /**
   * @param {string} name name of device
   * @return {string} localized string
   * @private
   */
  getEditNameButtonAriaDescription_(name) {
    return this.i18n('nearbyShareDeviceNameAriaDescription', name);
  },

  /**
   * @param {nearbyShare.mojom.Visibility} visibility
   * @return {string} localized visibility string
   * @private
   */
  getVisibilityText_(visibility) {
    switch (visibility) {
      case nearbyShare.mojom.Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAll');
      case nearbyShare.mojom.Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySome');
      case nearbyShare.mojom.Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNone');
      case nearbyShare.mojom.Visibility.kUnknown:
        return this.i18n('nearbyShareContactVisibilityUnknown');
      default:
        return '';  // Make closure happy.
    }
  },

  /**
   * @param {nearbyShare.mojom.Visibility} visibility
   * @return {string} localized visibility description string
   * @private
   */
  getVisibilityDescription_(visibility) {
    switch (visibility) {
      case nearbyShare.mojom.Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAllDescription');
      case nearbyShare.mojom.Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySomeDescription');
      case nearbyShare.mojom.Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNoneDescription');
      case nearbyShare.mojom.Visibility.kUnknown:
        return this.i18n('nearbyShareContactVisibilityUnknownDescription');
      default:
        return '';  // Make closure happy.
    }
  },

  /**
   * @param {string} dataUsageValue enum value of data usage setting.
   * @return {string} localized string
   * @private
   */
  getDataUsageLabel_(dataUsageValue) {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataLabel');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
      return this.i18n('nearbyShareDataUsageOfflineLabel');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyLabel');
    }
  },

  /**
   * @param {string} dataUsageValue enum value of data usage setting.
   * @return {string} localized string
   * @private
   */
  getDataUsageSubLabel_(dataUsageValue) {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataDescription');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
      return this.i18n('nearbyShareDataUsageOfflineDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyDescription');
    }
  },

  /**
   * @param {string} dataUsageValue enum value of data usage setting.
   * @return {string} localized string
   * @private
   */
  getEditDataUsageButtonAriaDescription_(dataUsageValue) {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataEditButtonDescription');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
      return this.i18n('nearbyShareDataUsageOfflineEditButtonDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyEditButtonDescription');
    }
  },

  /**
   * @param {!settings.Route} route
   */
  currentRouteChanged(route) {
    // Does not apply to this page.
    if (route !== settings.routes.NEARBY_SHARE) {
      return;
    }

    const router = settings.Router.getInstance();
    const queryParams = router.getQueryParameters();

    if (queryParams.has('deviceName')) {
      this.showDeviceNameDialog_ = true;
    }

    if (queryParams.has('receive')) {
      this.showReceiveDialog_ = true;
      Polymer.dom.flush();
      this.$$('#receiveDialog').showHighVisibilityPage();
    }

    if (queryParams.has('stop_receiving')) {
      this.showReceiveDialog_ = false;
    }

    if (queryParams.has('confirm')) {
      this.showReceiveDialog_ = true;
      Polymer.dom.flush();
      this.$$('#receiveDialog').showConfirmPage();
    }

    if (queryParams.has('onboarding')) {
      this.showReceiveDialog_ = true;
      Polymer.dom.flush();
      this.$$('#receiveDialog').showOnboarding();
    }

    this.attemptDeepLink();
  },

  /**
   * @param {string} deviceName Customizable name of the device.
   * @param {string} profileLabel The user's email.
   * @return {string} Localized label.
   * @private
   */
  getAccountRowLabel(deviceName, profileLabel) {
    return this.i18n('nearbyShareAccountRowLabel', profileLabel, deviceName);
  },
});
