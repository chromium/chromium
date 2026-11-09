// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './dialogs/disconnect_cloud_authenticator_dialog.js';
import './dialogs/remove_actor_login_permission_dialog.js';
import './full_data_reset.js';
import './passwords_exporter.js';
import './passwords_importer.js';
import './prefs/pref_toggle_button.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import './site_favicon.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// <if expr="is_win or is_macosx">
import {PasskeysBrowserProxyImpl} from './passkeys_browser_proxy.js';
// </if>
import type {ActorLoginPermission} from './password_manager.mojom-webui.js';
import {PasswordManagerActionableError} from './password_manager.mojom-webui.js';
import type {BlockedSite, BlockedSitesListChangedListener, CredentialsChangedListener} from './password_manager_proxy.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {isSavingNewPasswordsBlockedByMandatoryPolicy} from './prefs/policy_utils.js';
import type {PrefToggleButtonElement} from './prefs/pref_toggle_button.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixinLit, Router, UrlParam} from './router.js';
import {getCss} from './settings_section.css.js';
import {getHtml} from './settings_section.html.js';
import {BatchUploadPasswordsEntryPoint, SyncBrowserProxyImpl, TrustedVaultBannerState} from './sync_browser_proxy.js';
import {UserUtilMixinLit} from './user_utils_mixin_lit.js';

export interface SettingsSectionElement {
  $: {
    autosigninToggle: PrefToggleButtonElement,
    blockedSitesList: HTMLElement,
    passwordToggle: PrefToggleButtonElement,
    toast: CrToastElement,
    trustedVaultBanner: CrLinkRowElement,
  };
}

const PASSWORD_MANAGER_ADD_SHORTCUT_ELEMENT_ID =
    'PasswordManagerUI::kAddShortcutElementId';
const PASSWORD_MANAGER_ADD_SHORTCUT_CUSTOM_EVENT_ID =
    'PasswordManagerUI::kAddShortcutCustomEventId';

const SettingsSectionElementBase =
    HelpBubbleMixinLit(RouteObserverMixinLit(PrefServiceObserverMixinLit(
        UserUtilMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement))))));

