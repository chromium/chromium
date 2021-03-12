// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-list-entry' shows an Allowed and Blocked site for a given category.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../icons.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {ChooserType, ContentSettingsTypes, SITE_EXCEPTION_WILDCARD} from './constants.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';
import {SiteException} from './site_settings_prefs_browser_proxy.js';

Polymer({
  is: 'site-list-entry',

  _template: html`{__html_template__}`,

  behaviors: [
    SiteSettingsBehavior,
    FocusRowBehavior,
  ],

  properties: {
    /**
     * Some content types (like Location) do not allow the user to manually
     * edit the exception list from within Settings.
     * @private
     */
    readOnlyList: {
      type: Boolean,
      value: false,
    },

    /**
     * Site to display in the widget.
     * @type {!SiteException}
     */
    model: {
      type: Object,
      observer: 'onModelChanged_',
    },

    /**
     * If the site represented is part of a chooser exception, the chooser type
     * will be stored here to allow the permission to be manipulated.
     * @private {!ChooserType}
     */
    chooserType: {
      type: String,
      value: ChooserType.NONE,
    },

    /**
     * If the site represented is part of a chooser exception, the chooser
     * object will be stored here to allow the permission to be manipulated.
     * @private
     */
    chooserObject: {
      type: Object,
      value: null,
    },

    /** @private */
    showPolicyPrefIndicator_: {
      type: Boolean,
      computed: 'computeShowPolicyPrefIndicator_(model)',
    },

    /** @private */
    allowNavigateToSiteDetail_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onShowTooltip_() {
    const indicator = assert(this.$$('cr-policy-pref-indicator'));
    // The tooltip text is used by an paper-tooltip contained inside the
    // cr-policy-pref-indicator. The text is currently held in a private
    // property. This text is needed here to send up to the common tooltip
    // component.
    const text = indicator.indicatorTooltip_;
    this.fire('show-tooltip', {target: indicator, text});
  },

  /** @private */
  onShowIncognitoTooltip_: function() {
    const tooltip = assert(this.$$('#incognitoTooltip'));
    // The tooltip text is used by an paper-tooltip contained inside the
    // cr-policy-pref-indicator. The text is currently held in a private
    // property. This text is needed here to send up to the common tooltip
    // component.
    const text = loadTimeData.getString('incognitoSiteExceptionDesc');
    this.fire('show-tooltip', {target: tooltip, text});
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideResetButton_() {
    if (this.model === undefined) {
      return false;
    }

    return this.model.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED ||
        !(this.readOnlyList || !!this.model.embeddingOrigin);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideActionMenu_() {
    if (this.model === undefined) {
      return false;
    }

    return this.model.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED ||
        this.readOnlyList || !!this.model.embeddingOrigin;
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @private
   */
  onOriginTap_() {
    if (!this.allowNavigateToSiteDetail_) {
      return;
    }
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + this.model.origin));
  },

  /**
   * Returns the appropriate display name to show for the exception.
   * This can, for example, be the website that is affected itself,
   * or the website whose third parties are also affected.
   * @return {string}
   */
  computeDisplayName_() {
    if (this.model.embeddingOrigin &&
        this.model.category === ContentSettingsTypes.COOKIES &&
        this.model.origin.trim() === SITE_EXCEPTION_WILDCARD) {
      return this.model.embeddingOrigin;
    }
    return this.model.displayName;
  },

  /**
   * Returns the appropriate site description to display. This can, for example,
   * be blank, an 'embedded on <site>' string, or a third-party exception
   * description string.
   * @return {string}
   */
  computeSiteDescription_() {
    let description = '';

    if (this.model.isEmbargoed) {
      assert(
          !this.model.embeddingOrigin,
          'Embedding origin should be empty for embargoed origin.');
      description = loadTimeData.getString('siteSettingsSourceEmbargo');
    } else if (this.model.embeddingOrigin) {
      if (this.model.category === ContentSettingsTypes.COOKIES &&
          this.model.origin.trim() === SITE_EXCEPTION_WILDCARD) {
        description = loadTimeData.getString(
            'siteSettingsCookiesThirdPartyExceptionLabel');
      } else {
        description = loadTimeData.getStringF(
            'embeddedOnHost', this.sanitizePort(this.model.embeddingOrigin));
      }
    } else if (this.category === ContentSettingsTypes.GEOLOCATION) {
      description = loadTimeData.getString('embeddedOnAnyHost');
    }

    // <if expr="chromeos">
    if (this.model.category === ContentSettingsTypes.NOTIFICATIONS &&
        this.model.showAndroidSmsNote) {
      description = loadTimeData.getString('androidSmsNote');
    }
    // </if>

    return description;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowPolicyPrefIndicator_() {
    return this.model.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        !!this.model.controlledBy;
  },

  /** @private */
  onResetButtonTap_() {
    // Use the appropriate method to reset a chooser exception.
    if (this.chooserType !== ChooserType.NONE && this.chooserObject !== null) {
      this.browserProxy.resetChooserExceptionForSite(
          this.chooserType, this.model.origin, this.model.embeddingOrigin,
          this.chooserObject);
      return;
    }

    this.browserProxy.resetCategoryPermissionForPattern(
        this.model.origin, this.model.embeddingOrigin, this.model.category,
        this.model.incognito);
  },

  /** @private */
  onShowActionMenuTap_() {
    // Chooser exceptions do not support the action menu, so do nothing.
    if (this.chooserType !== ChooserType.NONE) {
      return;
    }

    this.fire(
        'show-action-menu',
        {anchor: this.$.actionMenuButton, model: this.model});
  },

  /** @private */
  onModelChanged_() {
    if (!this.model) {
      this.allowNavigateToSiteDetail_ = false;
      return;
    }
    this.browserProxy.isOriginValid(this.model.origin).then((valid) => {
      this.allowNavigateToSiteDetail_ = valid;
    });
  }
});
