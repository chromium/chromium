// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'settings-sync-account-section' is the settings page containing sign-in
 * settings.
 */
cr.define('settings', function() {
  /** @const {number} */
  /* #export */ const MAX_SIGNIN_PROMO_IMPRESSION = 10;

  Polymer({
    is: 'settings-sync-account-control',

    behaviors: [
      WebUIListenerBehavior,
      PrefsBehavior,
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
      },

      /** @private */
      subLabel_: {
        type: String,
        computed: 'computeSubLabel_(promoSecondaryLabelWithAccount,' +
            'promoSecondaryLabelWithNoAccount, shownAccount_)',
      },

      /** @private */
      showSetupButtons_: {
        type: Boolean,
        computed: 'computeShowSetupButtons_(' +
            'hideButtons, syncStatus.firstSetupInProgress)',
      },
    },

    observers: [
      'onShownAccountShouldChange_(storedAccounts_, syncStatus)',
    ],

    /** @private {?settings.SyncBrowserProxy} */
    syncBrowserProxy_: null,

    created() {
      this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    },

    /** @override */
    attached() {
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
    recordImpressionUserActions_() {
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
    computeSignedIn_() {
      return !!this.syncStatus && !!this.syncStatus.signedIn;
    },

    /** @private */
    onSignedInChanged_() {
      if (this.embeddedInSubpage) {
        this.showingPromo = true;
        return;
      }

      if (!this.showingPromo && !this.syncStatus.signedIn &&
          this.syncBrowserProxy_.getPromoImpressionCount() <
              MAX_SIGNIN_PROMO_IMPRESSION) {
        this.showingPromo = true;
        this.syncBrowserProxy_.incrementPromoImpressionCount();
      } else {
        // Turn off the promo if the user is signed in.
        this.showingPromo = false;
      }
      if (!this.syncStatus.signedIn && this.shownAccount_ !== undefined) {
        this.recordImpressionUserActions_();
      }
    },

    /**
     * @param {string} labelWithAccount
     * @param {string} labelWithNoAccount
     * @return {string}
     * @private
     */
    getLabel_(labelWithAccount, labelWithNoAccount) {
      return this.shownAccount_ ? labelWithAccount : labelWithNoAccount;
    },

    /**
     * @return {string}
     * @private
     */
    computeSubLabel_() {
      return this.getLabel_(
          this.promoSecondaryLabelWithAccount,
          this.promoSecondaryLabelWithNoAccount);
    },

    /**
     * @param {string} label
     * @param {string} name
     * @return {string}
     * @private
     */
    getSubstituteLabel_(label, name) {
      return loadTimeData.substituteString(label, name);
    },

    /**
     * @param {string} label
     * @param {string} account
     * @return {string}
     * @private
     */
    getAccountLabel_(label, account) {
      if (this.syncStatus.firstSetupInProgress) {
        return this.syncStatus.statusText || account;
      }
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
    getAccountImageSrc_(image) {
      // image can be undefined if the account has not set an avatar photo.
      return image || 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
    },

    /**
     * Returns the class of the sync icon.
     * @return {string}
     * @private
     */
    getSyncIconStyle_() {
      if (this.syncStatus.disabled) {
        return 'sync-disabled';
      }
      if (!this.syncStatus.hasError) {
        return 'sync';
      }
      // Specific error cases below.
      if (this.syncStatus.hasUnrecoverableError) {
        return 'sync-problem';
      }
      if (this.syncStatus.statusAction ===
          settings.StatusAction.REAUTHENTICATE) {
        return 'sync-paused';
      }
      return 'sync-problem';
    },

    /**
     * Returned value must match one of iron-icon's settings:(*) icon name.
     * @return {string}
     * @private
     */
    getSyncIcon_() {
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
     * @param {string} accountName
     * @param {string} syncErrorLabel
     * @param {string} syncPasswordsOnlyErrorLabel
     * @param {string} authErrorLabel
     * @param {string} disabledLabel
     * @return {string}
     * @private
     */
    getAvatarRowTitle_(
        accountName, syncErrorLabel, syncPasswordsOnlyErrorLabel,
        authErrorLabel, disabledLabel) {
      if (this.syncStatus.disabled) {
        return disabledLabel;
      }
      if (!this.syncStatus.hasError) {
        return accountName;
      }
      // Specific error cases below.
      if (this.syncStatus.hasUnrecoverableError) {
        return syncErrorLabel;
      }
      if (this.syncStatus.statusAction ===
          settings.StatusAction.REAUTHENTICATE) {
        return authErrorLabel;
      }
      if (this.syncStatus.hasPasswordsOnlyError) {
        return syncPasswordsOnlyErrorLabel;
      }
      return syncErrorLabel;
    },

    /**
     * Determines if the sync button should be disabled in response to
     * either a first setup flow or chrome sign-in being disabled.
     * @return {boolean}
     * @private
     */
    shouldDisableSyncButton_() {
      if (this.hideButtons || this.prefs === undefined) {
        return this.computeShowSetupButtons_();
      }
      return !!this.syncStatus.firstSetupInProgress ||
          !this.getPref('signin.allowed_on_next_startup').value;
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldShowTurnOffButton_() {
      // <if expr="chromeos">
      if (this.syncStatus.domain) {
        // Chrome OS cannot delete the user's profile like other platforms, so
        // hide the turn off sync button for enterprise users who are not
        // allowed to sign out.
        return false;
      }
      // </if>
      return !this.hideButtons && !this.showSetupButtons_ &&
          !!this.syncStatus.signedIn;
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldShowErrorActionButton_() {
      if (this.embeddedInSubpage &&
          this.syncStatus.statusAction ===
              settings.StatusAction.ENTER_PASSPHRASE) {
        // In a subpage the passphrase button is not required.
        return false;
      }
      return !this.hideButtons && !this.showSetupButtons_ &&
          !!this.syncStatus.signedIn && !!this.syncStatus.hasError &&
          this.syncStatus.statusAction !== settings.StatusAction.NO_ACTION;
    },

    /**
     * @param {!Array<!settings.StoredAccount>} accounts
     * @private
     */
    handleStoredAccounts_(accounts) {
      this.storedAccounts_ = accounts;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeShouldShowAvatarRow_() {
      if (this.storedAccounts_ === undefined || this.syncStatus === undefined) {
        return false;
      }

      return this.syncStatus.signedIn || this.storedAccounts_.length > 0;
    },

    /** @private */
    onErrorButtonTap_() {
      const router = settings.Router.getInstance();
      const routes =
          /** @type {{ SIGN_OUT: !settings.Route }} */ (router.getRoutes());
      switch (this.syncStatus.statusAction) {
        case settings.StatusAction.REAUTHENTICATE:
          this.syncBrowserProxy_.startSignIn();
          break;
        case settings.StatusAction.SIGNOUT_AND_SIGNIN:
          if (this.syncStatus.domain) {
            router.navigateTo(routes.SIGN_OUT);
          } else {
            // Silently sign the user out without deleting their profile and
            // prompt them to sign back in.
            this.syncBrowserProxy_.signOut(false);
            this.syncBrowserProxy_.startSignIn();
          }
          break;
        case settings.StatusAction.UPGRADE_CLIENT:
          router.navigateTo(router.getRoutes().ABOUT);
          break;
        case settings.StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS:
          this.syncBrowserProxy_.startKeyRetrieval();
          break;
        case settings.StatusAction.ENTER_PASSPHRASE:
        case settings.StatusAction.CONFIRM_SYNC_SETTINGS:
        default:
          router.navigateTo(
              /** @type {{ SYNC: !settings.Route }} */ (router.getRoutes())
                  .SYNC);
      }
    },

    /** @private */
    onSigninTap_() {
      // <if expr="not chromeos">
      this.syncBrowserProxy_.startSignIn();
      // </if>
      // <if expr="chromeos">
      // Chrome OS is always signed-in, so just turn on sync.
      this.syncBrowserProxy_.turnOnSync();
      // </if>
      // Need to close here since one menu item also triggers this function.
      if (this.$$('#menu')) {
        /** @type {!CrActionMenuElement} */ (this.$$('#menu')).close();
      }
    },

    /** @private */
    onSignoutTap_() {
      this.syncBrowserProxy_.signOut(false /* deleteProfile */);
      /** @type {!CrActionMenuElement} */ (this.$$('#menu')).close();
    },

    /** @private */
    onSyncButtonTap_() {
      assert(this.shownAccount_);
      assert(this.storedAccounts_.length > 0);
      const isDefaultPromoAccount =
          (this.shownAccount_.email === this.storedAccounts_[0].email);

      this.syncBrowserProxy_.startSyncingWithEmail(
          this.shownAccount_.email, isDefaultPromoAccount);
    },

    /** @private */
    onTurnOffButtonTap_() {
      /* This will route to people_page's disconnect dialog. */
      const router = settings.Router.getInstance();
      router.navigateTo(
          /** @type {{ SIGN_OUT: !settings.Route }} */ (router.getRoutes())
              .SIGN_OUT);
    },

    /** @private */
    onMenuButtonTap_() {
      const actionMenu =
          /** @type {!CrActionMenuElement} */ (this.$$('#menu'));
      actionMenu.showAt(assert(this.$$('#dropdown-arrow')));
    },

    /** @private */
    onShouldShowAvatarRowChange_() {
      // Close dropdown when avatar-row hides, so if it appears again, the menu
      // won't be open by default.
      const actionMenu = this.$$('#menu');
      if (!this.shouldShowAvatarRow_ && actionMenu && actionMenu.open) {
        actionMenu.close();
      }
    },

    /**
     * @param {!{model:
     *          !{item: !settings.StoredAccount},
     *        }} e
     * @private
     */
    onAccountTap_(e) {
      this.shownAccount_ = e.model.item;
      /** @type {!CrActionMenuElement} */ (this.$$('#menu')).close();
    },

    /** @private */
    onShownAccountShouldChange_() {
      if (this.storedAccounts_ === undefined || this.syncStatus === undefined) {
        return;
      }

      if (this.syncStatus.signedIn) {
        for (let i = 0; i < this.storedAccounts_.length; i++) {
          if (this.storedAccounts_[i].email ===
              this.syncStatus.signedInUsername) {
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

        if (shouldRecordImpression) {
          this.recordImpressionUserActions_();
        }
      }
    },

    /**
     * @return {boolean}
     * @private
     */
    computeShowSetupButtons_() {
      return !this.hideButtons && !!this.syncStatus.firstSetupInProgress;
    },

    /** @private */
    onSetupCancel_() {
      this.fire('sync-setup-done', false);
    },

    /** @private */
    onSetupConfirm_() {
      this.fire('sync-setup-done', true);
    },
  });

  // #cr_define_end
  return {MAX_SIGNIN_PROMO_IMPRESSION};
});
