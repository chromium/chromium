// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-account-manager' is the settings subpage containing controls to
 * list, add and delete Secondary Google Accounts.
 */

Polymer({
  is: 'settings-account-manager',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * List of Accounts.
     * @type {!Array<settings.Account>}
     */
    accounts_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * Primary / Device account.
     * @private {?settings.Account}
     */
    deviceAccount_: Object,

    /**
     * The targeted account for menu operations.
     * @private {?settings.Account}
     */
    actionMenuAccount_: Object,

    /** @private {boolean} */
    isChildUser_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isChild');
      },
    },

    /**
     * True if device account is managed.
     * @private {boolean}
     */
    isDeviceAccountManaged_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isDeviceAccountManaged');
      },
      readOnly: true,
    },

    /**
     * True if redesign of account management flows is enabled.
     * @private
     */
    isAccountManagementFlowsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled');
      },
      readOnly: true,
    },

    /**
     * @return {boolean} True if secondary account sign-ins are allowed, false
     *    otherwise.
     * @private
     */
    isSecondaryGoogleAccountSigninAllowed_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('secondaryGoogleAccountSigninAllowed');
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kAddAccount,
        chromeos.settings.mojom.Setting.kRemoveAccount,
      ]),
    },
  },

  /** @private {?settings.AccountManagerBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached() {
    this.addWebUIListener('accounts-changed', this.refreshAccounts_.bind(this));
  },

  /** @override */
  ready() {
    this.browserProxy_ = settings.AccountManagerBrowserProxyImpl.getInstance();
    this.refreshAccounts_();
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute !== settings.routes.ACCOUNT_MANAGER) {
      return;
    }

    this.browserProxy_.showWelcomeDialogIfRequired();
    this.attemptDeepLink();
  },

  /**
   * @return {string} account manager description text.
   * @private
   */
  getAccountManagerDescription_() {
    if (this.isChildUser_ && this.isSecondaryGoogleAccountSigninAllowed_) {
      return loadTimeData.getString('accountManagerChildDescription');
    }
    return loadTimeData.getString('accountManagerDescription');
  },

  /**
   * @return {boolean} whether the additional messages for child user should be
   *     shown.
   * @private
   */
  showChildMessage_() {
    return this.isChildUser_ && !this.isAccountManagementFlowsV2Enabled_;
  },

  /**
   * @return {string} account manager 'add account' label.
   * @private
   */
  getAddAccountLabel_() {
    if (this.isChildUser_ && this.isSecondaryGoogleAccountSigninAllowed_) {
      return loadTimeData.getString('addSchoolAccountLabel');
    }
    return loadTimeData.getString('addAccountLabel');
  },

  /**
   * @return {string} accounts list header (e.g. 'Secondary accounts' for
   *     regular users or 'School accounts' for child users).
   * @private
   */
  getAccountListHeader_() {
    if (this.isAccountManagementFlowsV2Enabled_ && this.isChildUser_) {
      return loadTimeData.getString('accountListHeaderChild');
    }
    return loadTimeData.getString('accountListHeader');
  },

  /**
   * @return {string} accounts list description.
   * @private
   */
  getAccountListDescription_() {
    return this.isChildUser_ ?
        loadTimeData.getString('accountListChildDescription') :
        loadTimeData.getString('accountListDescription');
  },

  /**
   * @return {boolean} whether 'Secondary Accounts disabled' tooltip should be
   *     shown.
   * @private
   */
  showSecondaryAccountsDisabledTooltip_() {
    return this.isAccountManagementFlowsV2Enabled_ &&
        !this.isSecondaryGoogleAccountSigninAllowed_;
  },

  /**
   * @return {string} 'Secondary Accounts disabled' message depending on
   *    account type
   * @private
   */
  getSecondaryAccountsDisabledUserMessage_() {
    return this.isChildUser_
      ? this.i18n('accountManagerSecondaryAccountsDisabledChildText')
      : this.i18n('accountManagerSecondaryAccountsDisabledText');
  },

  /**
   * @return {string} cr icon name.
   * @private
   */
  getPrimaryAccountTooltipIcon_() {
    return this.isChildUser_ ? 'cr20:kite' : 'cr:info-outline';
  },

  /**
   * @return {string} tooltip text
   * @private
   */
  getPrimaryAccountTooltip_() {
    return this.isChildUser_ ?
        this.i18n('accountManagerPrimaryAccountChildManagedTooltip') :
        this.i18n('accountManagerPrimaryAccountTooltip');
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!Event} event
   * @private
   */
  addAccount_(event) {
    settings.recordSettingChange(
        chromeos.settings.mojom.Setting.kAddAccount,
        {intValue: this.accounts_.length + 1});
    this.browserProxy_.addAccount();
  },

  /**
   * @param {!settings.Account} account
   * @return {boolean} True if the account reauthentication button should be
   *    shown, false otherwise.
   * @private
   */
  shouldShowReauthenticationButton_(account) {
    // Device account re-authentication cannot be handled in-session, primarily
    // because the user may have changed their password (leading to an LST
    // invalidation) and we do not have a mechanism to change the cryptohome
    // password in-session.
    return !account.isDeviceAccount && !account.isSignedIn;
  },

  /**
   * @return {boolean} True if 'School account' label should be displayed for
   *     secondary accounts.
   * @private
   */
  shouldDisplayEduSecondaryAccountLabel_() {
    return this.isChildUser_ && !this.isAccountManagementFlowsV2Enabled_;
  },

  /**
   * @return {boolean} True if managed badge should be shown next to the device
   *     account picture.
   * @private
   */
  shouldShowManagedBadge_() {
    return this.isDeviceAccountManaged_ && !this.isChildUser_;
  },

  /**
   * @param {!settings.Account} account
   * @return {string} An appropriate management status label. e.g.
   *    "Primary account" for unmanaged accounts, "Managed by <Domain>"
   *    for Enterprise managed accounts etc.
   * @private
   */
  getManagementLabel_(account) {
    if (account.organization) {
      return this.i18n('accountManagerManagedLabel', account.organization);
    }

    return this.i18n('accountManagerUnmanagedLabel');
  },

  /**
   * @return {string} icon
   * @private
   */
  getManagedAccountTooltipIcon_() {
    if (this.isChildUser_) {
      return 'cr20:kite';
    }
    if (this.isDeviceAccountManaged_) {
      return 'cr20:domain';
    }
    return '';
  },

  /**
   * @return {string} description text
   * @private
   */
  getManagementDescription_() {
    if (this.isChildUser_) {
      return loadTimeData.getString('accountManagerManagementDescription');
    }
    if (!this.deviceAccount_) {
      return '';
    }
    if (!this.deviceAccount_.organization) {
      if (this.isDeviceAccountManaged_) {
        console.error(
            'The device account is managed, but the organization is not set.');
      }
      return '';
    }
    // Format: 'This account is managed by
    //          <a target="_blank" href="chrome://management">google.com</a>'.
    // Where href will be set by <settings-localized-link>.
    return loadTimeData.getStringF(
        'accountManagerManagementDescription',
        this.deviceAccount_.organization);
  },

  /**
   * @param {boolean} unmigrated
   * @private
   */
  getAccountManagerSignedOutName_(unmigrated) {
    return this.i18n(unmigrated ? 'accountManagerUnmigratedAccountName'
                                : 'accountManagerSignedOutAccountName');
  },

  /**
   * @param {boolean} unmigrated
   * @private
   */
  getAccountManagerSignedOutLabel_(unmigrated) {
    return this.i18n(unmigrated ? 'accountManagerMigrationLabel'
                                : 'accountManagerReauthenticationLabel');
  },


  /**
   * @param {!settings.Account} account
   * @private
   */
  getAccountManagerSignedOutTitle_(account) {
    const label = account.unmigrated ? 'accountManagerMigrationTooltip'
                                     : 'accountManagerReauthenticationTooltip';
    return loadTimeData.getStringF(label, account.email);
  },

  /**
   * @param {!settings.Account} account
   * @private
   */
  getMoreActionsTitle_(account) {
    return loadTimeData.getStringF('accountManagerMoreActionsTooltip',
                                    account.email);
  },

  /**
   * @return {!Array<settings.Account>} list of accounts.
   * @private
   */
  getAccounts_() {
    // TODO(crbug.com/1152711): rename the method to `getSecondaryAccounts_`.
    if (this.isAccountManagementFlowsV2Enabled_) {
      // Return only secondary accounts.
      return this.accounts_.filter(account => !account.isDeviceAccount);
    }

    return this.accounts_;
  },


  /**
   * @param {string} classList existing class list.
   * @return {string} new class list.
   * @private
   */
  getAccountManagerDescriptionClassList_(classList) {
    if (this.isAccountManagementFlowsV2Enabled_) {
      return classList + ' full-width';
    }
    return classList;
  },

  /**
   * @param {!CustomEvent<!{model: !{item: !settings.Account}}>} event
   * @private
   */
  onReauthenticationTap_(event) {
    if (event.model.item.unmigrated) {
      this.browserProxy_.migrateAccount(event.model.item.email);
    } else {
      this.browserProxy_.reauthenticateAccount(event.model.item.email);
    }
  },

  /** @private */
  onManagedIconClick_() {
    if (this.isChildUser_) {
      parental_controls.ParentalControlsBrowserProxyImpl.getInstance()
          .launchFamilyLinkSettings();
    }
  },

  /**
   * @private
   */
  refreshAccounts_() {
    this.browserProxy_.getAccounts().then(accounts => {
      this.set('accounts_', accounts);
      const deviceAccount = accounts.find(account => account.isDeviceAccount);
      if (!deviceAccount) {
        console.error('Cannot find device account.');
        return;
      }
      this.deviceAccount_ = deviceAccount;
    });
  },

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: settings.Account}, target: !Element}} event
   * @private
   */
  onAccountActionsMenuButtonTap_(event) {
    this.actionMenuAccount_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(event.target);
  },

  /**
   * Closes action menu and resets action menu model.
   * @private
   */
  closeActionMenu_() {
    this.$$('cr-action-menu').close();
    this.actionMenuAccount_ = null;
  },

  /**
   * Removes the account being pointed to by |this.actionMenuAccount_|.
   * @private
   */
  onRemoveAccountTap_() {
    this.browserProxy_.removeAccount(
        /** @type {?settings.Account} */ (this.actionMenuAccount_));
    this.closeActionMenu_();
    this.$$('#add-account-button').focus();
  },
});
