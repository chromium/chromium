// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../controls/extension_controlled_indicator.js';
// <if expr="chromeos_ash or chromeos_lacros">
import '../controls/password_prompt_dialog.js';
// </if>
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';
import '../site_favicon.js';
import './password_list_item.js';
import './passwords_list_handler.js';
import './passwords_export_dialog.js';
import './passwords_shared_css.js';
import './avatar_icon.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';
import {StoredAccount, SyncBrowserProxyImpl, SyncPrefs, SyncStatus, TrustedVaultBannerState} from '../people_page/sync_browser_proxy.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {routes} from '../route.js';
import {Route, Router} from '../router.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {MergeExceptionsStoreCopiesMixin} from './merge_exceptions_store_copies_mixin.js';
import {MergePasswordsStoreCopiesMixin} from './merge_passwords_store_copies_mixin.js';
import {MultiStoreExceptionEntry} from './multi_store_exception_entry.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordCheckMixin} from './password_check_mixin.js';
import {AddCredentialFromSettingsUserInteractions} from './password_edit_dialog.js';
import {PasswordCheckReferrer, PasswordExceptionListChangedListener, PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {PasswordsListHandlerElement} from './passwords_list_handler.js';
import {getTemplate} from './passwords_section.html.js';

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

type FocusConfig = Map<string, string|(() => void)>;

export interface PasswordsSectionElement {
  $: {
    exportImportMenu: CrActionMenuElement,
    passwordsListHandler: PasswordsListHandlerElement,
  };
}

const PasswordsSectionElementBase = MergePasswordsStoreCopiesMixin(
    PrefsMixin(GlobalScrollTargetMixin(MergeExceptionsStoreCopiesMixin(
        WebUIListenerMixin(I18nMixin(PasswordCheckMixin(PolymerElement)))))));

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

      storedAccounts_: Array,

      signedIn_: {
        type: Boolean,
        value: true,
        computed: 'computeSignedIn_(syncStatus_, storedAccounts_)',
      },

      eligibleForAccountStorage_: {
        type: Boolean,
        value: false,
        computed: 'computeEligibleForAccountStorage_(' +
            'syncStatus_, signedIn_, syncPrefs_)',
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
            'signedIn_, hasNeverCheckedPasswords_, hasSavedPasswords_)',
      },

      /**
       * If true, the edit dialog and removal notification show
       * information about which location(s) a password is stored.
       */
      isAccountStoreUser_: {
        type: Boolean,
        value: false,
        computed: 'computeIsAccountStoreUser_(' +
            'eligibleForAccountStorage_, isOptedInForAccountStorage_)',
      },

      /**
       * Whether the entry point leading to the device passwords page should be
       * shown for a user who is already eligible for account storage.
       */
      shouldShowDevicePasswordsLink_: {
        type: Boolean,
        value: false,
        computed: 'computeShouldShowDevicePasswordsLink_(' +
            'isOptedInForAccountStorage_, numberOfDevicePasswords_)',
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

      hidePasswordsLink_: {
        type: Boolean,
        computed: 'computeHidePasswordsLink_(syncPrefs_, syncStatus_, ' +
            'eligibleForAccountStorage_)',
      },

      showImportPasswords_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showImportPasswords') &&
              loadTimeData.getBoolean('showImportPasswords');
        }
      },

      profileEmail_: {
        type: String,
        value: '',
        computed: 'getFirstStoredAccountEmail_(storedAccounts_)',
      },

      /**
       * The currently selected profile icon as CSS image set.
       */
      profileIcon_: String,

      isOptedInForAccountStorage_: Boolean,
      syncPrefs_: Object,
      syncStatus_: Object,

      // <if expr="chromeos_ash or chromeos_lacros">
      showPasswordPromptDialog_: Boolean,
      tokenRequestManager_: Object,
      // </if>

      showPasswordsExportDialog_: Boolean,

      showAddPasswordDialog_: Boolean,

      showAddPasswordButton_: {
        type: Boolean,
        computed: 'computeShowAddPasswordButton_(' +
            'prefs.credentials_enable_service.enforcement, ' +
            'prefs.credentials_enable_service.value)',
      },
    };
  }

  focusConfig: FocusConfig;
  subpageRoute: Route;
  filter: string;

  private shownPasswordsCount_: number;
  private shownExceptionsCount_: number;

  private storedAccounts_: Array<StoredAccount>;
  private signedIn_: boolean;
  private eligibleForAccountStorage_: boolean;
  private hasNeverCheckedPasswords_: boolean;
  private hasSavedPasswords_: boolean;
  private numberOfDevicePasswords_: number;
  private hasPasswordExceptions_: boolean;
  private shouldShowBanner_: boolean;
  private isAccountStoreUser_: boolean;
  private shouldShowDevicePasswordsLink_: boolean;
  private trustedVaultBannerState_: TrustedVaultBannerState;
  private hasLeakedCredentials_: boolean;
  private hidePasswordsLink_: boolean;
  private showImportPasswords_: boolean;
  private profileEmail_: string;
  private profileIcon_: string;
  private isOptedInForAccountStorage_: boolean;
  private syncPrefs_: SyncPrefs;
  private syncStatus_: SyncStatus;

  // <if expr="chromeos_ash or chromeos_lacros">
  private showPasswordPromptDialog_: boolean;
  private tokenRequestManager_: BlockingRequestManager;
  // </if>

  private showPasswordsExportDialog_: boolean;
  private showAddPasswordDialog_: boolean;
  private showAddPasswordButton_: boolean;

  private activeDialogAnchorStack_: Array<HTMLElement>;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();
  private setIsOptedInForAccountStorageListener_:
      ((isOptedIn: boolean) => void)|null = null;
  private setPasswordExceptionsListener_: PasswordExceptionListChangedListener|
      null = null;

  constructor() {
    super();

    /**
     * A stack of the elements that triggered dialog to open and should
     * therefore receive focus when that dialog is closed. The bottom of the
     * stack is the element that triggered the earliest open dialog and top of
     * the stack is the element that triggered the most recent (i.e. active)
     * dialog. If no dialog is open, the stack is empty.
     */
    this.activeDialogAnchorStack_ = [];
  }

  override ready() {
    super.ready();

    document.addEventListener('keydown', e => {
      // <if expr="is_macosx">
      if (e.metaKey && e.key === 'z') {
        this.onUndoKeyBinding_(e);
      }
      // </if>
      // <if expr="not is_macosx">
      if (e.ctrlKey && e.key === 'z') {
        this.onUndoKeyBinding_(e);
      }
      // </if>
    });
  }

  override connectedCallback() {
    super.connectedCallback();

    // Create listener functions.
    const setIsOptedInForAccountStorageListener = (optedIn: boolean) => {
      this.isOptedInForAccountStorage_ = optedIn;
    };

    this.setIsOptedInForAccountStorageListener_ =
        setIsOptedInForAccountStorageListener;

    // <if expr="chromeos_ash or chromeos_lacros">
    // If the user's account supports the password check, an auth token will be
    // required in order for them to view or export passwords. Otherwise there
    // is no additional security so |tokenRequestManager_| will immediately
    // resolve requests.
    if (loadTimeData.getBoolean('userCannotManuallyEnterPassword')) {
      this.tokenRequestManager_ = new BlockingRequestManager();
    } else {
      this.tokenRequestManager_ =
          new BlockingRequestManager(() => this.openPasswordPromptDialog_());
    }
    // </if>

    // Request initial data.
    this.passwordManager_.isOptedInForAccountStorage().then(
        setIsOptedInForAccountStorageListener);

    // Listen for changes.
    this.passwordManager_.addAccountStorageOptInStateListener(
        setIsOptedInForAccountStorageListener);

    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();

    const syncStatusChanged = (syncStatus: SyncStatus) => this.syncStatus_ =
        syncStatus;
    syncBrowserProxy.getSyncStatus().then(syncStatusChanged);
    this.addWebUIListener('sync-status-changed', syncStatusChanged);

    const syncPrefsChanged = (syncPrefs: SyncPrefs) => this.syncPrefs_ =
        syncPrefs;
    this.addWebUIListener('sync-prefs-changed', syncPrefsChanged);
    syncBrowserProxy.sendSyncPrefsChanged();

    // For non-ChromeOS, non-Lacros, also check whether accounts are available.
    // <if expr="not (chromeos_ash or chromeos_lacros)">
    const storedAccountsChanged = (accounts: Array<StoredAccount>) =>
        this.storedAccounts_ = accounts;
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>

    syncBrowserProxy.sendTrustedVaultBannerStateChanged();
    this.addWebUIListener(
        'trusted-vault-banner-state-changed',
        (state: TrustedVaultBannerState) => {
          this.trustedVaultBannerState_ = state;
        });

    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });

    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.OPENED_PASSWORD_MANAGER);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.passwordManager_.removeAccountStorageOptInStateListener(
        assert(this.setIsOptedInForAccountStorageListener_!));
    this.setIsOptedInForAccountStorageListener_ = null;
  }

  private computeShowAddPasswordButton_(): boolean {
    return loadTimeData.getBoolean('addPasswordsInSettingsEnabled') &&
        // Don't show add button if password manager is disabled by policy.
        !(this.prefs.credentials_enable_service.enforcement ===
              chrome.settingsPrivate.Enforcement.ENFORCED &&
          !this.prefs.credentials_enable_service.value);
  }

  private computeSignedIn_(): boolean {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn ?
        !this.syncStatus_.hasError :
        (!!this.storedAccounts_ && this.storedAccounts_.length > 0);
  }

  private computeEligibleForAccountStorage_(): boolean {
    // The user must have signed in but should have sync disabled
    // (|!this.syncStatus_.signedin|). They should not be using a custom
    // passphrase to encrypt their sync data, since there's no way for account
    // storage users to input their passphrase and decrypt the passwords.
    return (!!this.syncStatus_ && !this.syncStatus_.signedIn) &&
        this.signedIn_ && (!this.syncPrefs_ || !this.syncPrefs_.encryptAllData);
  }

  private computeHasSavedPasswords_(): boolean {
    return this.savedPasswords.length > 0;
  }

  private computeNumberOfDevicePasswords_(): number {
    return this.savedPasswords.filter(p => p.isPresentOnDevice()).length;
  }

  private computeHasPasswordExceptions_(): boolean {
    return this.passwordExceptions.length > 0;
  }

  private computeShouldShowBanner_(): boolean {
    return this.signedIn_ && this.hasSavedPasswords_ &&
        this.hasNeverCheckedPasswords_ && !this.hasLeakedCredentials_;
  }

  private computeIsAccountStoreUser_(): boolean {
    return this.eligibleForAccountStorage_ && this.isOptedInForAccountStorage_;
  }

  private computeShouldShowDevicePasswordsLink_(): boolean {
    return this.isOptedInForAccountStorage_ &&
        (this.numberOfDevicePasswords_ > 0);
  }

  /**
   * hide the link to the user's Google Account if:
   *  a) the link is embedded in the account storage message OR
   *  b) the user is signed out (or signed-in but has encrypted passwords)
   */
  private computeHidePasswordsLink_(): boolean {
    return this.eligibleForAccountStorage_ ||
        (!!this.syncStatus_ && !!this.syncStatus_.signedIn &&
         !!this.syncPrefs_ && !!this.syncPrefs_.encryptAllData);
  }

  private computeHasLeakedCredentials_(): boolean {
    return this.leakedPasswords.length > 0;
  }

  private computeHasNeverCheckedPasswords_(): boolean {
    return !this.status.elapsedTimeSinceLastCheck;
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
        OpenWindowProxyImpl.getInstance().openURL(
            loadTimeData.getString('trustedVaultLearnMoreUrl'));
        break;
      case TrustedVaultBannerState.OFFER_OPT_IN:
        OpenWindowProxyImpl.getInstance().openURL(
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

  // <if expr="chromeos_ash or chromeos_lacros">
  /**
   * When this event fired, it means that the password-prompt-dialog succeeded
   * in creating a fresh token in the quickUnlockPrivate API. Because new tokens
   * can only ever be created immediately following a GAIA password check, the
   * passwordsPrivate API can now safely grant requests for secure data (i.e.
   * saved passwords) for a limited time. This observer resolves the request,
   * triggering a callback that requires a fresh auth token to succeed and that
   * was provided to the BlockingRequestManager by another DOM element seeking
   * secure data.
   *
   * @param e - Contain newly created auth token
   *     chrome.quickUnlockPrivate.TokenInfo. Note that its precise value is not
   *     relevant here, only the facts that it's created.
   */
  private onTokenObtained_(e: CustomEvent<any>) {
    assert(e.detail);
    this.tokenRequestManager_.resolve();
  }

  private onPasswordPromptClosed_() {
    this.showPasswordPromptDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()!));
  }

  private openPasswordPromptDialog_() {
    this.activeDialogAnchorStack_.push(getDeepActiveElement() as HTMLElement);
    this.showPasswordPromptDialog_ = true;
  }
  // </if>

  private passwordFilter_(): ((entry: MultiStorePasswordUiEntry) => boolean) {
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
      e: DomRepeatEvent<MultiStoreExceptionEntry>) {
    const exception = e.model.item;
    const allExceptionIds: Array<number> = [];
    if (exception.isPresentInAccount()) {
      allExceptionIds.push(exception.accountId!);
    }
    if (exception.isPresentOnDevice()) {
      allExceptionIds.push(exception.deviceId!);
    }
    this.passwordManager_.removeExceptions(allExceptionIds);
  }

  /**
   * Opens the export/import action menu.
   */
  private onImportExportMenuTap_() {
    const target = this.shadowRoot!.querySelector('#exportImportMenuButton') as
        HTMLElement;
    this.$.exportImportMenu.showAt(target);
    this.activeDialogAnchorStack_.push(target);
  }

  /**
   * Fires an event that should trigger the password import process.
   */
  private onImportTap_() {
    this.passwordManager_.importPasswords();
    this.$.exportImportMenu.close();
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
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()!));
  }

  private onAddPasswordTap_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AddCredentialFromSettings.UserAction',
        AddCredentialFromSettingsUserInteractions.Add_Dialog_Opened,
        AddCredentialFromSettingsUserInteractions.COUNT);
    this.showAddPasswordDialog_ = true;
    this.activeDialogAnchorStack_.push(
        this.shadowRoot!.querySelector('#addPasswordButton')!);
  }

  private onAddPasswordDialogClosed_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AddCredentialFromSettings.UserAction',
        AddCredentialFromSettingsUserInteractions.Add_Dialog_Closed,
        AddCredentialFromSettingsUserInteractions.COUNT);
    this.showAddPasswordDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()!));
  }

  private onOptIn_() {
    this.passwordManager_.optInForAccountStorage(true);
  }

  private onOptOut_() {
    this.passwordManager_.optInForAccountStorage(false);
  }

  private showImportOrExportPasswords_(): boolean {
    return this.hasSavedPasswords_ || this.showImportPasswords_;
  }

  /**
   * Return the first available stored account. This is useful when trying to
   * figure out the account logged into the content area which seems to always
   * be first even if multiple accounts are available.
   * @return The email address of the first stored account or an empty string.
   */
  private getFirstStoredAccountEmail_(): string {
    return !!this.storedAccounts_ && this.storedAccounts_.length > 0 ?
        this.storedAccounts_[0].email :
        '';
  }

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-autofill-page>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    this.focusConfig.set(assert(routes.CHECK_PASSWORDS).path, () => {
      focusWithoutInk(assert(this.shadowRoot!.querySelector('#icon')!));
    });
  }

  private announceSearchResults_() {
    if (!this.filter.trim()) {
      return;
    }
    setTimeout(() => {  // Async to allow list to update.
      IronA11yAnnouncer.requestAvailability();
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
      this.dispatchEvent(new CustomEvent('iron-announce', {
        bubbles: true,
        composed: true,
        detail: {
          text: text,
        }
      }));
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
        return '';
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
