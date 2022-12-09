// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-account-manager' is the settings subpage containing controls to
 * list, add and delete Secondary Google Accounts.
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {getImage} from '../icon.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from '../parental_controls_page/parental_controls_browser_proxy.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {Account, AccountManagerBrowserProxy, AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsAccountManagerElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      WebUIListenerBehavior,
      RouteObserverBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsAccountManagerElement extends SettingsAccountManagerElementBase {
  static get is() {
    return 'settings-account-manager';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * List of Accounts.
       * @type {!Array<Account>}
       */
      accounts_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Primary / Device account.
       * @private {?Account}
       */
      deviceAccount_: Object,

      /**
       * The targeted account for menu operations.
       * @private {?Account}
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
       * @return {boolean} True if `kArcAccountRestrictionsEnabled` feature is
       *     enabled, false otherwise.
       * @private
       */
      isArcAccountRestrictionsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('arcAccountRestrictionsEnabled');
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kAddAccount,
          Setting.kRemoveAccount,
        ]),
      },
    };
  }

  constructor() {
    super();

    /** @private {!AccountManagerBrowserProxy} */
    this.browserProxy_ = AccountManagerBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener('accounts-changed', this.refreshAccounts_.bind(this));
  }

  /** @override */
  ready() {
    super.ready();
    this.refreshAccounts_();
  }

  /**
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute !== routes.ACCOUNT_MANAGER) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @return {string} account manager description text.
   * @private
   */
  getAccountManagerDescription_() {
    if (this.isChildUser_ && this.isSecondaryGoogleAccountSigninAllowed_) {
      return loadTimeData.getString('accountManagerChildDescription');
    }
    return loadTimeData.getString('accountManagerDescription');
  }

  /**
   * @return {string} account manager 'add account' label.
   * @private
   */
  getAddAccountLabel_() {
    if (this.isChildUser_ && this.isSecondaryGoogleAccountSigninAllowed_) {
      return loadTimeData.getString('addSchoolAccountLabel');
    }
    return loadTimeData.getString('addAccountLabel');
  }

  /**
   * @return {string} accounts list header (e.g. 'Secondary accounts' for
   *     regular users or 'School accounts' for child users).
   * @private
   */
  getAccountListHeader_() {
    return this.isChildUser_ ?
        loadTimeData.getString('accountListHeaderChild') :
        loadTimeData.getString('accountListHeader');
  }

  /**
   * @return {string} accounts list description.
   * @private
   */
  getAccountListDescription_() {
    return this.isChildUser_ ?
        loadTimeData.getString('accountListChildDescription') :
        loadTimeData.getString('accountListDescription');
  }

  /**
   * @return {string} 'Secondary Accounts disabled' message depending on
   *    account type
   * @private
   */
  getSecondaryAccountsDisabledUserMessage_() {
    return this.isChildUser_
      ? this.i18n('accountManagerSecondaryAccountsDisabledChildText')
      : this.i18n('accountManagerSecondaryAccountsDisabledText');
  }

  /**
   * @return {string} class name for account list header class.
   * @private
   */
  getAccountListHeaderClass_() {
    return this.isArcAccountRestrictionsEnabled_ ?
        'account-list-header-description with-padding' :
        'account-list-header-description';
  }

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return getImage(iconUrl);
  }

  /**
   * @param {!Event} event
   * @private
   */
  addAccount_(event) {
    recordSettingChange(
        Setting.kAddAccount, {intValue: this.accounts_.length + 1});
    this.browserProxy_.addAccount();
  }

  /**
   * @param {!Account} account
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
  }

  /**
   * @return {boolean} True if managed badge should be shown next to the device
   *     account picture.
   * @private
   */
  shouldShowManagedBadge_() {
    return this.isDeviceAccountManaged_ && !this.isChildUser_;
  }

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
  }

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
    // Where href will be set by <localized-link>.
    return loadTimeData.getStringF(
        'accountManagerManagementDescription',
        this.deviceAccount_.organization);
  }

  /**
   * @param {boolean} unmigrated
   * @private
   */
  getAccountManagerSignedOutName_(unmigrated) {
    return this.i18n(unmigrated ? 'accountManagerUnmigratedAccountName'
                                : 'accountManagerSignedOutAccountName');
  }

  /**
   * @param {boolean} unmigrated
   * @private
   */
  getAccountManagerSignedOutLabel_(unmigrated) {
    return this.i18n(unmigrated ? 'accountManagerMigrationLabel'
                                : 'accountManagerReauthenticationLabel');
  }


  /**
   * @param {!Account} account
   * @private
   */
  getAccountManagerSignedOutTitle_(account) {
    const label = account.unmigrated ? 'accountManagerMigrationTooltip'
                                     : 'accountManagerReauthenticationTooltip';
    return loadTimeData.getStringF(label, account.email);
  }

  /**
   * @param {!Account} account
   * @private
   */
  getMoreActionsTitle_(account) {
    return loadTimeData.getStringF('accountManagerMoreActionsTooltip',
                                    account.email);
  }

  /**
   * @return {!Array<Account>} list of accounts.
   * @private
   */
  getSecondaryAccounts_() {
    return this.accounts_.filter(account => !account.isDeviceAccount);
  }

  /**
   * @param {!CustomEvent<!{model: !{item: !Account}}>} event
   * @private
   */
  onReauthenticationTap_(event) {
    if (event.model.item.unmigrated) {
      this.browserProxy_.migrateAccount(event.model.item.email);
    } else {
      this.browserProxy_.reauthenticateAccount(event.model.item.email);
    }
  }

  /**
   * @private
   *
   * TODO(crbug/1315757) ParentalControlsBrowserProxy is in TS so
   * suppress the closure compilation error for launchFamilyLinkSettings()
   * until this element is converted to TS.
   * @suppress {missingProperties}
   */
  onManagedIconClick_() {
    if (this.isChildUser_) {
      ParentalControlsBrowserProxyImpl.getInstance().launchFamilyLinkSettings();
    }
  }

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
  }

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: Account}, target: !HTMLElement}} event
   * @private
   */
  onAccountActionsMenuButtonTap_(event) {
    this.actionMenuAccount_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (
        this.shadowRoot.querySelector('cr-action-menu'))
        .showAt(event.target);
  }

  /**
   * If Lacros is not enabled, removes the account pointed to by
   * |this.actionMenuAccount_|.
   * If Lacros is enabled, shows a warning dialog that the user needs to
   * confirm before removing the account.
   * @private
   */
  onRemoveAccountTap_() {
    this.shadowRoot.querySelector('cr-action-menu').close();
    if (loadTimeData.getBoolean('lacrosEnabled') &&
        this.actionMenuAccount_.isManaged) {
      this.$.removeConfirmationDialog.showModal();
    } else {
      this.browserProxy_.removeAccount(
          /** @type {?Account} */ (this.actionMenuAccount_));
      this.actionMenuAccount_ = null;
      this.shadowRoot.querySelector('#add-account-button').focus();
    }
  }

  /**
   * The user chooses not to remove the account after seeing the warning
   * dialog, and taps the cancel button.
   * @private
   */
  onRemoveAccountDialogCancelTap_() {
    this.actionMenuAccount_ = null;
    this.$.removeConfirmationDialog.cancel();
    this.shadowRoot.querySelector('#add-account-button').focus();
  }

  /**
   * After seeing the warning dialog, the user chooses to removes the account
   * pointed to by |this.actionMenuAccount_|, and taps the remove button.
   * @private
   */
  onRemoveAccountDialogRemoveTap_() {
    this.browserProxy_.removeAccount(
        /** @type {?Account} */ (this.actionMenuAccount_));
    this.actionMenuAccount_ = null;
    this.$.removeConfirmationDialog.close();
    this.shadowRoot.querySelector('#add-account-button').focus();
  }

  /**
   * Get the test for button that changes ARC availability.
   * @private
   */
  getChangeArcAvailabilityLabel_() {
    if (!this.actionMenuAccount_) {
      return '';
    }
    return this.actionMenuAccount_.isAvailableInArc ?
        this.i18n('accountStopUsingInArcButtonLabel') :
        this.i18n('accountUseInArcButtonLabel');
  }

  /**
   * Change ARC availability for |this.actionMenuAccount_|.
   * Closes the 'More actions' menu and focuses the 'More actions' button for
   * |this.actionMenuAccount_|.
   * @private
   */
  onChangeArcAvailability_() {
    this.shadowRoot.querySelector('cr-action-menu').close();
    const newArcAvailability = !this.actionMenuAccount_.isAvailableInArc;
    this.browserProxy_.changeArcAvailability(
        this.actionMenuAccount_, newArcAvailability);

    const actionMenuAccountIndex =
        this.shadowRoot.querySelector('#account-list')
            .items.indexOf(this.actionMenuAccount_);
    if (actionMenuAccountIndex >= 0) {
      // Focus 'More actions' button for the current account.
      this.shadowRoot
          .querySelectorAll('.icon-more-vert')[actionMenuAccountIndex]
          .focus();
    } else {
      console.error(
          'Couldn\'t find active account in the list: ',
          this.actionMenuAccount_);
      this.shadowRoot.querySelector('#add-account-button').focus();
    }
    this.actionMenuAccount_ = null;
  }
}

customElements.define(
    SettingsAccountManagerElement.is, SettingsAccountManagerElement);
