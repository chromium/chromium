// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './collapse_radio_button.js';
import './disable_safebrowsing_dialog.js';
import './secure_dns.js';
import '../controls/settings_toggle_button.js';
import '../icons.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyElementInteractions, SafeBrowsingInteractions} from '../metrics_browser_proxy.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';

import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from './privacy_page_browser_proxy.js';

/**
 * Enumeration of all safe browsing modes. Must be kept in sync with the enum
 * of the same name located in:
 * chrome/browser/safe_browsing/generated_safe_browsing_pref.h
 * @enum {number}
 */
export const SafeBrowsingSetting = {
  ENHANCED: 0,
  STANDARD: 1,
  DISABLED: 2,
};

Polymer({
  is: 'settings-security-page',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
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
     * Whether the secure DNS setting should be displayed.
     * @private
     */
    showSecureDnsSetting_: {
      type: Boolean,
      readOnly: true,
      value: function() {
        return loadTimeData.getBoolean('showSecureDnsSetting');
      },
    },

    /**
     * Valid safe browsing states.
     * @private
     */
    safeBrowsingSettingEnum_: {
      type: Object,
      value: SafeBrowsingSetting,
    },

    /** @private */
    enableSecurityKeysSubpage_: {
      type: Boolean,
      readOnly: true,
      value() {
        return loadTimeData.getBoolean('enableSecurityKeysSubpage');
      }
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },

    /** @private */
    showDisableSafebrowsingDialog_: Boolean,
  },

  /*
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    assert(!oldConfig);
    // <if expr="use_nss_certs">
    if (routes.CERTIFICATES) {
      this.focusConfig.set(routes.CERTIFICATES.path, () => {
        focusWithoutInk(assert(this.$$('#manageCertificates')));
      });
    }
    // </if>

    if (routes.SECURITY_KEYS) {
      this.focusConfig.set(routes.SECURITY_KEYS.path, () => {
        focusWithoutInk(assert(this.$$('#security-keys-subpage-trigger')));
      });
    }
  },

  /** @private {PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @private {MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  ready() {
    // Expand initial pref value manually because automatic
    // expanding is disabled.
    const prefValue = this.getPref('generated.safe_browsing').value;
    if (prefValue === SafeBrowsingSetting.ENHANCED) {
      this.$.safeBrowsingEnhanced.expanded = true;
    } else if (prefValue === SafeBrowsingSetting.STANDARD) {
      this.$.safeBrowsingStandard.expanded = true;
    }
    this.browserProxy_ = PrivacyPageBrowserProxyImpl.getInstance();

    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @protected
   */
  currentRouteChanged(route) {
    if (route === routes.SECURITY) {
      this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
          SafeBrowsingInteractions.SAFE_BROWSING_SHOWED);
      const queryParams = Router.getInstance().getQueryParameters();
      const section = queryParams.get('q');
      if (section === 'enhanced') {
        this.$.safeBrowsingEnhanced.expanded = true;
        this.$.safeBrowsingStandard.expanded = false;
      }
    }
  },

  /**
   * Updates the buttons' expanded status by propagating previous click
   * events
   * @private
   */
  updateCollapsedButtons_() {
    this.$.safeBrowsingEnhanced.updateCollapsed();
    this.$.safeBrowsingStandard.updateCollapsed();
  },

  /**
   * Possibly displays the Safe Browsing disable dialog based on the users
   * selection.
   * @private
   */
  onSafeBrowsingRadioChange_: function() {
    const selected =
        Number.parseInt(this.$.safeBrowsingRadioGroup.selected, 10);
    const prefValue = this.getPref('generated.safe_browsing').value;
    if (prefValue !== selected) {
      this.recordInteractionHistogramOnRadioChange_(
          /** @type {!SafeBrowsingSetting} */ (selected));
      this.recordActionOnRadioChange_(
          /** @type {!SafeBrowsingSetting} */ (selected));
    }
    if (selected === SafeBrowsingSetting.DISABLED) {
      this.showDisableSafebrowsingDialog_ = true;
    } else {
      this.updateCollapsedButtons_();
      this.$.safeBrowsingRadioGroup.sendPrefChange();
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  getDisabledExtendedSafeBrowsing_() {
    return this.getPref('generated.safe_browsing').value !==
        SafeBrowsingSetting.STANDARD;
  },

  /**
   * @return {string}
   * @private
   */
  getPasswordsLeakToggleSubLabel_() {
    let subLabel = this.i18n('passwordsLeakDetectionGeneralDescription');
    // If the backing password leak detection preference is enabled, but the
    // generated preference is disabled, then additional text explaining that
    // the feature will be enabled if the user signs in is added.
    if (this.getPref('profile.password_manager_leak_detection').value &&
        !this.getPref('generated.password_leak_detection').value) {
      subLabel +=
          ' ' +  // Whitespace is a valid sentence separator w.r.t. i18n.
          this.i18n('passwordsLeakDetectionSignedOutEnabledDescription');
    }
    return subLabel;
  },

  /** @private */
  onManageCertificatesClick_() {
    // <if expr="use_nss_certs">
    Router.getInstance().navigateTo(routes.CERTIFICATES);
    // </if>
    // <if expr="is_win or is_macosx">
    this.browserProxy_.showManageSSLCertificates();
    // </if>
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.MANAGE_CERTIFICATES);
  },

  /** @private */
  onAdvancedProtectionProgramLinkClick_() {
    window.open(loadTimeData.getString('advancedProtectionURL'));
  },

  /** @private */
  onSecurityKeysClick_() {
    Router.getInstance().navigateTo(routes.SECURITY_KEYS);
  },

  /** @private */
  onSafeBrowsingExtendedReportingChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.IMPROVE_SECURITY);
  },

  /**
   * Handles the closure of the disable safebrowsing dialog, reselects the
   * appropriate radio button if the user cancels the dialog, and puts focus on
   * the disable safebrowsing button.
   * @private
   */
  onDisableSafebrowsingDialogClose_() {
    const confirmed =
        /** @type {!SettingsDisableSafebrowsingDialogElement} */ (
            this.$$('settings-disable-safebrowsing-dialog'))
            .wasConfirmed();
    this.recordInteractionHistogramOnSafeBrowsingDialogClose_(confirmed);
    this.recordActionOnSafeBrowsingDialogClose_(confirmed);
    // Check if the dialog was confirmed before closing it.
    if (confirmed) {
      this.$.safeBrowsingRadioGroup.sendPrefChange();
      this.updateCollapsedButtons_();
    } else {
      this.$.safeBrowsingRadioGroup.resetToPrefValue();
    }

    this.showDisableSafebrowsingDialog_ = false;

    // Set focus back to the no protection button regardless of user interaction
    // with the dialog, as it was the entry point to the dialog.
    focusWithoutInk(assert(this.$.safeBrowsingDisabled));
  },

  /** @private */
  onEnhancedProtectionExpandButtonClicked_() {
    this.recordInteractionHistogramOnExpandButtonClicked_(
        SafeBrowsingSetting.ENHANCED);
    this.recordActionOnExpandButtonClicked_(SafeBrowsingSetting.ENHANCED);
  },

  /** @private */
  onStandardProtectionExpandButtonClicked_() {
    this.recordInteractionHistogramOnExpandButtonClicked_(
        SafeBrowsingSetting.STANDARD);
    this.recordActionOnExpandButtonClicked_(SafeBrowsingSetting.STANDARD);
  },

  /**
   * @param {!SafeBrowsingSetting} safeBrowsingSetting
   * @private
   */
  recordInteractionHistogramOnRadioChange_(safeBrowsingSetting) {
    let action;
    if (safeBrowsingSetting === SafeBrowsingSetting.ENHANCED) {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED;
    } else if (safeBrowsingSetting === SafeBrowsingSetting.STANDARD) {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_STANDARD_PROTECTION_CLICKED;
    } else {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED;
    }
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(action);
  },

  /**
   * @param {!SafeBrowsingSetting} safeBrowsingSetting
   * @private
   */
  recordInteractionHistogramOnExpandButtonClicked_(safeBrowsingSetting) {
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        safeBrowsingSetting === SafeBrowsingSetting.ENHANCED ?
            SafeBrowsingInteractions
                .SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED :
            SafeBrowsingInteractions
                .SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED);
  },

  /**
   * @param {boolean} confirmed
   * @private
   */
  recordInteractionHistogramOnSafeBrowsingDialogClose_(confirmed) {
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        confirmed ? SafeBrowsingInteractions
                        .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED :
                    SafeBrowsingInteractions
                        .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED);
  },

  /**
   * @param {!SafeBrowsingSetting} safeBrowsingSetting
   * @private
   */
  recordActionOnRadioChange_(safeBrowsingSetting) {
    let actionName;
    if (safeBrowsingSetting === SafeBrowsingSetting.ENHANCED) {
      actionName = 'SafeBrowsing.Settings.EnhancedProtectionClicked';
    } else if (safeBrowsingSetting === SafeBrowsingSetting.STANDARD) {
      actionName = 'SafeBrowsing.Settings.StandardProtectionClicked';
    } else {
      actionName = 'SafeBrowsing.Settings.DisableSafeBrowsingClicked';
    }
    this.metricsBrowserProxy_.recordAction(actionName);
  },

  /**
   * @param {!SafeBrowsingSetting} safeBrowsingSetting
   * @private
   */
  recordActionOnExpandButtonClicked_(safeBrowsingSetting) {
    this.metricsBrowserProxy_.recordAction(
        safeBrowsingSetting === SafeBrowsingSetting.ENHANCED ?
            'SafeBrowsing.Settings.EnhancedProtectionExpandArrowClicked' :
            'SafeBrowsing.Settings.StandardProtectionExpandArrowClicked');
  },

  /**
   * @param {boolean} confirmed
   * @private
   */
  recordActionOnSafeBrowsingDialogClose_(confirmed) {
    this.metricsBrowserProxy_.recordAction(
        confirmed ? 'SafeBrowsing.Settings.DisableSafeBrowsingDialogConfirmed' :
                    'SafeBrowsing.Settings.DisableSafeBrowsingDialogDenied');
  },
});
