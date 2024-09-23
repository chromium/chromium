// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './prefs/pref_toggle_button.js';
import './user_utils_mixin.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import './dialogs/move_passwords_dialog.js';
import './dialogs/disconnect_cloud_authenticator_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MoveToAccountStoreTrigger} from './dialogs/move_passwords_dialog.js';
// <if expr="is_win or is_macosx">
import {PasskeysBrowserProxyImpl} from './passkeys_browser_proxy.js';
// </if>
import type {BlockedSite, BlockedSitesListChangedListener, CredentialsChangedListener} from './password_manager_proxy.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import type {PrefToggleButtonElement} from './prefs/pref_toggle_button.js';
import type {Route} from './router.js';
import {RouteObserverMixin, Router, UrlParam} from './router.js';
import {getTemplate} from './settings_section.html.js';
import {SyncBrowserProxyImpl, TrustedVaultBannerState} from './sync_browser_proxy.js';
import {UserUtilMixin} from './user_utils_mixin.js';

export interface SettingsSectionElement {
  $: {
    autosigninToggle: PrefToggleButtonElement,
    blockedSitesList: HTMLElement,
    passwordToggle: PrefToggleButtonElement,
    trustedVaultBanner: CrLinkRowElement,
    accountStorageToggle: PrefToggleButtonElement,
    toast: CrToastElement,
  };
}

const PASSWORD_MANAGER_ADD_SHORTCUT_ELEMENT_ID =
    'PasswordManagerUI::kAddShortcutElementId';
const PASSWORD_MANAGER_ADD_SHORTCUT_CUSTOM_EVENT_ID =
    'PasswordManagerUI::kAddShortcutCustomEventId';
export const PASSWORD_MANAGER_ACCOUNT_STORE_TOGGLE_ELEMENT_ID =
    'PasswordManagerUI::kAccountStoreToggleElementId';

const SettingsSectionElementBase = HelpBubbleMixin(RouteObserverMixin(
    PrefsMixin(UserUtilMixin(WebUiListenerMixin(I18nMixin(PolymerElement))))));

