// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-list-entry' shows an Allowed and Blocked site for a given category.
 */
Polymer({
  is: 'site-list-entry',

  behaviors: [
    SiteSettingsBehavior,
    FocusRowBehavior,
  ],

  properties: {
    /** @private */
    enableSiteSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSiteSettings');
      },
    },

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
    model: Object,

    /** @private */
    siteDescription_: {
      type: String,
      computed: 'computeSiteDescription_(model)',
    },

    /** @private */
    showPolicyPrefIndicator_: {
      type: Boolean,
      computed: 'computeShowPolicyPrefIndicator_(model)',
    },
  },

  /** @private */
  onShowTooltip_: function() {
    const indicator = assert(this.$$('cr-policy-pref-indicator'));
    // The tooltip text is used by an paper-tooltip contained inside the
    // cr-policy-pref-indicator. The text is currently held in a private
    // property. This text is needed here to send up to the common tooltip
    // component.
    const text = indicator.indicatorTooltip_;
    this.fire('show-tooltip', {target: indicator, text});
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideResetButton_: function() {
    if (this.model === undefined)
      return false;

    return this.model.enforcement ==
        chrome.settingsPrivate.Enforcement.ENFORCED ||
        !(this.readOnlyList || !!this.model.embeddingOrigin);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideActionMenu_: function() {
    if (this.model === undefined)
      return false;

    return this.model.enforcement ==
        chrome.settingsPrivate.Enforcement.ENFORCED ||
        this.readOnlyList || !!this.model.embeddingOrigin;
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @param {!{model: !{item: !SiteException}}} event
   * @private
   */
  onOriginTap_: function(event) {
    if (!this.enableSiteSettings_)
      return;
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + this.model.origin));
  },

  /**
   * Returns the appropriate site description to display. This can, for example,
   * be blank, an 'embedded on <site>' or 'Current incognito session' (or a
   * mix of the last two).
   * @return {string} The site description.
   */
  computeSiteDescription_: function() {
    let displayName = '';
    if (this.model.embeddingOrigin) {
      displayName = loadTimeData.getStringF(
          'embeddedOnHost', this.sanitizePort(this.model.embeddingOrigin));
    } else if (this.category == settings.ContentSettingsTypes.GEOLOCATION) {
      displayName = loadTimeData.getString('embeddedOnAnyHost');
    }

    // <if expr="chromeos">
    if (this.model.category === settings.ContentSettingsTypes.NOTIFICATIONS &&
        this.model.showAndroidSmsNote) {
      displayName = loadTimeData.getString('androidSmsNote');
    }
    // </if>

    if (this.model.incognito) {
      if (displayName.length > 0)
        return loadTimeData.getStringF('embeddedIncognitoSite', displayName);
      return loadTimeData.getString('incognitoSite');
    }
    return displayName;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowPolicyPrefIndicator_: function() {
    return this.model.enforcement ==
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        !!this.model.controlledBy;
  },

  /** @private */
  onResetButtonTap_: function() {
    this.browserProxy.resetCategoryPermissionForPattern(
        this.model.origin, this.model.embeddingOrigin, this.model.category,
        this.model.incognito);
  },

  /** @private */
  onShowActionMenuTap_: function() {
    this.fire(
        'show-action-menu',
        {anchor: this.$.actionMenuButton, model: this.model});
  },
});
