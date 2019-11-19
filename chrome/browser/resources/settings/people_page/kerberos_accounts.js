// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-accounts' is the settings subpage containing controls to
 * list, add and delete Kerberos Accounts.
 */

'use strict';

Polymer({
  is: 'settings-kerberos-accounts',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of Accounts.
     * @private {!Array<!settings.KerberosAccount>}
     */
    accounts_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The targeted account for menu and other operations.
     * @private {?settings.KerberosAccount}
     */
    selectedAccount_: Object,

    /** @private */
    showAddAccountDialog_: Boolean,

    /** @private */
    addAccountsAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('kerberosAddAccountsAllowed');
      },
    },

    /** @private */
    accountToastText_: {
      type: String,
      value: '',
    },
  },

  /** @private {?settings.KerberosAccountsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'kerberos-accounts-changed', this.refreshAccounts_.bind(this));
  },

  /** @override */
  ready: function() {
    this.browserProxy_ =
        settings.KerberosAccountsBrowserProxyImpl.getInstance();

    // Grab account list and - when done - pop up the reauthentication dialog if
    // there is a kerberos_reauth param.
    this.refreshAccounts_().then(() => {
      const queryParams = settings.getQueryParameters();
      const reauthPrincipal = queryParams.get('kerberos_reauth');
      const reauthAccount = this.accounts_.find(account => {
        return account.principalName == reauthPrincipal;
      });
      if (reauthAccount) {
        this.selectedAccount_ = reauthAccount;
        this.showAddAccountDialog_ = true;
      }
    });
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_: function(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAddAccountClick_: function(event) {
    this.selectedAccount_ = null;
    this.showAddAccountDialog_ = true;
  },

  /**
   * @param {!CustomEvent<!{model: !{item: !settings.Account}}>} event
   * @private
   */
  onReauthenticationClick_: function(event) {
    this.selectedAccount_ = event.model.item;
    this.showAddAccountDialog_ = true;
  },

  /** @private */
  onAddAccountDialogClosed_: function() {
    if (this.$$('kerberos-add-account-dialog').accountWasRefreshed) {
      this.showToast_('kerberosAccountsAccountRefreshedTip');
    }

    this.showAddAccountDialog_ = false;

    // In case it was opened by the 'Refresh now' action menu.
    this.closeActionMenu_();
  },

  /**
   * @return {!Promise}
   * @private
   */
  refreshAccounts_: function() {
    return this.browserProxy_.getAccounts().then(accounts => {
      this.accounts_ = accounts;
    });
  },

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: !settings.KerberosAccount}, target: !Element}}
   *      event
   * @private
   */
  onAccountActionsMenuButtonClick_: function(event) {
    this.selectedAccount_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(event.target);
  },

  /**
   * Closes action menu and resets action menu model.
   * @private
   */
  closeActionMenu_: function() {
    this.$$('cr-action-menu').close();
    this.selectedAccount_ = null;
  },

  /**
   * Removes |this.selectedAccount_|.
   * @private
   */
  onRemoveAccountClick_: function() {
    this.browserProxy_
        .removeAccount(
            /** @type {!settings.KerberosAccount} */ (this.selectedAccount_))
        .then(error => {
          if (error == settings.KerberosErrorType.kNone) {
            this.showToast_('kerberosAccountsAccountRemovedTip');
          } else {
            console.error('Unexpected error removing account: ' + error);
          }
        });
    this.closeActionMenu_();
  },

  /**
   * Sets |this.selectedAccount_| as active Kerberos account.
   * @private
   */
  onSetAsActiveAccountClick_: function() {
    this.browserProxy_.setAsActiveAccount(
        /** @type {!settings.KerberosAccount} */ (this.selectedAccount_));
    this.closeActionMenu_();
  },

  /**
   * Opens the reauth dialog for |this.selectedAccount_|.
   * @private
   */
  onRefreshNowClick_: function() {
    this.showAddAccountDialog_ = true;
  },

  /**
   * Pops up a toast with localized text |label|.
   * @param {string} label Name of the localized label string.
   * @private
   */
  showToast_: function(label) {
    this.accountToastText_ = this.i18n(label);
    this.$$('#account-toast').show();
  }
});
