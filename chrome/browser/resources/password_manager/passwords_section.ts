// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '/strings.m.js';
import './password_list_item.js';
import './dialogs/add_password_dialog.js';
import './dialogs/auth_timed_out_dialog.js';
import './notification_cards/notification_card.js';
import './notification_cards/notification_cards_browser_proxy.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {htmlEscape} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {FocusConfig} from './focus_config.js';
import {NotificationCardId} from './notification_cards/notification_card.js';
import type {NotificationCard} from './notification_cards/notification_cards_browser_proxy.js';
import {NotificationCardsProxyImpl} from './notification_cards/notification_cards_browser_proxy.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getCss} from './passwords_section.css.js';
import {getHtml} from './passwords_section.html.js';
import {isSavingNewPasswordsBlockedByMandatoryPolicy} from './prefs/policy_utils.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixinLit, Router, UrlParam} from './router.js';
import {UserUtilMixinLit} from './user_utils_mixin_lit.js';

export interface PasswordsSectionElement {
  $: {
    addPasswordButton: CrButtonElement,
    descriptionLabel: HTMLElement,
    passwords: HTMLElement,
    noPasswordsFound: HTMLElement,
    importPasswords: HTMLElement,
    trustedVaultUnlock: HTMLElement,
  };
}

const PasswordsSectionElementBase = PrefServiceObserverMixinLit(
    UserUtilMixinLit(RouteObserverMixinLit(I18nMixinLit(CrLitElement))));

