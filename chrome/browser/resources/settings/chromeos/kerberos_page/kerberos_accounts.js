// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-accounts' is the settings subpage containing controls to
 * list, add and delete Kerberos Accounts.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../../settings_shared.css.js';
import './kerberos_add_account_dialog.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Account} from '../os_people_page/account_manager_browser_proxy.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {KerberosAccount, KerberosAccountsBrowserProxy, KerberosAccountsBrowserProxyImpl, KerberosErrorType} from './kerberos_accounts_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsKerberosAccountsElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteObserverBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsKerberosAccountsElement extends
    SettingsKerberosAccountsElementBase {
  static get is() {
    return 'settings-kerberos-accounts';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kAddKerberosTicketV2,
          Setting.kRemoveKerberosTicketV2,
          Setting.kSetActiveKerberosTicketV2,
        ]),
      },

    };
  }

  constructor() {
    super();

    /** @private {!KerberosAccountsBrowserProxy} */
    this.browserProxy_ = KerberosAccountsBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'kerberos-accounts-changed', this.refreshAccounts_.bind(this));
  }

  /** @override */
  ready() {
    super.ready();

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
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.KERBEROS_ACCOUNTS_V2) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @return {string} the icon to use for the error badge.
   * @private
   */
  getErrorBadgeIcon_() {
    return this.isDarkModeActive_ ?
        'chrome://os-settings/images/error_badge_dark.svg' :
        'chrome://os-settings/images/error_badge.svg';
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
  onAddAccountClick_(event) {
    this.selectedAccount_ = null;
    this.showAddAccountDialog_ = true;
  }

  /**
   * @param {!CustomEvent<!{model: !{item: !Account}}>} event
   * @private
   */
  onReauthenticationClick_(event) {
    this.selectedAccount_ = event.model.item;
    this.showAddAccountDialog_ = true;
  }

  /** @private */
  onAddAccountDialogClosed_() {
    if (this.shadowRoot.querySelector('kerberos-add-account-dialog')
            .accountWasRefreshed) {
      this.showToast_('kerberosAccountsAccountRefreshedTip');
    }

    this.showAddAccountDialog_ = false;

    // In case it was opened by the 'Refresh now' action menu.
    this.closeActionMenu_();
  }

  /**
   * @return {!Promise}
   * @private
   */
  refreshAccounts_() {
    return this.browserProxy_.getAccounts().then(accounts => {
      this.accounts_ = accounts;
    });
  }

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: !KerberosAccount}, target: !HTMLElement}}
   *      event
   * @private
   */
  onAccountActionsMenuButtonClick_(event) {
    this.selectedAccount_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (
        this.shadowRoot.querySelector('cr-action-menu'))
        .showAt(event.target);
  }

  /**
   * Closes action menu and resets action menu model.
   * @private
   */
  closeActionMenu_() {
    this.shadowRoot.querySelector('cr-action-menu').close();
    this.selectedAccount_ = null;
  }

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
  }

  /**
   * Sets |this.selectedAccount_| as active Kerberos account.
   * @private
   */
  onSetAsActiveAccountClick_() {
    this.browserProxy_.setAsActiveAccount(
        /** @type {!KerberosAccount} */ (this.selectedAccount_));
    recordSettingChange();
    this.closeActionMenu_();
  }

  /**
   * Opens the reauth dialog for |this.selectedAccount_|.
   * @private
   */
  onRefreshNowClick_() {
    this.showAddAccountDialog_ = true;
  }

  /**
   * Pops up a toast with localized text |label|.
   * @param {string} label Name of the localized label string.
   * @private
   */
  showToast_(label) {
    this.accountToastText_ = this.i18n(label);
    this.shadowRoot.querySelector('#account-toast').show();
  }
}

customElements.define(
    SettingsKerberosAccountsElement.is, SettingsKerberosAccountsElement);
