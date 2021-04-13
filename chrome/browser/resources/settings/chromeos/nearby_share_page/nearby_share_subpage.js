// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!number}
 * @private
 */
const DEFAULT_HIGH_VISIBILITY_TIMEOUT_S = 300;

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

    profileName_: {
      type: String,
      value: '',
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

    /** @private */
    manageContactsUrl_: {
      type: String,
      value: () => loadTimeData.getString('nearbyShareManageContactsUrl')
    },

    /** @private {boolean} */
    inHighVisibility_: {
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
        chromeos.settings.mojom.Setting.kNearbyShareDeviceName,
        chromeos.settings.mojom.Setting.kNearbyShareDeviceVisibility,
        chromeos.settings.mojom.Setting.kNearbyShareContacts,
        chromeos.settings.mojom.Setting.kNearbyShareDataUsage,
      ]),
    },
  },

  listeners: {'onboarding-cancelled': 'onOnboardingCancelled_'},

  /** @private {?nearbyShare.mojom.ReceiveObserverReceiver} */
  receiveObserver_: null,

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

          this.profileName_ = accounts[0].fullName;
          this.profileLabel_ = accounts[0].email;
        });
    this.receiveObserver_ = nearby_share.observeReceiveManager(
        /** @type {!nearbyShare.mojom.ReceiveObserverInterface} */ (this));

    // Trigger a contact sync whenever the Nearby subpage is opened to improve
    // consistency. This should help avoid scenarios where a share is attempted
    // and contacts are stale on the receiver.
    nearby_share.getContactManager().downloadContacts();
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
    this.inHighVisibility_ = false;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onManageContactsTap_(event) {
    window.open(this.manageContactsUrl_);
  },

  /**
   * @private
   * @return {string} Sublabel for manage contacts row.
   */
  getManageContactsSubLabel_() {
    // Remove the protocol part of the contacts url.
    return this.manageContactsUrl_.replace(/(^\w+:|^)\/\//, '');
  },

  /**
   * Mojo callback when high visibility changes.
   * @param {boolean} inHighVisibility
   */
  onHighVisibilityChanged(inHighVisibility) {
    this.inHighVisibility_ = inHighVisibility;
  },

  /**
   * Mojo callback when transfer status changes.
   * @param {!nearbyShare.mojom.ShareTarget} shareTarget
   * @param {!nearbyShare.mojom.TransferMetadata} metadata
   */
  onTransferUpdate(shareTarget, metadata) {
    // Note: Intentionally left empty.
  },

  /**
   * Mojo callback when the Nearby utility process stops.
   */
  onNearbyProcessStopped() {
    this.inHighVisibility_ = false;
  },

  /**
   * Mojo callback when advertising fails to start.
   */
  onStartAdvertisingFailure() {
    this.inHighVisibility_ = false;
  },

  /** @private */
  onInHighVisibilityToggledByUser_() {
    if (this.inHighVisibility_) {
      this.showHighVisibilityPage_();
    }
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
   * @param {boolean} inHighVisibility
   */
  getHighVisibilityToggleText_(inHighVisibility) {
    // TODO(crbug.com/1154830): Add logic to show how much time the user
    // actually has left.
    return inHighVisibility ?
        this.i18n('nearbyShareHighVisibilityOn', 5) :
        this.i18nAdvanced(
            'nearbyShareHighVisibilityOff', {substitutions: ['5']});
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
      this.showHighVisibilityPage_(Number(queryParams.get('timeout')));
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
   * @param {number=} timeoutInSeconds
   * @private
   */
  showHighVisibilityPage_(timeoutInSeconds) {
    const shutoffTimeoutInSeconds =
        timeoutInSeconds || DEFAULT_HIGH_VISIBILITY_TIMEOUT_S;
    this.showReceiveDialog_ = true;
    Polymer.dom.flush();
    this.$$('#receiveDialog').showHighVisibilityPage(shutoffTimeoutInSeconds);
  },

  /**
   * @param {string} profileName The user's full name.
   * @param {string} profileLabel The user's email.
   * @return {string} Localized label.
   * @private
   */
  getAccountRowLabel(profileName, profileLabel) {
    return this.i18n('nearbyShareAccountRowLabel', profileName, profileLabel);
  },

  /** @private */
  getEnabledToggleClassName_() {
    if (this.getPref('nearby_sharing.enabled').value) {
      return 'enabled-toggle-on';
    }
    return 'enabled-toggle-off';
  },

  /** @private */
  onOnboardingCancelled_() {
    // Return to main settings page multidevice section
    settings.Router.getInstance().navigateTo(settings.routes.MULTIDEVICE);
  },
});
