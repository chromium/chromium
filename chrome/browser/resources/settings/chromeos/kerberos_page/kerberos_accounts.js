// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-accounts' is the settings subpage containing controls to
 * list, add and delete Kerberos Accounts.
 */

'use strict';

import {afterNextRender, Polymer, html, flush, Templatizer, TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {getImage} from '//resources/js/icon.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import {loadTimeData} from '../../i18n_setup.js';
import {Account} from '../os_people_page/account_manager_browser_proxy.js';
import {Router, Route} from '../../router.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';
import '../../settings_shared_css.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {recordSettingChange, recordSearch, setUserActionRecorderForTesting, recordPageFocus, recordPageBlur, recordClick, recordNavigation} from '../metrics_recorder.m.js';
import {routes} from '../os_route.m.js';
import {KerberosAccount, KerberosAccountsBrowserProxyImpl, KerberosAccountsBrowserProxy, KerberosErrorType, KerberosConfigErrorCode, ValidateKerberosConfigResult} from './kerberos_accounts_browser_proxy.js';
import './kerberos_add_account_dialog.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-kerberos-accounts',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of Accounts.
     * @private {!Array<!KerberosAccount>}
     */
    accounts_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * Whether dark mode is currently active.
     * @private
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },

    /**
     * The targeted account for menu and other operations.
     * @private {?KerberosAccount}
     */
    selectedAccount_: Object,

    /** @private */
    showAddAccountDialog_: Boolean,

    /** @private */
    addAccountsAllowed_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('kerberosAddAccountsAllowed');
      },
    },

    /** @private */
    accountToastText_: {
      type: String,
      value: '',
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kAddKerberosTicketV2,
        chromeos.settings.mojom.Setting.kRemoveKerberosTicketV2,
        chromeos.settings.mojom.Setting.kSetActiveKerberosTicketV2,
      ]),
    },
  },

  /** @private {?KerberosAccountsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached() {
    this.addWebUIListener(
        'kerberos-accounts-changed', this.refreshAccounts_.bind(this));
  },

  /** @override */
  ready() {
    this.browserProxy_ = KerberosAccountsBrowserProxyImpl.getInstance();

    // Grab account list and - when done - pop up the reauthentication dialog if
    // there is a kerberos_reauth param.
    this.refreshAccounts_().then(() => {
      const queryParams = Router.getInstance().getQueryParameters();
      const reauthPrincipal = queryParams.get('kerberos_reauth');
      const reauthAccount = this.accounts_.find(account => {
        return account.principalName === reauthPrincipal;
      });
      if (reauthAccount) {
        this.selectedAccount_ = reauthAccount;
        this.showAddAccountDialog_ = true;
      }
    });
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.KERBEROS_ACCOUNTS_V2) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @return {string} the icon to use for the error badge.
   * @private
   */
  getErrorBadgeIcon_() {
    return this.isDarkModeActive_ ?
        'chrome://os-settings/images/error_badge_dark.svg' :
        'chrome://os-settings/images/error_badge.svg';
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return getImage(iconUrl);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAddAccountClick_(event) {
    this.selectedAccount_ = null;
    this.showAddAccountDialog_ = true;
  },

  /**
   * @param {!CustomEvent<!{model: !{item: !Account}}>} event
   * @private
   */
  onReauthenticationClick_(event) {
    this.selectedAccount_ = event.model.item;
    this.showAddAccountDialog_ = true;
  },

  /** @private */
  onAddAccountDialogClosed_() {
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
  refreshAccounts_() {
    return this.browserProxy_.getAccounts().then(accounts => {
      this.accounts_ = accounts;
    });
  },

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: !KerberosAccount}, target: !Element}}
   *      event
   * @private
   */
  onAccountActionsMenuButtonClick_(event) {
    this.selectedAccount_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(event.target);
  },

  /**
   * Closes action menu and resets action menu model.
   * @private
   */
  closeActionMenu_() {
    this.$$('cr-action-menu').close();
    this.selectedAccount_ = null;
  },

  /**
   * Removes |this.selectedAccount_|.
   * @private
   */
  onRemoveAccountClick_() {
    this.browserProxy_
        .removeAccount(
            /** @type {!KerberosAccount} */ (this.selectedAccount_))
        .then(error => {
          if (error === KerberosErrorType.kNone) {
            this.showToast_('kerberosAccountsAccountRemovedTip');
          } else {
            console.error('Unexpected error removing account: ' + error);
          }
        });
    recordSettingChange();
    this.closeActionMenu_();
  },

  /**
   * Sets |this.selectedAccount_| as active Kerberos account.
   * @private
   */
  onSetAsActiveAccountClick_() {
    this.browserProxy_.setAsActiveAccount(
        /** @type {!KerberosAccount} */ (this.selectedAccount_));
    recordSettingChange();
    this.closeActionMenu_();
  },

  /**
   * Opens the reauth dialog for |this.selectedAccount_|.
   * @private
   */
  onRefreshNowClick_() {
    this.showAddAccountDialog_ = true;
  },

  /**
   * Pops up a toast with localized text |label|.
   * @param {string} label Name of the localized label string.
   * @private
   */
  showToast_(label) {
    this.accountToastText_ = this.i18n(label);
    this.$$('#account-toast').show();
  }
});
