// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../controls/extension_controlled_indicator.js';
// <if expr="is_chromeos">
import '../controls/password_prompt_dialog.js';
// </if>
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
import './password_list_item.js';
import './passwords_list_handler.js';
import './passwords_export_dialog.js';
import './passwords_import_dialog.js';
import './passwords_shared.css.js';
import './avatar_icon.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {getDeepActiveElement, isUndoKeyboardEvent} from 'chrome://resources/js/util_ts.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {FocusConfig} from '../focus_config.js';
import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {SyncBrowserProxyImpl, TrustedVaultBannerState} from '../people_page/sync_browser_proxy.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, Router} from '../router.js';

import {MergePasswordsStoreCopiesMixin} from './merge_passwords_store_copies_mixin.js';
// <if expr="is_win or is_macosx">
import {PasskeysBrowserProxy, PasskeysBrowserProxyImpl} from './passkeys_browser_proxy.js';
// </if>
import {PasswordCheckMixin} from './password_check_mixin.js';
import {AddCredentialFromSettingsUserInteractions, PasswordEditDialogElement} from './password_edit_dialog.js';
import {PasswordCheckReferrer, PasswordExceptionListChangedListener, PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {PASSWORD_MANAGER_AUTH_TIMEOUT_PARAM} from './password_view.js';
import {PasswordsImportDesktopInteractions, recordPasswordsImportInteraction} from './passwords_import_dialog.js';
import {PasswordsListHandlerElement} from './passwords_list_handler.js';
import {getTemplate} from './passwords_section.html.js';
import {UserUtilMixin} from './user_util_mixin.js';

/**
 * Checks if an HTML element is an editable. An editable is either a text
 * input or a text area.
 */
function isEditable(element: Element): boolean {
  const nodeName = element.nodeName.toLowerCase();
  return element.nodeType === Node.ELEMENT_NODE &&
      (nodeName === 'textarea' ||
       (nodeName === 'input' &&
        /^(?:text|search|email|number|tel|url|password)$/i.test(
            (element as HTMLInputElement).type)));
}

export interface PasswordsSectionElement {
  $: {
    accountEmail: HTMLElement,
    accountStorageButtonsContainer: HTMLElement,
    accountStorageOptInBody: HTMLElement,
    accountStorageOptOutBody: HTMLElement,
    addPasswordDialog: PasswordEditDialogElement,
    authTimeoutDialog: CrDialogElement,
    checkPasswordLeakCount: HTMLElement,
    checkPasswordLeakDescription: HTMLElement,
    checkPasswordWarningIcon: HTMLElement,
    checkPasswordsBannerContainer: HTMLElement,
    checkPasswordsButtonRow: HTMLElement,
    checkPasswordsLinkRow: HTMLElement,
    devicePasswordsLink: HTMLElement,
    exportImportMenu: CrActionMenuElement,
    manageLink: HTMLElement,
    menuEditPassword: HTMLElement,
    menuExportPassword: HTMLButtonElement,
    noExceptionsLabel: HTMLElement,
    noPasswordsLabel: HTMLElement,
    optInToAccountStorageButton: HTMLElement,
    optOutOfAccountStorageButton: HTMLElement,
    passwordExceptionsList: HTMLElement,
    passwordList: DomRepeat,
    passwordsListHandler: PasswordsListHandlerElement,
    savedPasswordsHeaders: HTMLElement,
    trustedVaultBanner: CrLinkRowElement,
  };
}

const PasswordsSectionElementBase =
    UserUtilMixin(MergePasswordsStoreCopiesMixin(PrefsMixin(
        GlobalScrollTargetMixin(RouteObserverMixin(WebUiListenerMixin(
            I18nMixin(PasswordCheckMixin(PolymerElement))))))));

export class PasswordsSectionElement extends PasswordsSectionElementBase {
  static get is() {
    return 'passwords-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      subpageRoute: {
        type: Object,
        value: routes.PASSWORDS,
      },

      /** Filter on the saved passwords and exceptions. */
      filter: {
        type: String,
        value: '',
        observer: 'announceSearchResults_',
      },

      shownPasswordsCount_: {
        type: Number,
        value: 0,
      },

      shownExceptionsCount_: {
        type: Number,
        value: 0,
      },

      hasNeverCheckedPasswords_: {
        type: Boolean,
        computed: 'computeHasNeverCheckedPasswords_(status)',
      },

      hasSavedPasswords_: {
        type: Boolean,
        computed:
            'computeHasSavedPasswords_(savedPasswords, savedPasswords.splices)',
      },

      /**
       * Used to decide the text on the button leading to 'device passwords'
       * page.
       */
      numberOfDevicePasswords_: {
        type: Number,
        computed: 'computeNumberOfDevicePasswords_(savedPasswords, ' +
            'savedPasswords.splices)',
      },

      hasPasswordExceptions_: {
        type: Boolean,
        computed: 'computeHasPasswordExceptions_(passwordExceptions)',
      },

      shouldShowBanner_: {
        type: Boolean,
        value: true,
        computed: 'computeShouldShowBanner_(hasLeakedCredentials_,' +
            'signedIn, hasNeverCheckedPasswords_, hasSavedPasswords_)',
      },

      /**
       * Whether the entry point leading to the device passwords page should be
       * shown for a user who is already eligible for account storage.
       */
      shouldShowDevicePasswordsLink_: {
        type: Boolean,
        value: false,
        computed: 'computeShouldShowDevicePasswordsLink_(' +
            'isOptedInForAccountStorage, numberOfDevicePasswords_)',
      },

      /** The visibility state of the trusted vault banner. */
      trustedVaultBannerState_: {
        type: Object,
        value: TrustedVaultBannerState.NOT_SHOWN,
      },

      hasLeakedCredentials_: {
        type: Boolean,
        computed: 'computeHasLeakedCredentials_(leakedPasswords)',
      },

      /** Whether the user has passkeys (i.e. WebAuthn credentials). */
      hasPasskeys_: {
        type: Boolean,
        value: false,
      },

      isPasswordViewPageEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordViewPage');
        },
        reflectToAttribute: true,
      },

      // <if expr="is_win or is_macosx">
      isBiometricAuthenticationForFillingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('biometricAuthenticationForFilling');
        },
      },
      // </if>

      showImportPasswords_: {
        type: Boolean,
        computed:
            'computeShowImportPasswords_(passwordManagerDisabledByPolicy_)',
      },

      /** An array of blocked sites to display. */
      passwordExceptions: {
        type: Array,
        value: () => [],
      },

      showPasswordsExportDialog_: Boolean,
      showPasswordsImportDialog_: Boolean,

      showAddPasswordDialog_: Boolean,

      passwordManagerDisabledByPolicy_: {
        type: Boolean,
        computed: 'computePasswordManagerDisabledByPolicy_(' +
            'prefs.credentials_enable_service.enforcement, ' +
            'prefs.credentials_enable_service.value)',
      },
    };
  }

  focusConfig: FocusConfig;
  subpageRoute: Route;
  filter: string;
  passwordExceptions: chrome.passwordsPrivate.ExceptionEntry[];

  private shownPasswordsCount_: number;
  private shownExceptionsCount_: number;
  private hasNeverCheckedPasswords_: boolean;
  private hasSavedPasswords_: boolean;
  private numberOfDevicePasswords_: number;
  private hasPasswordExceptions_: boolean;
  private shouldShowBanner_: boolean;
  private isPasswordViewPageEnabled_: boolean;
  private shouldShowDevicePasswordsLink_: boolean;
  private trustedVaultBannerState_: TrustedVaultBannerState;
  private hasLeakedCredentials_: boolean;
  private hasPasskeys_: boolean;
  private showImportPasswords_: boolean;

  private showPasswordsExportDialog_: boolean;
  private showPasswordsImportDialog_: boolean;
  private showAddPasswordDialog_: boolean;
  private passwordManagerDisabledByPolicy_: boolean;

  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();
  // <if expr="is_win or is_macosx">
  private passkeysBrowserProxy_: PasskeysBrowserProxy =
      PasskeysBrowserProxyImpl.getInstance();
  // </if>
  private setPasswordExceptionsListener_: PasswordExceptionListChangedListener|
      null = null;

  override ready() {
    super.ready();

    document.addEventListener('keydown', (e: KeyboardEvent) => {
      if (isUndoKeyboardEvent(e)) {
        this.onUndoKeyBinding_(e);
      }
    });

    // <if expr="is_win or is_macosx">
    this.passkeysBrowserProxy_.hasPasskeys().then(hasPasskeys => {
      this.hasPasskeys_ = hasPasskeys;
    });
    // </if>

    if (this.showImportPasswords_) {
      const importLink = this.$.noPasswordsLabel.querySelector('a');
      // Add an event listener to the import link, points to the import flow.
      assert(importLink);
      importLink!.addEventListener('click', (event: Event) => {
        // The action is triggered from a dummy anchor element poining to "#".
        // For that case preventing the default behaviour is required here.
        event.preventDefault();
        recordPasswordsImportInteraction(
            PasswordsImportDesktopInteractions.DIALOG_OPENED_FROM_EMPTY_STATE);
        this.showPasswordsImportDialog_ = true;
      });
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    this.setPasswordExceptionsListener_ = exceptionList => {
      this.passwordExceptions = exceptionList;
    };

    // Request initial data.
    this.passwordManager_.getExceptionList().then(
        this.setPasswordExceptionsListener_);

    // Listen for changes.
    this.passwordManager_.addExceptionListChangedListener(
        this.setPasswordExceptionsListener_);

    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();

    syncBrowserProxy.sendTrustedVaultBannerStateChanged();
    this.addWebUiListener(
        'trusted-vault-banner-state-changed',
        (state: TrustedVaultBannerState) => {
          this.trustedVaultBannerState_ = state;
        });

    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.OPENED_PASSWORD_MANAGER);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.setPasswordExceptionsListener_);
    PasswordManagerImpl.getInstance().removeExceptionListChangedListener(
        this.setPasswordExceptionsListener_);
    this.setPasswordExceptionsListener_ = null;
  }

  override currentRouteChanged(route: Route): void {
    super.currentRouteChanged(route);

    if (route !== routes.PASSWORDS) {
      return;
    }

    // Show the auth timeout dialog if the URL has the URL param.
    const params = Router.getInstance().getQueryParameters();
    if (!params.get(PASSWORD_MANAGER_AUTH_TIMEOUT_PARAM)) {
      return;
    }
    this.$.authTimeoutDialog.showModal();
    params.delete(PASSWORD_MANAGER_AUTH_TIMEOUT_PARAM);
    Router.getInstance().updateRouteParams(params);
  }

  private onCloseAuthTimeoutDialogButton_() {
    this.$.authTimeoutDialog.close();
  }

  private computeShowImportPasswords_(): boolean {
    return !this.passwordManagerDisabledByPolicy_ &&
        loadTimeData.valueExists('showImportPasswords') &&
        loadTimeData.getBoolean('showImportPasswords');
  }

  private computePasswordManagerDisabledByPolicy_(): boolean {
    // Don't show add button if password manager is disabled by policy.
    return this.prefs.credentials_enable_service.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED &&
        !this.prefs.credentials_enable_service.value;
  }

  private computeHasSavedPasswords_(): boolean {
    return this.savedPasswords.length > 0;
  }

  private computeNumberOfDevicePasswords_(): number {
    return this.savedPasswords
        .filter(
            p =>
                p.storedIn !== chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT)
        .length;
  }

  private computeHasPasswordExceptions_(): boolean {
    return this.passwordExceptions.length > 0;
  }

  private computeShouldShowBanner_(): boolean {
    return !!this.signedIn && this.hasSavedPasswords_ &&
        this.hasNeverCheckedPasswords_ && !this.hasLeakedCredentials_;
  }

  private computeShouldShowDevicePasswordsLink_(): boolean {
    return this.isOptedInForAccountStorage &&
        (this.numberOfDevicePasswords_ > 0);
  }

  private computeHasLeakedCredentials_(): boolean {
    return this.leakedPasswords.length > 0;
  }

  private computeHasNeverCheckedPasswords_(): boolean {
    return !this.status.elapsedTimeSinceLastCheck;
  }

  private switchBiometricAuthBeforeFillingState_(e: Event) {
    const biometricAuthenticationForFillingToggle =
        e!.target as SettingsToggleButtonElement;
    assert(biometricAuthenticationForFillingToggle);
    // User action is removed since toggle value shouldn't change until user
    // authenticates successfully, after that toggle value will be put in the
    // correct state.
    biometricAuthenticationForFillingToggle.checked =
        !biometricAuthenticationForFillingToggle.checked;
    this.passwordManager_.switchBiometricAuthBeforeFillingState();
  }

  /**
   * Shows the check passwords sub page.
   */
  private onCheckPasswordsClick_() {
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    this.passwordManager_.recordPasswordCheckReferrer(
        PasswordCheckReferrer.PASSWORD_SETTINGS);
  }

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

  /**
   * Shows the 'device passwords' page.
   */
  private onDevicePasswordsLinkClicked_() {
    Router.getInstance().navigateTo(routes.DEVICE_PASSWORDS);
  }

  private onManagePasskeysClick_() {
    Router.getInstance().navigateTo(routes.PASSKEYS);
  }

  getPasswordManagerForTest(): PasswordManagerProxy {
    return this.passwordManager_;
  }

  private passwordFilter_():
      ((entry: chrome.passwordsPrivate.PasswordUiEntry) => boolean) {
    return password => [password.urls.shown, password.username].some(
               term => term.toLowerCase().includes(
                   this.filter.trim().toLowerCase()));
  }

  private passwordExceptionFilter_():
      ((entry: chrome.passwordsPrivate.ExceptionEntry) => boolean) {
    return exception => exception.urls.shown.toLowerCase().includes(
               this.filter.trim().toLowerCase());
  }

  /**
   * Handle the shortcut to undo a removal of passwords/exceptions. This must
   * be handled here and not at the PasswordsListHandler level because that
   * component does not know about exception deletions.
   */
  private onUndoKeyBinding_(event: Event) {
    const activeElement = getDeepActiveElement();
    // If the focused element is editable (e.g. search box) the undo event
    // should be handled there and not here.
    if (!activeElement || !isEditable(activeElement)) {
      this.passwordManager_.undoRemoveSavedPasswordOrException();
      this.$.passwordsListHandler.onSavedPasswordOrExceptionRemoved();
      // Preventing the default is necessary to not conflict with a possible
      // search action.
      event.preventDefault();
    }
  }

  /**
   * Fires an event that should delete the password exception.
   */
  private onRemoveExceptionButtonTap_(
      e: DomRepeatEvent<chrome.passwordsPrivate.ExceptionEntry>) {
    const exception = e.model.item;
    this.passwordManager_.removeException(exception.id);
  }

  /**
   * Opens the export/import action menu.
   */
  private onImportExportMenuTap_() {
    const target = this.shadowRoot!.querySelector('#exportImportMenuButton') as
        HTMLElement;
    this.$.exportImportMenu.showAt(target);
  }

  /**
   * Opens the passwords import dialog.
   */
  private onImportTap_() {
    recordPasswordsImportInteraction(
        PasswordsImportDesktopInteractions.DIALOG_OPENED_FROM_THREE_DOT_MENU);
    this.showPasswordsImportDialog_ = true;
    this.$.exportImportMenu.close();
  }

  private onPasswordsImportDialogClosed_() {
    this.showPasswordsImportDialog_ = false;
  }

  /**
   * Opens the export passwords dialog.
   */
  private onExportTap_() {
    this.showPasswordsExportDialog_ = true;
    this.$.exportImportMenu.close();
  }

  private onPasswordsExportDialogClosed_() {
    this.showPasswordsExportDialog_ = false;
  }

  private onAddPasswordTap_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AddCredentialFromSettings.UserAction2',
        AddCredentialFromSettingsUserInteractions.ADD_DIALOG_OPENED,
        AddCredentialFromSettingsUserInteractions.COUNT);
    this.showAddPasswordDialog_ = true;
  }

  private onAddPasswordDialogClosed_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AddCredentialFromSettings.UserAction2',
        AddCredentialFromSettingsUserInteractions.ADD_DIALOG_CLOSED,
        AddCredentialFromSettingsUserInteractions.COUNT);
    this.showAddPasswordDialog_ = false;
  }

  private showImportOrExportPasswords_(): boolean {
    return this.hasSavedPasswords_ || this.showImportPasswords_;
  }

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-autofill-page>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    this.focusConfig.set(routes.CHECK_PASSWORDS.path, () => {
      const toFocus = this.shadowRoot!.querySelector<HTMLElement>('#icon');
      assert(toFocus);
      focusWithoutInk(toFocus);
    });
    // <if expr="is_win or is_macosx">
    this.focusConfig.set(routes.PASSKEYS.path, () => {
      const toFocus =
          this.shadowRoot!.querySelector<HTMLElement>('#managePasskeysIcon');
      assert(toFocus);
      focusWithoutInk(toFocus);
    });
    // </if>
  }

  private announceSearchResults_() {
    if (!this.filter.trim()) {
      return;
    }
    setTimeout(() => {  // Async to allow list to update.
      const total = this.shownPasswordsCount_ + this.shownExceptionsCount_;
      let text;
      switch (total) {
        case 0:
          text = this.i18n('noSearchResults');
          break;
        case 1:
          text = this.i18n('searchResultsSingular', this.filter);
          break;
        default:
          text =
              this.i18n('searchResultsPlural', total.toString(), this.filter);
      }

      getAnnouncerInstance().announce(text);
    }, 0);
  }

  private getTrustedVaultBannerSubLabel_(): string {
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
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-section': PasswordsSectionElement;
  }
}

customElements.define(PasswordsSectionElement.is, PasswordsSectionElement);
