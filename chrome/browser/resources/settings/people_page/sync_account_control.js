// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'settings-sync-account-section' is the settings page containing sign-in
 * settings.
 */
cr.exportPath('settings');

/** @const {number} */
settings.MAX_SIGNIN_PROMO_IMPRESSION = 10;

Polymer({
  is: 'settings-sync-account-control',
  behaviors: [WebUIListenerBehavior],
  properties: {
    /**
     * The current sync status, supplied by parent element.
     * @type {!settings.SyncStatus}
     */
    syncStatus: Object,

    // String to be used as a title when the promo has an account.
    promoLabelWithAccount: String,

    // String to be used as title of the promo has no account.
    promoLabelWithNoAccount: String,

    // String to be used as a subtitle when the promo has an account.
    promoSecondaryLabelWithAccount: String,

    // String to be used as subtitle of the promo has no account.
    promoSecondaryLabelWithNoAccount: String,

    /**
     * Proxy variable for syncStatus.signedIn to shield observer from being
     * triggered multiple times whenever syncStatus changes.
     * @private {boolean}
     */
    signedIn_: {
      type: Boolean,
      computed: 'computeSignedIn_(syncStatus.signedIn)',
      observer: 'onSignedInChanged_',
    },

    /** @private {!Array<!settings.StoredAccount>} */
    storedAccounts_: Object,

    /** @private {?settings.StoredAccount} */
    shownAccount_: Object,

    showingPromo: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    // This property should be set by the parent only and should not change
    // after the element is created.
    embeddedInSubpage: {
      type: Boolean,
      reflectToAttribute: true,
    },

    // This property should be set by the parent only and should not change
    // after the element is created.
    hideButtons: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private {boolean} */
    shouldShowAvatarRow_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowAvatarRow_(storedAccounts_, syncStatus,' +
          'storedAccounts_.length, syncStatus.signedIn)',
      observer: 'onShouldShowAvatarRowChange_',
    }
  },

  observers: [
    'onShownAccountShouldChange_(storedAccounts_, syncStatus)',
  ],

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  created: function() {
    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.addWebUIListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
  },

  /**
   * Records the following user actions:
   * - Signin_Impression_FromSettings and
   * - Signin_ImpressionWithAccount_FromSettings
   * - Signin_ImpressionWithNoAccount_FromSettings
   * @private
   */
  recordImpressionUserActions_: function() {
    assert(!this.syncStatus.signedIn);
    assert(this.shownAccount_ !== undefined);

    chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');
    if (this.shownAccount_) {
      chrome.metricsPrivate.recordUserAction(
          'Signin_ImpressionWithAccount_FromSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Signin_ImpressionWithNoAccount_FromSettings');
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSignedIn_: function() {
    return !!this.syncStatus.signedIn;
  },

  /** @private */
  onSignedInChanged_: function() {
    if (this.embeddedInSubpage) {
      this.showingPromo = true;
      return;
    }

    if (!this.showingPromo && !this.syncStatus.signedIn &&
        this.syncBrowserProxy_.getPromoImpressionCount() <
            settings.MAX_SIGNIN_PROMO_IMPRESSION) {
      this.showingPromo = true;
      this.syncBrowserProxy_.incrementPromoImpressionCount();
    } else {
      // Turn off the promo if the user is signed in.
      this.showingPromo = false;
    }
    if (!this.syncStatus.signedIn && this.shownAccount_ !== undefined)
      this.recordImpressionUserActions_();
  },

  /**
   * @param {string} labelWithAccount
   * @param {string} labelWithNoAccount
   * @return {string}
   * @private
   */
  getLabel_: function(labelWithAccount, labelWithNoAccount) {
    return this.shownAccount_ ? labelWithAccount : labelWithNoAccount;
  },

  /**
   * @param {string} label
   * @param {string} name
   * @return {string}
   * @private
   */
  getSubstituteLabel_: function(label, name) {
    return loadTimeData.substituteString(label, name);
  },

  /**
   * @param {string} label
   * @param {string} account
   * @return {string}
   * @private
   */
  getAccountLabel_: function(label, account) {
    return this.syncStatus.signedIn && !this.syncStatus.hasError &&
            !this.syncStatus.disabled ?
        loadTimeData.substituteString(label, account) :
        account;
  },

  /**
   * @param {?string} image
   * @return {string}
   * @private
   */
  getAccountImageSrc_: function(image) {
    // image can be undefined if the account has not set an avatar photo.
    return image || 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  },

  /**
   * Returns the class of the sync icon.
   * @return {string}
   * @private
   */
  getSyncIconStyle_: function() {
    if (!!this.syncStatus.hasUnrecoverableError)
      return 'sync-problem';
    if (!!this.syncStatus.hasError) {
      return this.syncStatus.statusAction ==
              settings.StatusAction.REAUTHENTICATE ?
          'sync-paused' :
          'sync-problem';
    }
    if (!!this.syncStatus.disabled)
      return 'sync-disabled';
    return 'sync';
  },

  /**
   * Returned value must match one of iron-icon's settings:(*) icon name.
   * @return {string}
   * @private
   */
  getSyncIcon_: function() {
    switch (this.getSyncIconStyle_()) {
      case 'sync-problem':
        return 'settings:sync-problem';
      case 'sync-paused':
        return 'settings:sync-disabled';
      default:
        return 'cr:sync';
    }
  },

  /**
   * @return {string}
   * @private
   */
  getAvatarRowTitle_: function(
      accountName, syncErrorLabel, authErrorLabel, disabledLabel) {
    switch (this.getSyncIconStyle_()) {
      case 'sync-problem':
        return syncErrorLabel;
      case 'sync-paused':
        return authErrorLabel;
      case 'sync-disabled':
        return disabledLabel;
      default:
        return accountName;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowTurnOffButton_: function() {
    return !this.hideButtons && !!this.syncStatus.signedIn &&
        !this.embeddedInSubpage;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSigninAgainButton_: function() {
    return !this.hideButtons && !!this.syncStatus.signedIn &&
        this.embeddedInSubpage && !!this.syncStatus.hasError &&
        this.syncStatus.statusAction == settings.StatusAction.REAUTHENTICATE;
  },

  /**
   * @param {!Array<!settings.StoredAccount>} accounts
   * @private
   */
  handleStoredAccounts_: function(accounts) {
    this.storedAccounts_ = accounts;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowAvatarRow_: function() {
    if (this.storedAccounts_ === undefined || this.syncStatus === undefined)
      return false;

    return this.syncStatus.signedIn || this.storedAccounts_.length > 0;
  },

  /** @private */
  onSigninTap_: function() {
    this.syncBrowserProxy_.startSignIn();

    // Need to close here since one menu item also triggers this function.
    if (this.$$('#menu')) {
      /** @type {!CrActionMenuElement} */ (this.$$('#menu')).close();
    }
  },

  /** @private */
  onSignoutTap_: function() {
    this.syncBrowserProxy_.signOut(false /* deleteProfile */);
    /** @type {!CrActionMenuElement} */ (this.$$('#menu')).close();
  },

  /** @private */
  onSyncButtonTap_: function() {
    assert(this.shownAccount_);
    assert(this.storedAccounts_.length > 0);
    const isDefaultPromoAccount =
        (this.shownAccount_.email == this.storedAccounts_[0].email);

    this.syncBrowserProxy_.startSyncingWithEmail(
        this.shownAccount_.email, isDefaultPromoAccount);
  },

  /** @private */
  onTurnOffButtonTap_: function() {
    /* This will route to people_page's disconnect dialog. */
    settings.navigateTo(settings.routes.SIGN_OUT);
  },

  /** @private */
  onMenuButtonTap_: function() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (this.$$('#menu'));
    actionMenu.showAt(assert(this.$$('#dropdown-arrow')));
  },

  /** @private */
  onShouldShowAvatarRowChange_: function() {
    // Close dropdown when avatar-row hides, so if it appears again, the menu
    // won't be open by default.
    const actionMenu = this.$$('#menu');
    if (!this.shouldShowAvatarRow_ && actionMenu && actionMenu.open)
      actionMenu.close();
  },

  /**
   * @param {!{model:
   *          !{item: !settings.StoredAccount},
   *        }} e
   * @private
   */
  onAccountTap_: function(e) {
    this.shownAccount_ = e.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('#menu')).close();
  },

  /** @private */
  onShownAccountShouldChange_: function() {
    if (this.storedAccounts_ === undefined || this.syncStatus === undefined)
      return;

    if (this.syncStatus.signedIn) {
      for (let i = 0; i < this.storedAccounts_.length; i++) {
        if (this.storedAccounts_[i].email == this.syncStatus.signedInUsername) {
          this.shownAccount_ = this.storedAccounts_[i];
          return;
        }
      }
    } else {
      const firstStoredAccount =
          (this.storedAccounts_.length > 0) ? this.storedAccounts_[0] : null;

      // Sign-in impressions should be recorded in the following cases:
      // 1. When the promo is first shown, i.e. when |shownAccount_| is
      //   initialized;
      // 2. When the impression account state changes, i.e. promo impression
      //   state changes (WithAccount -> WithNoAccount) or
      //   (WithNoAccount -> WithAccount).
      const shouldRecordImpression = (this.shownAccount_ === undefined) ||
          (!this.shownAccount_ && firstStoredAccount) ||
          (this.shownAccount_ && !firstStoredAccount);

      this.shownAccount_ = firstStoredAccount;

      if (shouldRecordImpression)
        this.recordImpressionUserActions_();
    }
  }
});
