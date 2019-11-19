// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'add-site-dialog' provides a dialog to add exceptions for a given Content
 * Settings category.
 */
Polymer({
  is: 'add-site-dialog',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * What kind of setting, e.g. Location, Camera, Cookies, and so on.
     * @type {settings.ContentSettingsTypes}
     */
    category: String,

    /**
     * Whether this is about an Allow, Block, SessionOnly, or other.
     * @type {settings.ContentSetting}
     */
    contentSetting: String,

    /** @private */
    hasIncognito: {
      type: Boolean,
      observer: 'hasIncognitoChanged_',
    },

    /**
     * The site to add an exception for.
     * @private
     */
    site_: String,

    /**
     * The error message to display when the pattern is invalid.
     * @private
     */
    errorMessage_: String,
  },

  /** @override */
  attached: function() {
    assert(this.category);
    assert(this.contentSetting);
    assert(typeof this.hasIncognito != 'undefined');

    this.$.dialog.showModal();
  },

  /**
   * Validates that the pattern entered is valid.
   * @private
   */
  validate_: function() {
    // If input is empty, disable the action button, but don't show the red
    // invalid message.
    if (this.$.site.value.trim() == '') {
      this.$.site.invalid = false;
      this.$.add.disabled = true;
      return;
    }

    this.browserProxy.isPatternValidForType(this.site_, this.category)
        .then(({isValid, reason}) => {
          this.$.site.invalid = !isValid;
          this.$.add.disabled = !isValid;
          this.errorMessage_ = reason || '';
        });
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },

  /**
   * The tap handler for the Add [Site] button (adds the pattern and closes
   * the dialog).
   * @private
   */
  onSubmit_: function() {
    assert(!this.$.add.disabled);
    let primaryPattern = this.site_;
    let secondaryPattern = settings.SITE_EXCEPTION_WILDCARD;

    if (this.$.thirdParties.checked) {
      primaryPattern = settings.SITE_EXCEPTION_WILDCARD;
      secondaryPattern = this.site_;
    }

    this.browserProxy.setCategoryPermissionForPattern(
        primaryPattern, secondaryPattern, this.category, this.contentSetting,
        this.$.incognito.checked);

    this.$.dialog.close();
  },

  /** @private */
  showIncognitoSessionOnly_: function() {
    return this.hasIncognito && !loadTimeData.getBoolean('isGuest') &&
        this.contentSetting != settings.ContentSetting.SESSION_ONLY;
  },

  /** @private */
  hasIncognitoChanged_: function() {
    if (!this.hasIncognito) {
      this.$.incognito.checked = false;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideThirdPartyCookieCheckbox_: function() {
    return this.category !== settings.ContentSettingsTypes.COOKIES ||
        !loadTimeData.getBoolean('showImprovedCookieControlsForThirdParties');
  },
});
