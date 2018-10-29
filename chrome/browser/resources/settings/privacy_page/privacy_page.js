// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    /** @private */
    enableSafeBrowsingSubresourceFilter_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
      }
    },

    /** @private */
    enableSoundContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSoundContentSetting');
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
    enableClipboardContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableClipboardContentSetting');
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
    enableSensorsContentSetting_: {
      type: Boolean,
      readOnly: true,
      value: function() {
        return loadTimeData.getBoolean('enableSensorsContentSetting');
      }
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        // <if expr="use_nss_certs">
        if (settings.routes.CERTIFICATES) {
          map.set(
              settings.routes.CERTIFICATES.path,
              '#manageCertificates .subpage-arrow button');
        }
        // </if>
        if (settings.routes.SITE_SETTINGS) {
          map.set(
              settings.routes.SITE_SETTINGS.path,
              '#site-settings-subpage-trigger .subpage-arrow button');
        }

        if (settings.routes.SITE_SETTINGS_SITE_DATA) {
          map.set(
              settings.routes.SITE_SETTINGS_SITE_DATA.path,
              '#site-data-trigger .subpage-arrow button');
        }
        return map;
      },
    },

    /**
     * This flag is used to conditionally show a set of sync UIs to the
     * profiles that have been migrated to have a unified consent flow.
     * TODO(scottchen): In the future when all profiles are completely migrated,
     * this should be removed, and UIs hidden behind it should become default.
     * @private
     */
    unifiedConsentEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('unifiedConsentEnabled');
      },
    },

    // <if expr="not chromeos">
    /** @private */
    showRestart_: Boolean,
    // </if>

    /** @private */
    showSignoutDialog_: Boolean,
  },

  /** @override */
  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;

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
    if (this.showDoNotTrackDialog_)
      this.maybeShowDoNotTrackDialog_();
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
   * Handles the change event for the do-not-track toggle. Shows a
   * confirmation dialog when enabling the setting.
   * @param {!Event} event
   * @private
   */
  onDoNotTrackChange_: function(event) {
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
    if (dialog && !dialog.open)
      dialog.showModal();
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
  },

  /**
   * @param {!Event} e
   * @private
   */
  onMoreSettingsBoxClicked_: function(e) {
    if (e.target.tagName === 'A') {
      e.preventDefault();
      settings.navigateTo(settings.routes.SYNC);
    }
  },

  /**
   * This is a workaround to connect the remove all button to the subpage.
   * @private
   */
  onRemoveAllCookiesFromSite_: function() {
    const node = /** @type {?SiteDataDetailsSubpageElement} */ (
        this.$$('site-data-details-subpage'));
    if (node)
      node.removeAll();
  },

  /** @private */
  onSiteDataTap_: function() {
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
  },

  /** @private */
  onSiteSettingsTap_: function() {
    settings.navigateTo(settings.routes.SITE_SETTINGS);
  },

  /** @private */
  onClearBrowsingDataTap_: function() {
    settings.navigateTo(settings.routes.CLEAR_BROWSER_DATA);
  },

  /** @private */
  onDialogClosed_: function() {
    settings.navigateTo(settings.routes.CLEAR_BROWSER_DATA.parent);
    cr.ui.focusWithoutInk(assert(this.$.clearBrowsingDataTrigger));
  },

  /**
   * The sub-page title for the site or content settings.
   * @return {string}
   * @private
   */
  siteSettingsPageTitle_: function() {
    return loadTimeData.getBoolean('enableSiteSettings') ?
        loadTimeData.getString('siteSettings') :
        loadTimeData.getString('contentSettings');
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
