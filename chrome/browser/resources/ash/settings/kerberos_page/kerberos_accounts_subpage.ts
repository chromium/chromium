// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-accounts-subpage' is the settings subpage containing
 * controls to list, add and delete Kerberos Accounts.
 */

import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../settings_shared.css.js';
import './kerberos_add_account_dialog.js';

import {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast, castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {KerberosAccount, KerberosAccountsBrowserProxy, KerberosAccountsBrowserProxyImpl, KerberosErrorType} from './kerberos_accounts_browser_proxy.js';
import {getTemplate} from './kerberos_accounts_subpage.html.js';

const SettingsKerberosAccountsSubpageElementBase = DeepLinkingMixin(
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

export class SettingsKerberosAccountsSubpageElement extends
    SettingsKerberosAccountsSubpageElementBase {
  static get is() {
    return 'settings-kerberos-accounts-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of Accounts.
       */
      accounts_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Whether dark mode is currently active.
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * The targeted account for menu and other operations.
       */
      selectedAccount_: Object,

      showAddAccountDialog_: Boolean,

      addAccountsAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('kerberosAddAccountsAllowed');
        },
      },

      accountToastText_: {
        type: String,
        value: '',
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kAddKerberosTicketV2,
          Setting.kRemoveKerberosTicketV2,
          Setting.kSetActiveKerberosTicketV2,
        ]),
      },
    };
  }

  private accountToastText_: string;
  private accounts_: KerberosAccount[];
  private addAccountsAllowed_: boolean;
  private isDarkModeActive_: boolean;
  private selectedAccount_: KerberosAccount|null;
  private showAddAccountDialog_: boolean;

  private browserProxy_: KerberosAccountsBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = KerberosAccountsBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'kerberos-accounts-changed', this.refreshAccounts_.bind(this));
  }

  override ready(): void {
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

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.KERBEROS_ACCOUNTS_V2) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @return The icon to use for the error badge.
   */
  private getErrorBadgeIcon_(): string {
    return this.isDarkModeActive_ ?
        'chrome://os-settings/images/error_badge_dark.svg' :
        'chrome://os-settings/images/error_badge.svg';
  }

  private getIconImageSet_(iconUrl: string): string {
    return getImage(iconUrl);
  }

  private onAddAccountClick_(): void {
    this.selectedAccount_ = null;
    this.showAddAccountDialog_ = true;
  }

  private onReauthenticationClick_(event: DomRepeatEvent<KerberosAccount>):
      void {
    this.selectedAccount_ = event.model.item;
    this.showAddAccountDialog_ = true;
  }

  private onAddAccountDialogClosed_(): void {
    if (this.shadowRoot!.querySelector('kerberos-add-account-dialog')!
            .accountWasRefreshed) {
      this.showToast_('kerberosAccountsAccountRefreshedTip');
    }

    this.showAddAccountDialog_ = false;

    // In case it was opened by the 'Refresh now' action menu.
    this.closeActionMenu_();
  }

  private refreshAccounts_(): Promise<void> {
    return this.browserProxy_.getAccounts().then(accounts => {
      this.accounts_ = accounts;
    });
  }

  /**
   * Opens the Account actions menu.
   */
  private onAccountActionsMenuButtonClick_(
      event: DomRepeatEvent<KerberosAccount>): void {
    this.selectedAccount_ = event.model.item;
    const target = cast(event.target, HTMLElement);
    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(target);
  }

  /**
   * Closes action menu and resets action menu model.
   */
  private closeActionMenu_(): void {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.selectedAccount_ = null;
  }

  /**
   * Removes |this.selectedAccount_|.
   */
  private onRemoveAccountClick_(): void {
    this.browserProxy_.removeAccount(castExists(this.selectedAccount_))
        .then(error => {
          if (error === KerberosErrorType.NONE) {
            this.showToast_('kerberosAccountsAccountRemovedTip');
            recordSettingChange(Setting.kRemoveKerberosTicketV2);
          } else {
            console.error('Unexpected error removing account: ' + error);
          }
        });
    this.closeActionMenu_();
  }

  /**
   * Sets |this.selectedAccount_| as active Kerberos account.
   */
  private onSetAsActiveAccountClick_(): void {
    this.browserProxy_.setAsActiveAccount(castExists(this.selectedAccount_));
    recordSettingChange(Setting.kSetActiveKerberosTicketV2);
    this.closeActionMenu_();
  }

  /**
   * Opens the reauth dialog for |this.selectedAccount_|.
   */
  private onRefreshNowClick_(): void {
    this.showAddAccountDialog_ = true;
  }

  /**
   * Pops up a toast with localized text |label|.
   * @param label Name of the localized label string.
   */
  private showToast_(label: string): void {
    this.accountToastText_ = this.i18n(label);
    this.shadowRoot!.querySelector<CrToastElement>('#account-toast')!.show();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsKerberosAccountsSubpageElement.is]:
        SettingsKerberosAccountsSubpageElement;
  }
}

customElements.define(
    SettingsKerberosAccountsSubpageElement.is,
    SettingsKerberosAccountsSubpageElement);