export class SettingsSectionElement extends SettingsSectionElementBase {
  static get is() {
    return 'settings-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** An array of blocked sites to display. */
      blockedSites_: {type: Array},

      /** An array of sites with permissions for actor login. */
      actorLoginPermissions_: {type: Array},

      isActorLoginPermissionsEnabled_: {type: Boolean},
      isFedCmEmbedderInitiatedLoginEnabled_: {type: Boolean},

      // <if expr="is_win or is_macosx or is_chromeos">
      isBiometricAuthenticationForFillingToggleVisible_: {type: Boolean},
      // </if>

      isAutomatedPasswordChangeVisible_: {type: Boolean},
      hasPasswordsToExport_: {type: Boolean},
      hasPasskeys_: {type: Boolean},
      credentialsEnableServicePref_: {type: Object},

      /** The visibility state of the trusted vault banner. */
      trustedVaultBannerState_: {type: Number},

      movePasswordsLabel_: {type: String},
      canAddShortcut_: {type: Boolean},
      isPasswordManagerPinAvailable_: {type: Boolean},
      isConnectedToCloudAuthenticator_: {type: Boolean},
      isDisconnectCloudAuthenticatorInProgress_: {type: Boolean},
      toastMessage_: {type: String},
      showDisconnectCloudAuthenticatorDialog_: {type: Boolean},
      removeActorLoginPermissionSite_: {type: Object},
      localPasswordCount_: {type: Number},
    };
  }

  protected accessor blockedSites_: BlockedSite[] = [];
  protected accessor actorLoginPermissions_: ActorLoginPermission[] = [];
  private accessor isActorLoginPermissionsEnabled_: boolean =
      loadTimeData.getBoolean('enableActorLoginPermissions');
  private accessor isFedCmEmbedderInitiatedLoginEnabled_: boolean =
      loadTimeData.getBoolean('fedCmEmbedderInitiatedLoginEnabled');
  // <if expr="is_win or is_macosx or is_chromeos">
  protected accessor isBiometricAuthenticationForFillingToggleVisible_:
      boolean = loadTimeData.getBoolean(
          'biometricAuthenticationForFillingToggleVisible');
  // </if>
  protected accessor hasPasskeys_: boolean = false;
  protected accessor credentialsEnableServicePref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor hasPasswordsToExport_: boolean = false;
  protected accessor isAutomatedPasswordChangeVisible_: boolean =
      loadTimeData.getBoolean('passwordChangeAvailable');
  protected accessor canAddShortcut_: boolean =
      loadTimeData.getBoolean('canAddShortcut');
  private accessor trustedVaultBannerState_: TrustedVaultBannerState =
      TrustedVaultBannerState.NOT_SHOWN;
  protected accessor movePasswordsLabel_: string = '';
  protected accessor isPasswordManagerPinAvailable_: boolean = false;
  protected accessor isConnectedToCloudAuthenticator_: boolean = false;
  protected accessor isDisconnectCloudAuthenticatorInProgress_: boolean = false;
  protected accessor toastMessage_: string = '';
  protected accessor showDisconnectCloudAuthenticatorDialog_: boolean = false;
  protected accessor removeActorLoginPermissionSite_: ActorLoginPermission|
      undefined;
  // This variable depend on the sync service API, which the Batch Upload Dialog
  // uses.
  private accessor localPasswordCount_: number = 0;

  private setBlockedSitesListListener_: BlockedSitesListChangedListener|null =
      null;
  private setCredentialsChangedListener_: CredentialsChangedListener|null =
      null;

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref(
        'credentials_enable_service', 'credentialsEnableServicePref_');

    const updateLocalPasswordCount = (localPasswordCount: number) => {
      this.updateLocalPasswordCount_(localPasswordCount);
    };
    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();
    syncBrowserProxy.getLocalPasswordCount().then(updateLocalPasswordCount);

    this.setBlockedSitesListListener_ = blockedSites => {
      this.blockedSites_ = blockedSites;
    };
    PasswordManagerImpl.getInstance().getBlockedSitesList().then(
        blockedSites => this.blockedSites_ = blockedSites);
    PasswordManagerImpl.getInstance().addBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);

    this.addWebUiListener(
        'sync-service-local-password-count', updateLocalPasswordCount);

    this.setCredentialsChangedListener_ =
        (passwords: chrome.passwordsPrivate.PasswordUiEntry[]) => {
          this.hasPasswordsToExport_ = passwords.length > 0;
          // Update the local password count based on the SyncService API
          // whenever the password list was modified.
          syncBrowserProxy.getLocalPasswordCount().then(
              (localPasswordCount: number) => {
                this.updateLocalPasswordCount_(localPasswordCount);
              });
          if (this.isActorLoginPermissionsEnabled_) {
            PasswordManagerImpl.getInstance().getActorLoginPermissions().then(
                actorLoginPermissions => this.actorLoginPermissions_ =
                    actorLoginPermissions);
          }
        };
    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setCredentialsChangedListener_);

    const trustedVaultStateChanged = (state: TrustedVaultBannerState) => {
      // Set the state provided by the backend.
      this.trustedVaultBannerState_ = state;

      // If we are currently in the error state, override the visual state to
      // OPTED_IN.
      if (this.actionableError ===
          PasswordManagerActionableError.kTrustedVaultKeyNeeded) {
        this.trustedVaultBannerState_ = TrustedVaultBannerState.OPTED_IN;
      }
    };
    syncBrowserProxy.getTrustedVaultBannerState().then(
        trustedVaultStateChanged);
    this.addWebUiListener(
        'trusted-vault-banner-state-changed', trustedVaultStateChanged);

    // <if expr="is_win or is_macosx">
    PasskeysBrowserProxyImpl.getInstance().hasPasskeys().then(hasPasskeys => {
      this.hasPasskeys_ = hasPasskeys;
    });
    // </if>
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('isSyncingPasswords') ||
        changedProperties.has('isAccountStoreUser')) {
      // TODO(crbug.com/331611435): add listener for enclave availability and
      // trigger `updateIsPasswordManagerPinAvailable_`.
      this.updateIsPasswordManagerPinAvailable_();
      // Checks if the Chrome client is connected to / registered with the
      // Cloud Authenticator. If the client is connected, then a button to
      // disconnect the client is displayed.
      this.updateIsCloudAuthenticatorConnected_();
    }

    if (changedProperties.has('actionableError')) {
      this.updateTrustedVaultBannerState_(this.actionableError);
    }
  }

  override firstUpdated() {
    chrome.metricsPrivate.recordBoolean(
        'PasswordManager.OpenedAsShortcut',
        window.matchMedia('(display-mode: standalone)').matches);

    const addShortcutBanner =
        this.shadowRoot.querySelector<HTMLElement>('#addShortcutBanner');
    if (addShortcutBanner) {
      this.registerHelpBubble(
          PASSWORD_MANAGER_ADD_SHORTCUT_ELEMENT_ID, addShortcutBanner);
    }
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    if (newRoute.page === Page.SETTINGS &&
        oldRoute?.page === Page.PASSWORD_CHANGE &&
        this.isAutomatedPasswordChangeVisible_) {
      setTimeout(() => {
        const automatedPasswordChangeRow =
            this.shadowRoot.querySelector<HTMLElement>(
                '#automatedPasswordChange');
        if (automatedPasswordChangeRow) {
          automatedPasswordChangeRow.focus();
        }
      }, 0);
    }

    const triggerImportParam =
        newRoute.queryParameters.get(UrlParam.START_IMPORT) || '';
    if (triggerImportParam === 'true') {
      const importer = this.shadowRoot.querySelector('passwords-importer');
      assert(importer);
      importer.launchImport();
      const params = new URLSearchParams();
      Router.getInstance().updateRouterParams(params);
    }
  }

  protected onAddShortcutClick_() {
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
  protected onRemoveBlockedSiteClick_(e: Event) {
    const id = Number((e.currentTarget as HTMLElement).dataset['id']);
    PasswordManagerImpl.getInstance().removeBlockedSite(id);
  }

  protected onRemoveActorLoginPermissionClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    this.removeActorLoginPermissionSite_ = this.actorLoginPermissions_[index];
  }

  protected onRemoveActorLoginPermissionDialogClose_() {
    this.removeActorLoginPermissionSite_ = undefined;
  }

  protected onDialogRemoveActorLoginPermissionClick_() {
    assert(this.removeActorLoginPermissionSite_);
    PasswordManagerImpl.getInstance().revokeActorLoginPermission(
        this.removeActorLoginPermissionSite_);
    this.removeActorLoginPermissionSite_ = undefined;
  }

  // <if expr="is_win or is_macosx or is_chromeos">
  protected onValidateAndChangePref_(e: Event) {
    const biometricAuthenticationForFillingToggle =
        e.target as PrefToggleButtonElement;
    assert(biometricAuthenticationForFillingToggle);
    PasswordManagerImpl.getInstance().switchBiometricAuthBeforeFillingState();
  }
  // </if>

  protected onTrustedVaultBannerClick_() {
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

  protected getTrustedVaultBannerTitle_(): string {
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

  protected getTrustedVaultBannerDescription_(): string {
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

  protected shouldHideTrustedVaultBanner_(): boolean {
    return this.trustedVaultBannerState_ === TrustedVaultBannerState.NOT_SHOWN;
  }

  protected getAriaLabelForBlockedSite_(
      blockedSite: chrome.passwordsPrivate.ExceptionEntry): string {
    return this.i18n('removeBlockedAriaDescription', blockedSite.urls.shown);
  }

  // <if expr="is_win or is_macosx">
  protected onManagePasskeysClick_() {
    PasskeysBrowserProxyImpl.getInstance().managePasskeys();
  }
  // </if>

  protected isPasswordSavingBlockedByPolicy_(): boolean {
    if (!this.credentialsEnableServicePref_) {
      return false;
    }
    return isSavingNewPasswordsBlockedByMandatoryPolicy(
        this.credentialsEnableServicePref_);
  }

  protected shouldShowActorLoginPermissions_(): boolean {
    return this.actorLoginPermissions_.length > 0 &&
        this.isActorLoginPermissionsEnabled_ &&
        !this.isFedCmEmbedderInitiatedLoginEnabled_;
  }

  protected onMovePasswordsClick_(e: Event) {
    e.preventDefault();
    SyncBrowserProxyImpl.getInstance().openBatchUpload(
        BatchUploadPasswordsEntryPoint.PASSWORD_MANAGER);
  }

  protected shouldShowMovePasswordsEntry_(): boolean {
    // Only show the move password entry if there are passwords returned from
    // the sync service API. This is needed to be consistent with the
    // availability of data in the dialog which uses the same API.
    return this.localPasswordCount_ > 0;
  }

  protected getAriaDescriptionMovePasswordsButton_(): string {
    return [
      this.movePasswordsLabel_,
      this.i18n('movePasswordsInSettingsSubLabel'),
    ].join('. ');
  }

  // This updates the local password count coming from the Sync Service API.
  private async updateLocalPasswordCount_(localPasswordCount: number) {
    this.localPasswordCount_ = localPasswordCount;

    this.movePasswordsLabel_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'deviceOnlyPasswordsIconTooltip', this.localPasswordCount_);
  }

  private updateIsPasswordManagerPinAvailable_() {
    PasswordManagerImpl.getInstance().isPasswordManagerPinAvailable().then(
        available => this.isPasswordManagerPinAvailable_ =
            available && (this.isSyncingPasswords || this.isAccountStoreUser));
  }

  protected onChangePasswordManagerPinRowClick_() {
    PasswordManagerImpl.getInstance().changePasswordManagerPin().then(
        this.showToastForPasswordChange_.bind(this));
  }

  private updateIsCloudAuthenticatorConnected_() {
    PasswordManagerImpl.getInstance().isConnectedToCloudAuthenticator().then(
        connected => this.isConnectedToCloudAuthenticator_ =
            connected && (this.isSyncingPasswords || this.isAccountStoreUser));
  }

  private updateTrustedVaultBannerState_(
      error: PasswordManagerActionableError|null) {
    if (error === PasswordManagerActionableError.kTrustedVaultKeyNeeded) {
      this.trustedVaultBannerState_ = TrustedVaultBannerState.OPTED_IN;
    } else if (
        this.trustedVaultBannerState_ === TrustedVaultBannerState.OPTED_IN) {
      // Re-fetch the state from the browser if we no longer require unlock.
      SyncBrowserProxyImpl.getInstance().getTrustedVaultBannerState().then(
          state => this.trustedVaultBannerState_ = state);
    }
  }

  protected onDisconnectCloudAuthenticatorClick_() {
    this.showDisconnectCloudAuthenticatorDialog_ = true;
  }

  protected onDisconnectCloudAuthenticatorDialogClose_(): void {
    this.showDisconnectCloudAuthenticatorDialog_ = false;
  }

  protected onDisconnectCloudAuthenticator_(e: CustomEvent<{success: boolean}>):
      void {
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

  protected getAriaDescriptionForCloudAuthenticatorButton_(): string {
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

  protected onAutomatedPasswordChangeClick_() {
    Router.getInstance().navigateTo(Page.PASSWORD_CHANGE);
  }

  protected getAriaLabelForAutomatedPasswordChange_(): string {
    return [
      this.i18n('automatedPasswordChangeTitle'),
      this.i18n('automatedPasswordChangeDescription'),
    ].join('. ');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);