export class PasswordsSectionElement extends PasswordsSectionElementBase {
  static get is() {
    return 'passwords-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      focusConfig: {type: Object},

      /**
       * Password groups displayed in the UI.
       */
      groups_: {type: Array},
      filteredGroups_: {type: Array},

      /** Filter on the saved passwords and exceptions. */
      searchTerm_: {type: String},

      showAddPasswordDialog_: {type: Boolean},
      showAuthTimedOutDialog_: {type: Boolean},
      notificationCard_: {type: Object},
      passwordSavingBlockedByPolicy_: {type: Boolean},
    };
  }

  accessor focusConfig: FocusConfig;
  private accessor groups_: chrome.passwordsPrivate.CredentialGroup[]|null =
      null;
  protected accessor filteredGroups_:
      chrome.passwordsPrivate.CredentialGroup[] = [];
  protected accessor searchTerm_: string = '';
  protected accessor showAddPasswordDialog_: boolean = false;
  protected accessor showAuthTimedOutDialog_: boolean = false;
  protected accessor notificationCard_: NotificationCard|null = null;
  protected accessor passwordSavingBlockedByPolicy_: boolean = false;

  private shownGroupsCount_: number = 0;
  /**
   * The element to return focus to, when moving from details page to
   * passwords page.
   */
  private activeListItem_: HTMLElement|null = null;
  private setSavedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;
  private authTimedOutListener_: (() => void)|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.addPrefObserver<boolean>('credentials_enable_service', pref => {
      this.passwordSavingBlockedByPolicy_ =
          isSavingNewPasswordsBlockedByMandatoryPolicy(pref);
    });
    const updateGroups = () => {
      PasswordManagerImpl.getInstance().getCredentialGroups().then(groups => {
        this.groups_ = groups;
      });
    };

    this.setSavedPasswordsListener_ = _passwordList => {
      if (_passwordList.length === 0 &&
          this.notificationCard_?.id === NotificationCardId.CHECKUP) {
        this.notificationCard_ = null;
      }
      updateGroups();
    };

    updateGroups();
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    NotificationCardsProxyImpl.getInstance()
        .getAvailableNotificationCard()
        .then(card => this.notificationCard_ = card);

    this.authTimedOutListener_ = this.onAuthTimedOut_.bind(this);
    window.addEventListener('auth-timed-out', this.authTimedOutListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.setSavedPasswordsListener_ = null;
    assert(this.authTimedOutListener_);
    window.removeEventListener('auth-timed-out', this.authTimedOutListener_);
    this.authTimedOutListener_ = null;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('focusConfig')) {
      this.focusConfigChanged_(
          this.focusConfig, changedProperties.get('focusConfig'));
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('groups_') ||
        changedPrivateProperties.has('searchTerm_')) {
      this.filteredGroups_ = this.computeFilteredGroups_();
      const count = this.filteredGroups_.length;
      if (count !== this.shownGroupsCount_) {
        this.shownGroupsCount_ = count;
        this.announceSearchResults_();
      }
    }
  }

  override currentRouteChanged(newRoute: Route): void {
    const searchTerm = newRoute.queryParameters.get(UrlParam.SEARCH_TERM) || '';
    if (searchTerm !== this.searchTerm_) {
      this.searchTerm_ = searchTerm;
    }
  }

  focusFirstResult() {
    if (!this.searchTerm_) {
      // If search term is empty don't do anything.
      return;
    }
    const result = this.shadowRoot.querySelector('password-list-item');
    if (result) {
      result.focus();
    }
  }

  private computeFilteredGroups_(): chrome.passwordsPrivate.CredentialGroup[] {
    if (!this.groups_) {
      return [];
    }
    const filtered = this.groups_.filter(this.groupFilter_());
    const sortFn = this.computeSortFunction_(this.searchTerm_);
    return sortFn ? filtered.sort(sortFn) : filtered;
  }

  protected hideGroupsList_(): boolean {
    return this.filteredGroups_.length === 0;
  }

  private groupFilter_():
      ((entry: chrome.passwordsPrivate.CredentialGroup) => boolean) {
    const term = this.searchTerm_.trim().toLowerCase();
    // Group is matching if:
    // * group name includes term,
    // * any credential's username within a group includes a term,
    // * any credential within a group includes a term in a domain.
    return group => group.name.toLowerCase().includes(term) ||
        group.entries.some(
            credential => credential.username.toLowerCase().includes(term) ||
                credential.affiliatedDomains?.some(
                    domain => domain.name.toLowerCase().includes(term)));
  }

  private async announceSearchResults_() {
    if (!this.searchTerm_.trim()) {
      return;
    }
    const searchResult =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'searchResults', this.shownGroupsCount_);
    getAnnouncerInstance().announce(searchResult);
  }

  protected onAddPasswordClick_() {
    this.executeIfTrustedVaultUnlocked(() => {
      this.showAddPasswordDialog_ = true;
    });
  }

  protected onAddPasswordDialogClose_() {
    this.showAddPasswordDialog_ = false;
  }

  private onAuthTimedOut_() {
    this.showAuthTimedOutDialog_ = true;
  }

  protected onAuthTimedOutDialogClose_() {
    this.showAuthTimedOutDialog_ = false;
  }

  // TODO(crbug.com/410001569): This should check for localPasswordCount
  // instead, coming from the SyncHandler that queries the batch uploader. This
  // is needed to align the showing of the trigger and the content (which now
  // uses the BatchUpload data).
  private computePasswordsOnDevice_():
      chrome.passwordsPrivate.PasswordUiEntry[] {
    if (this.groups_ === null) {
      return [];
    }
    const localStorage = [
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT,
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
    ];
    return this.groups_.map(group => group.entries)
        .flat()
        .filter(entry => localStorage.includes(entry.storedIn));
  }

  protected showImportPasswordsOption_(): boolean {
    if (this.groups_ === null || this.actionableError === null ||
        this.passwordSavingBlockedByPolicy_) {
      return false;
    }
    return this.groups_.length === 0 && !this.isTrustedVaultKeyNeeded();
  }

  protected showTrustedVaultUnlockOption_(): boolean {
    if (this.groups_ === null || this.actionableError === null) {
      return false;
    }
    return this.groups_.length === 0 && this.isTrustedVaultKeyNeeded();
  }

  protected computeImportPasswordsText_(): TrustedHTML {
    if (this.isAccountStoreUser) {
      return this.i18nAdvanced('emptyStateImportAccountStore');
    }
    if (this.isSyncingPasswords) {
      return this.i18nAdvanced('emptyStateImportSyncing', {
        substitutions: [
          this.i18n('localPasswordManager'),
          htmlEscape(this.accountEmail),
        ],
      });
    }
    return this.i18nAdvanced('emptyStateImportDevice');
  }

  protected onTrustedVaultUnlockClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      e.preventDefault();
      PasswordManagerImpl.getInstance().startTrustedVaultUnlock();
    }
  }

  protected onImportPasswordsClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      e.preventDefault();
      const params = new URLSearchParams();
      params.set(UrlParam.START_IMPORT, 'true');
      Router.getInstance().navigateTo(Page.SETTINGS, null, params);
    }
  }

  protected onCardClosed_() {
    this.notificationCard_ = null;
  }

  protected computeShowPasswordsDescription_(): boolean {
    return this.groups_ !== null && !this.searchTerm_ &&
        this.groups_.length > 0;
  }

  protected showNoPasswordsFound_(): boolean {
    return this.groups_ !== null && this.hideGroupsList_() &&
        this.groups_.length > 0;
  }

  protected onPasswordDetailsShown_(e: CustomEvent<HTMLElement>) {
    this.activeListItem_ = e.detail;
  }

  private focusConfigChanged_(
      _newConfig: FocusConfig, oldConfig?: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    this.focusConfig.set(Page.PASSWORD_DETAILS, () => {
      if (!this.activeListItem_) {
        return;
      }

      focusWithoutInk(this.activeListItem_);
    });
  }

  private computeSortFunction_(searchTerm: string):
      ((a: chrome.passwordsPrivate.CredentialGroup,
        b: chrome.passwordsPrivate.CredentialGroup) => number)|null {
    // Keep current order when not searching.
    if (!searchTerm) {
      return null;
    }

    // Always show group with matching name in the top, fallback to alphabetical
    // order when matching type is the same.
    return function(
        a: chrome.passwordsPrivate.CredentialGroup,
        b: chrome.passwordsPrivate.CredentialGroup) {
      const doesNameMatchA = a.name.toLowerCase().includes(searchTerm);
      const doesNameMatchB = b.name.toLowerCase().includes(searchTerm);
      if (doesNameMatchA === doesNameMatchB) {
        return a.name.localeCompare(b.name);
      }
      return doesNameMatchA ? -1 : 1;
    };
  }

  protected computeShouldShowNotificationCard_(): boolean {
    if (!this.notificationCard_) {
      return false;
    }
    if (this.notificationCard_.id ===
        NotificationCardId.ACCOUNT_AWARE_PASSWORD_SIGNIN) {
      return !this.isAccountStoreUser && !this.accountEmail;
    }
    if (this.notificationCard_.id !== NotificationCardId.MOVE_PASSWORDS) {
      return true;
    }

    // Check if there are local passwords and they can be moved to account.
    if (this.computePasswordsOnDevice_().length === 0 ||
        !this.isAccountStoreUser) {
      return false;
    }
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-section': PasswordsSectionElement;
  }
}

customElements.define(PasswordsSectionElement.is, PasswordsSectionElement);
