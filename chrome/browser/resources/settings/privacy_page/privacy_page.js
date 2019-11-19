// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * @typedef {{
 *   enabled: boolean,
 *   pref: !chrome.settingsPrivate.PrefObject
 * }}
 */
let BlockAutoplayStatus;

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
(function() {

/**
 * Must be kept in sync with the C++ enum of the same name.
 * @enum {number}
 */
const NetworkPredictionOptions = {
  ALWAYS: 0,
  WIFI_ONLY: 1,
  NEVER: 2,
  DEFAULT: 1,
};

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with enum of the same name in
 * histograms/enums.xml
 *
 * Interactions across all settings pages should be added here.
 */
settings.SettingsPageInteractions = {
  PRIVACY_SYNC_AND_GOOGLE_SERVICES: 0,
  PRIVACY_CHROME_SIGN_IN: 1,
  PRIVACY_DO_NOT_TRACK: 2,
  PRIVACY_PAYMENT_METHOD: 3,
  PRIVACY_NETWORK_PREDICTION: 4,
  PRIVACY_MANAGE_CERTIFICATES: 5,
  PRIVACY_SECURITY_KEYS: 6,
  PRIVACY_SITE_SETTINGS: 7,
  PRIVACY_CLEAR_BROWSING_DATA: 8,
  // Leave this at the end.
  SETTINGS_MAX_VALUE: 8,
};

Polymer({
  is: 'settings-privacy-page',

  behaviors: [
    settings.RouteObserverBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * Dictionary defining page visibility.
     * @type {!PrivacyPageVisibility}
     */
    pageVisibility: Object,

    /** @private */
    isGuest_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isGuest');
      }
    },

    /** @private */
    showClearBrowsingDataDialog_: Boolean,

    /** @private */
    showDoNotTrackDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used for HTML bindings. This is defined as a property rather than within
     * the ready callback, because the value needs to be available before
     * local DOM initialization - otherwise, the toggle has unexpected behavior.
     * @private
     */
    networkPredictionUncheckedValue_: {
      type: Number,
      value: NetworkPredictionOptions.NEVER,
    },

    /** @private */
    enableSafeBrowsingSubresourceFilter_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
      }
    },

    /** @private */
    enableBlockAutoplayContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableBlockAutoplayContentSetting');
      }
    },

    /** @private {BlockAutoplayStatus} */
    blockAutoplayStatus_: {
      type: Object,
      value: function() {
        return /** @type {BlockAutoplayStatus} */ ({});
      }
    },

    /** @private */
    enablePaymentHandlerContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enablePaymentHandlerContentSetting');
      }
    },

    /** @private */
    enableExperimentalWebPlatformFeatures_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures');
      },
    },

    /** @private */
    enableSecurityKeysSubpage_: {
      type: Boolean,
      readOnly: true,
      value: function() {
        return loadTimeData.getBoolean('enableSecurityKeysSubpage');
      }
    },

    /** @private */
    enableInsecureContentContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableInsecureContentContentSetting');
      }
    },

    /** @private */
    enableNativeFileSystemWriteContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean(
            'enableNativeFileSystemWriteContentSetting');
      }
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        // <if expr="use_nss_certs">
        if (settings.routes.CERTIFICATES) {
          map.set(settings.routes.CERTIFICATES.path, '#manageCertificates');
        }
        // </if>
        if (settings.routes.SITE_SETTINGS) {
          map.set(
              settings.routes.SITE_SETTINGS.path,
              '#site-settings-subpage-trigger');
        }

        if (settings.routes.SITE_SETTINGS_SITE_DATA) {
          map.set(
              settings.routes.SITE_SETTINGS_SITE_DATA.path,
              '#site-data-trigger');
        }

        if (settings.routes.SECURITY_KEYS) {
          map.set(
              settings.routes.SECURITY_KEYS.path,
              '#security-keys-subpage-trigger');
        }
        return map;
      },
    },

    // <if expr="not chromeos">
    /** @private */
    showRestart_: Boolean,
    // </if>

    /** @private */
    showSignoutDialog_: Boolean,

    /** @private */
    searchFilter_: String,
  },

  /** @override */
  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
    this.ChooserType = settings.ChooserType;

    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();

    this.onBlockAutoplayStatusChanged_({
      pref: /** @type {chrome.settingsPrivate.PrefObject} */ ({value: false}),
      enabled: false
    });

    this.addWebUIListener(
        'onBlockAutoplayStatusChanged',
        this.onBlockAutoplayStatusChanged_.bind(this));

    settings.SyncBrowserProxyImpl.getInstance().getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    this.syncStatus = syncStatus;
  },

  /** @protected */
  currentRouteChanged: function() {
    this.showClearBrowsingDataDialog_ =
        settings.getCurrentRoute() == settings.routes.CLEAR_BROWSER_DATA;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDoNotTrackDomChange_: function(event) {
    if (this.showDoNotTrackDialog_) {
      this.maybeShowDoNotTrackDialog_();
    }
  },

  /**
   * Called when the block autoplay status changes.
   * @param {BlockAutoplayStatus} autoplayStatus
   * @private
   */
  onBlockAutoplayStatusChanged_: function(autoplayStatus) {
    this.blockAutoplayStatus_ = autoplayStatus;
  },

  /**
   * Updates the block autoplay pref when the toggle is changed.
   * @param {!Event} event
   * @private
   */
  onBlockAutoplayToggleChange_: function(event) {
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    this.browserProxy_.setBlockAutoplayEnabled(target.checked);
  },

  /**
   * Records changes made to the "can a website check if you have saved payment
   * methods" setting for logging, the logic of actually changing the setting
   * is taken care of by the webUI pref.
   * @private
   */
  onCanMakePaymentChange_: function() {
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_PAYMENT_METHOD);
  },

  /**
   * Handles the change event for the do-not-track toggle. Shows a
   * confirmation dialog when enabling the setting.
   * @param {!Event} event
   * @private
   */
  onDoNotTrackChange_: function(event) {
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_DO_NOT_TRACK);
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    if (!target.checked) {
      // Always allow disabling the pref.
      target.sendPrefChange();
      return;
    }
    this.showDoNotTrackDialog_ = true;
    // If the dialog has already been stamped, show it. Otherwise it will be
    // shown in onDomChange_.
    this.maybeShowDoNotTrackDialog_();
  },

  /** @private */
  maybeShowDoNotTrackDialog_: function() {
    const dialog = this.$$('#confirmDoNotTrackDialog');
    if (dialog && !dialog.open) {
      dialog.showModal();
    }
  },

  /** @private */
  closeDoNotTrackDialog_: function() {
    this.$$('#confirmDoNotTrackDialog').close();
    this.showDoNotTrackDialog_ = false;
  },

  /** @private */
  onDoNotTrackDialogClosed_: function() {
    cr.ui.focusWithoutInk(this.$.doNotTrack);
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onDoNotTrackDialogConfirm_: function() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.doNotTrack)
        .sendPrefChange();
    this.closeDoNotTrackDialog_();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onDoNotTrackDialogCancel_: function() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.doNotTrack)
        .resetToPrefValue();
    this.closeDoNotTrackDialog_();
  },

  /** @private */
  onManageCertificatesTap_: function() {
    // <if expr="use_nss_certs">
    settings.navigateTo(settings.routes.CERTIFICATES);
    // </if>
    // <if expr="is_win or is_macosx">
    this.browserProxy_.showManageSSLCertificates();
    // </if>
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_MANAGE_CERTIFICATES);
  },

  /**
   * Records changes made to the network prediction setting for logging, the
   * logic of actually changing the setting is taken care of by the webUI pref.
   * @private
   */
  onNetworkPredictionChange_: function() {
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_NETWORK_PREDICTION);
  },

  /** @private */
  onSyncAndGoogleServicesClick_: function() {
    // Navigate to sync page, and remove (privacy related) search text to
    // avoid the sync page from being hidden.
    settings.navigateTo(settings.routes.SYNC, null, true);
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_SYNC_AND_GOOGLE_SERVICES);
  },

  /**
   * This is a workaround to connect the remove all button to the subpage.
   * @private
   */
  onRemoveAllCookiesFromSite_: function() {
    const node = /** @type {?SiteDataDetailsSubpageElement} */ (
        this.$$('site-data-details-subpage'));
    if (node) {
      node.removeAll();
    }
  },

  /** @private */
  onSiteDataTap_: function() {
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
  },

  /** @private */
  onSiteSettingsTap_: function() {
    settings.navigateTo(settings.routes.SITE_SETTINGS);
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS);
  },

  /** @private */
  onClearBrowsingDataTap_: function() {
    settings.navigateTo(settings.routes.CLEAR_BROWSER_DATA);
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_CLEAR_BROWSING_DATA);
  },

  /** @private */
  onDialogClosed_: function() {
    settings.navigateTo(settings.routes.CLEAR_BROWSER_DATA.parent);
    cr.ui.focusWithoutInk(assert(this.$.clearBrowsingData));
  },

  /** @private */
  onSecurityKeysTap_: function() {
    settings.navigateTo(settings.routes.SECURITY_KEYS);
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_SECURITY_KEYS);
  },

  /** @private */
  getProtectedContentLabel_: function(value) {
    return value ? this.i18n('siteSettingsProtectedContentEnable') :
                   this.i18n('siteSettingsBlocked');
  },

  /** @private */
  getProtectedContentIdentifiersLabel_: function(value) {
    return value ? this.i18n('siteSettingsProtectedContentEnableIdentifiers') :
                   this.i18n('siteSettingsBlocked');
  },

  /** @private */
  onSigninAllowedChange_: function() {
    if (this.syncStatus.signedIn && !this.$.signinAllowedToggle.checked) {
      // Switch the toggle back on and show the signout dialog.
      this.$.signinAllowedToggle.checked = true;
      this.showSignoutDialog_ = true;
    } else {
      /** @type {!SettingsToggleButtonElement} */ (this.$.signinAllowedToggle)
          .sendPrefChange();
      this.showRestart_ = true;
    }
    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_CHROME_SIGN_IN);
  },

  /** @private */
  onSignoutDialogClosed_: function() {
    if (/** @type {!SettingsSignoutDialogElement} */ (
            this.$$('settings-signout-dialog'))
            .wasConfirmed()) {
      this.$.signinAllowedToggle.checked = false;
      /** @type {!SettingsToggleButtonElement} */ (this.$.signinAllowedToggle)
          .sendPrefChange();
      this.showRestart_ = true;
    }
    this.showSignoutDialog_ = false;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRestartTap_: function(e) {
    e.stopPropagation();
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
  },
});
})();
