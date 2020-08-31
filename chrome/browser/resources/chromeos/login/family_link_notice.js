// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Family Link Notice screen.
 */

Polymer({
  is: 'family-link-notice',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'setDisplayEmail',
    'setDomain',
    'setIsNewGaiaAccount',
  ],

  properties: {

    /**
     * If the gaia account is newly created
     */
    isNewGaiaAccount_: {
      type: Boolean,
      value: false,
    },

    /**
     * The email address to be displayed
     */
    email_: {
      type: String,
      value: '',
    },

    /**
     * The enterprise domain to be displayed
     */
    domain_: {
      type: String,
      value: '',
    },

  },

  ready() {
    this.initializeLoginScreen('FamilyLinkNoticeScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.$.familyLinkDialog;
  },

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * Sets email address.
   * @param {string} email
   */
  setDisplayEmail(email) {
    this.email_ = email;
  },

  /**
   * Sets enterprise domain.
   * @param {string} domain
   */
  setDomain(domain) {
    this.domain_ = domain;
  },

  /**
   * Sets if the gaia account is newly created.
   * @param {boolean} isNewGaiaAccount
   */
  setIsNewGaiaAccount(isNewGaiaAccount) {
    this.isNewGaiaAccount_ = isNewGaiaAccount;
  },

  /**
   * Returns the title of the dialog based on if account is managed. Account is
   * managed when email or domain field is not empty and we show parental
   * controls is not eligible.
   *
   * @private
   */
  getDialogTitle_(locale, email, domain) {
    if (email || domain) {
      return this.i18n('familyLinkDialogNotEligibleTitle');
    } else {
      return this.i18n('familyLinkDialogTitle');
    }
  },

  /**
   * Formats and returns the subtitle of the dialog based on if account is
   * managed or if account is newly created. Account is managed when email or
   * domain field is not empty and we show parental controls is not eligible.
   *
   * @private
   */
  getDialogSubtitle_(locale, isNewGaiaAccount, email, domain) {
    if (email || domain) {
      return this.i18n('familyLinkDialogNotEligibleSubtitle', email, domain);
    } else {
      if (isNewGaiaAccount) {
        return this.i18n('familyLinkDialogNewGaiaAccountSubtitle');
      } else {
        return this.i18n('familyLinkDialogExistingGaiaAccountSubtitle');
      }
    }
  },

  /**
   * On-tap event handler for Continue button.
   *
   * @private
   */
  onContinueButtonPressed_() {
    this.userActed('continue');
  },

});