export class SettingsSectionElement extends SettingsSectionElementBase {
  static get is() {
    return 'settings-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** An array of blocked sites to display. */
      blockedSites_: {
        type: Array,
        value: () => [],
      },

      // <if expr="is_win or is_macosx or is_chromeos">
      isBiometricAuthenticationForFillingToggleVisible_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'biometricAuthenticationForFillingToggleVisible');
        },
      },
      // </if>

      hasPasswordsToExport_: {
        type: Boolean,
        value: false,
      },

      hasPasskeys_: {
        type: Boolean,
        value: false,
      },

      passwordManagerDisabled_: {
        type: Boolean,
        computed: 'computePasswordManagerDisabled_(' +
            'prefs.credentials_enable_service.enforcement, ' +
            'prefs.credentials_enable_service.value)',
      },

      /** The visibility state of the trusted vault banner. */
      trustedVaultBannerState_: {
        type: Object,
        value: TrustedVaultBannerState.NOT_SHOWN,
      },

      canAddShortcut_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('canAddShortcut');
        },
      },

      showMovePasswordsDialog_: Boolean,

      passwordsOnDevice_: {
        type: Array,
      },

      isPasswordManagerPinAvailable_: {
        type: Boolean,
        value: false,
      },

      isConnectedToCloudAuthenticator_: {
        type: Boolean,
        value: false,
      },

      isDisconnectCloudAuthenticatorInProgress_: {
        type: Boolean,
        value: false,
      },

      showDisconnectCloudAuthenticatorDialog_: {
        type: Boolean,
        value: false,
      },

      isDeleteAllPasswordManagerDataRowAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableWebAuthnGpmPin');
        },
      },
    };
  }

  static get observers() {
    return [
      'updateIsPasswordManagerPinAvailable_(isSyncingPasswords)',
      'updateIsCloudAuthenticatorConnected_(isSyncingPasswords)',
    ];
  }

  private blockedSites_: BlockedSite[];
  private hasPasskeys_: boolean;
  private hasPasswordsToExport_: boolean;
  private showPasswordsImporter_: boolean;
  private showMovePasswordsDialog_: boolean;
  private trustedVaultBannerState_: TrustedVaultBannerState;
  private movePasswordsLabel_: string;
  private passwordsOnDevice_: chrome.passwordsPrivate.PasswordUiEntry[] = [];
  private isPasswordManagerPinAvailable_: boolean = false;
  private isConnectedToCloudAuthenticator_: boolean = false;
  private isDisconnectCloudAuthenticatorInProgress_: boolean = false;
  private toastMessage_: string = '';
  private showDisconnectCloudAuthenticatorDialog_: boolean = false;
  private isDeleteAllPasswordManagerDataRowAvailable_: boolean;

  private setBlockedSitesListListener_: BlockedSitesListChangedListener|null =
      null;
  private setCredentialsChangedListener_: CredentialsChangedListener|null =
      null;

  override ready() {
    super.ready();

    chrome.metricsPrivate.recordBoolean(
        'PasswordManager.OpenedAsShortcut',
        window.matchMedia('(display-mode: standalone)').matches);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.updatePasswordsOnDevice_();
    this.setBlockedSitesListListener_ = blockedSites => {
      this.blockedSites_ = blockedSites;
    };
    PasswordManagerImpl.getInstance().getBlockedSitesList().then(
        blockedSites => this.blockedSites_ = blockedSites);
    PasswordManagerImpl.getInstance().addBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);

    this.setCredentialsChangedListener_ =
        (passwords: chrome.passwordsPrivate.PasswordUiEntry[]) => {
          this.hasPasswordsToExport_ = passwords.length > 0;
          this.updatePasswordsOnDevice_();
        };
    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setCredentialsChangedListener_);

    const trustedVaultStateChanged = (state: TrustedVaultBannerState) => {
      this.trustedVaultBannerState_ = state;
    };
    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();
    syncBrowserProxy.getTrustedVaultBannerState().then(
        trustedVaultStateChanged);
    this.addWebUiListener(
        'trusted-vault-banner-state-changed', trustedVaultStateChanged);
    // TODO(crbug.com/331611435): add listener for enclave availability and
    // trigger `updateIsPasswordManagerPinAvailable_`.
    this.updateIsPasswordManagerPinAvailable_();
    // Checks if the Chrome client is connected to / registered with the
    // Cloud Authenticator. If the client is connected, then a button to
    // disconnect the client is displayed.
    this.updateIsCloudAuthenticatorConnected_();

    // <if expr="is_win or is_macosx">
    PasskeysBrowserProxyImpl.getInstance().hasPasskeys().then(hasPasskeys => {
      this.hasPasskeys_ = hasPasskeys;
    });
    // </if>

    const accountStorageToggleRoot = this.$.accountStorageToggle.shadowRoot;
    this.registerHelpBubble(
        PASSWORD_MANAGER_ACCOUNT_STORE_TOGGLE_ELEMENT_ID,
        accountStorageToggleRoot!.querySelector('#control')!);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setBlockedSitesListListener_);
    PasswordManagerImpl.getInstance().removeBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);
    this.setBlockedSitesListListener_ = null;

    assert(this.setCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setCredentialsChangedListener_);
    this.setCredentialsChangedListener_ = null;
    this.$.toast.hide();
  }

  override currentRouteChanged(route: Route): void {
    const triggerImportParam =
        route.queryParameters.get(UrlParam.START_IMPORT) || '';
    if (triggerImportParam === 'true') {
      const importer = this.shadowRoot!.querySelector('passwords-importer');
      assert(importer);
      importer.launchImport();
      const params = new URLSearchParams();
      Router.getInstance().updateRouterParams(params);
    }
  }

  private onShortcutBannerDomChanged_() {
    const addShortcutBanner = this.root!.querySelector('#addShortcutBanner');
    if (addShortcutBanner) {
      this.registerHelpBubble(
          PASSWORD_MANAGER_ADD_SHORTCUT_ELEMENT_ID, addShortcutBanner);
    }
  }

  private onAddShortcutClick_() {
    this.notifyHelpBubbleAnchorCustomEvent(
        PASSWORD_MANAGER_ADD_SHORTCUT_ELEMENT_ID,
        PASSWORD_MANAGER_ADD_SHORTCUT_CUSTOM_EVENT_ID,
    );
    // TODO(crbug.com/40236982): Record metrics on all entry points usage.
    // TODO(crbug.com/40236982): Hide the button for users after the shortcut is
    // installed.
    PasswordManagerImpl.getInstance().showAddShortcutDialog();
  }

  /**
   * Fires an event that should delete the blocked password entry.
   */
  private onRemoveBlockedSiteClick_(
      event: DomRepeatEvent<chrome.passwordsPrivate.ExceptionEntry>) {
    PasswordManagerImpl.getInstance().removeBlockedSite(event.model.item.id);
  }

  // <if expr="is_win or is_macosx or is_chromeos">
  private switchBiometricAuthBeforeFillingState_(e: Event) {
    const biometricAuthenticationForFillingToggle =
        e!.target as PrefToggleButtonElement;
    assert(biometricAuthenticationForFillingToggle);
    PasswordManagerImpl.getInstance().switchBiometricAuthBeforeFillingState();
  }
  // </if>

  private onTrustedVaultBannerClick_() {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        OpenWindowProxyImpl.getInstance().openUrl(
            loadTimeData.getString('trustedVaultLearnMoreUrl'));
        break;
      case TrustedVaultBannerState.OFFER_OPT_IN:
        OpenWindowProxyImpl.getInstance().openUrl(
            loadTimeData.getString('trustedVaultOptInUrl'));
        break;
      case TrustedVaultBannerState.NOT_SHOWN:
      default:
        assertNotReached();
    }
  }

  private getTrustedVaultBannerTitle_(): string {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        return this.i18n('trustedVaultBannerLabelOptedIn');
      case TrustedVaultBannerState.OFFER_OPT_IN:
        return this.i18n('trustedVaultBannerLabelOfferOptIn');
      case TrustedVaultBannerState.NOT_SHOWN:
        return '';
      default:
        assertNotReached();
    }
  }

  private getTrustedVaultBannerDescription_(): string {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        return this.i18n('trustedVaultBannerSubLabelOptedIn');
      case TrustedVaultBannerState.OFFER_OPT_IN:
        return this.i18n('trustedVaultBannerSubLabelOfferOptIn');
      case TrustedVaultBannerState.NOT_SHOWN:
        return '';
      default:
        assertNotReached();
    }
  }

  private shouldHideTrustedVaultBanner_(): boolean {
    return this.trustedVaultBannerState_ === TrustedVaultBannerState.NOT_SHOWN;
  }

  private getAriaLabelForBlockedSite_(
      blockedSite: chrome.passwordsPrivate.ExceptionEntry): string {
    return this.i18n('removeBlockedAriaDescription', blockedSite.urls.shown);
  }

  private changeAccountStorageEnabled_() {
    if (this.isAccountStorageEnabled) {
      this.disableAccountStorage();
    } else {
      this.enableAccountStorage();
    }
  }

  private getAccountStorageSubLabel_(accountEmail: string): string {
    return this.i18n('accountStorageToggleSubLabel', accountEmail);
  }

  // <if expr="is_win or is_macosx">
  private onManagePasskeysClick_() {
    PasskeysBrowserProxyImpl.getInstance().managePasskeys();
  }
  // </if>

  private computePasswordManagerDisabled_(): boolean {
    const pref = this.getPref('credentials_enable_service');

    const isPolicyEnforced =
        pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;

    const isPolicyControlledByExtension =
        pref.controlledBy === chrome.settingsPrivate.ControlledBy.EXTENSION;

    if (isPolicyControlledByExtension) {
      return false;
    }

    return !pref.value && isPolicyEnforced;
  }

  private onMovePasswordsClicked_(e: Event) {
    e.preventDefault();
    this.showMovePasswordsDialog_ = true;
  }

  private onMovePasswordsDialogClose_() {
    this.showMovePasswordsDialog_ = false;
  }

  private getMovePasswordsDialogTrigger_(): MoveToAccountStoreTrigger {
    return MoveToAccountStoreTrigger
        .EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS;
  }

  private shouldShowMovePasswordsEntry_(): boolean {
    return this.isAccountStoreUser && this.passwordsOnDevice_.length > 0;
  }

  private async updatePasswordsOnDevice_() {
    const groups =
        await PasswordManagerImpl.getInstance().getCredentialGroups();
    const localStorage = [
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT,
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
    ];

    this.passwordsOnDevice_ =
        groups.map(group => group.entries)
            .flat()
            .filter(entry => localStorage.includes(entry.storedIn));

    this.movePasswordsLabel_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'deviceOnlyPasswordsIconTooltip', this.passwordsOnDevice_.length);
  }

  private updateIsPasswordManagerPinAvailable_() {
    PasswordManagerImpl.getInstance().isPasswordManagerPinAvailable().then(
        available => this.isPasswordManagerPinAvailable_ =
            available && this.isSyncingPasswords);
  }

  private onChangePasswordManagerPinRowClick_() {
    PasswordManagerImpl.getInstance().changePasswordManagerPin().then(
        this.showToastForPasswordChange_.bind(this));
  }

  private updateIsCloudAuthenticatorConnected_() {
    PasswordManagerImpl.getInstance().isConnectedToCloudAuthenticator().then(
        connected => this.isConnectedToCloudAuthenticator_ =
            connected && this.isSyncingPasswords);
  }

  private onDisconnectCloudAuthenticatorClick_() {
    this.showDisconnectCloudAuthenticatorDialog_ = true;
  }

  private onCloseDisconnectCloudAuthenticatorDialog_(): void {
    this.showDisconnectCloudAuthenticatorDialog_ = false;
  }

  private onDisconnectCloudAuthenticator_(e: CustomEvent): void {
    this.isDisconnectCloudAuthenticatorInProgress_ = false;
    this.updateIsCloudAuthenticatorConnected_();
    this.updateIsPasswordManagerPinAvailable_();
    if (e.detail.success) {
      this.showToastForCloudAuthenticatorDisconnected_();
    }
  }

  private showToastForCloudAuthenticatorDisconnected_(): void {
    this.toastMessage_ = this.i18n('disconnectCloudAuthenticatorToastMessage');
    this.$.toast.show();
  }

  private getAriaLabelForCloudAuthenticatorButton_(): string {
    return [
      this.i18n('disconnectCloudAuthenticatorTitle'),
      this.i18n('disconnectCloudAuthenticatorDescription'),
    ].join('. ');
  }

  private showToastForPasswordChange_(success: boolean): void {
    if (!success) {
      return;
    }
    this.toastMessage_ = this.i18n('passwordManagerPinChanged');
    this.$.toast.show();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);
