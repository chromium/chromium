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
    cr.ui.FocusRowBehavior,
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
     * @private {!settings.ChooserType}
     */
    chooserType: {
      type: String,
      value: settings.ChooserType.NONE,
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
    if (this.model === undefined) {
      return false;
    }

    return this.model.enforcement ==
        chrome.settingsPrivate.Enforcement.ENFORCED ||
        !(this.readOnlyList || !!this.model.embeddingOrigin);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideActionMenu_: function() {
    if (this.model === undefined) {
      return false;
    }

    return this.model.enforcement ==
        chrome.settingsPrivate.Enforcement.ENFORCED ||
        this.readOnlyList || !!this.model.embeddingOrigin;
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @private
   */
  onOriginTap_: function() {
    if (!this.allowNavigateToSiteDetail_) {
      return;
    }
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + this.model.origin));
  },

  /**
   * Returns the appropriate display name to show for the exception.
   * This can, for example, be the website that is affected itself,
   * or the website whose third parties are also affected.
   * @return {string}
   */
  computeDisplayName_: function() {
    if (this.model.embeddingOrigin &&
        this.model.category === settings.ContentSettingsTypes.COOKIES &&
        this.model.origin.trim() == settings.SITE_EXCEPTION_WILDCARD) {
      return this.model.embeddingOrigin;
    }
    return this.model.displayName;
  },

  /**
   * Returns the appropriate site description to display. This can, for example,
   * be blank, an 'embedded on <site>' or 'Current incognito session' (or a
   * mix of the last two).
   * @return {string}
   */
  computeSiteDescription_: function() {
    let description = '';

    if (this.model.embeddingOrigin) {
      if (this.model.category === settings.ContentSettingsTypes.COOKIES &&
          this.model.origin.trim() == settings.SITE_EXCEPTION_WILDCARD) {
        description =
            loadTimeData.getString(
                'siteSettingsCookiesThirdPartyExceptionLabel');
       } else {
         description = loadTimeData.getStringF(
             'embeddedOnHost', this.sanitizePort(this.model.embeddingOrigin));
       }
    } else if (this.category == settings.ContentSettingsTypes.GEOLOCATION) {
      description = loadTimeData.getString('embeddedOnAnyHost');
    }

    // <if expr="chromeos">
    if (this.model.category === settings.ContentSettingsTypes.NOTIFICATIONS &&
        this.model.showAndroidSmsNote) {
      description = loadTimeData.getString('androidSmsNote');
    }
    // </if>

    if (this.model.incognito) {
      if (description.length > 0) {
        description =
            loadTimeData.getStringF('embeddedIncognitoSite', description);
      } else {
        description = loadTimeData.getString('incognitoSite');
      }
    }
    return description;
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
    // Use the appropriate method to reset a chooser exception.
    if (this.chooserType !== settings.ChooserType.NONE &&
        this.chooserObject != null) {
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
  onShowActionMenuTap_: function() {
    // Chooser exceptions do not support the action menu, so do nothing.
    if (this.chooserType !== settings.ChooserType.NONE) {
      return;
    }

    this.fire(
        'show-action-menu',
        {anchor: this.$.actionMenuButton, model: this.model});
  },

  /** @private */
  onModelChanged_: function() {
    if (!this.model) {
      this.allowNavigateToSiteDetail_ = false;
      return;
    }
    this.browserProxy.isOriginValid(this.model.origin).then((valid) => {
      this.allowNavigateToSiteDetail_ = valid;
    });
  }
});
