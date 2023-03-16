// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './strings.m.js';
import './password_list_item.js';
import './dialogs/add_password_dialog.js';
import './dialogs/auth_timed_out_dialog.js';
import './user_utils_mixin.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './passwords_section.html.js';
import {Route, RouteObserverMixin, UrlParam} from './router.js';
import {UserUtilMixin} from './user_utils_mixin.js';


export interface PasswordsSectionElement {
  $: {
    addPasswordButton: CrButtonElement,
    passwordsList: IronListElement,
    movePasswords: HTMLElement,
    importPasswords: HTMLElement,
  };
}

const PasswordsSectionElementBase =
    UserUtilMixin(RouteObserverMixin(I18nMixin(PolymerElement)));

export class PasswordsSectionElement extends PasswordsSectionElementBase {
  static get is() {
    return 'passwords-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Password groups displayed in the UI.
       */
      groups_: {
        type: Array,
        value: () => [],
        observer: 'onGroupsChanged_',
      },

      /** Filter on the saved passwords and exceptions. */
      searchTerm_: {
        type: String,
        value: '',
      },

      shownGroupsCount_: {
        type: Number,
        value: 0,
        observer: 'announceSearchResults_',
      },

      showAddPasswordDialog_: Boolean,
      showAuthTimedOutDialog_: Boolean,

      movePasswordsText_: String,

      numberOfPasswordsOnDevice_: {
        type: Number,
        computed: 'computeNumberOfPasswordsOnDevice_(groups_)',
      },

      showMovePasswords_: {
        type: Boolean,
        computed: 'computeShowMovePasswords_(isOptedInForAccountStorage, ' +
            'numberOfPasswordsOnDevice_)',
      },
    };
  }

  private groups_: chrome.passwordsPrivate.CredentialGroup[] = [];
  private searchTerm_: string;
  private shownGroupsCount_: number;
  private showAddPasswordDialog_: boolean;
  private showAuthTimedOutDialog_: boolean;
  private movePasswordsText_: string;

  private setSavedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;
  private authTimedOutListener_: (() => void)|null;

  override connectedCallback() {
    super.connectedCallback();
    const updateGroups = () => {
      PasswordManagerImpl.getInstance().getCredentialGroups().then(
          groups => this.groups_ = groups);
    };

    this.setSavedPasswordsListener_ = _passwordList => {
      updateGroups();
    };

    updateGroups();
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);

    this.authTimedOutListener_ = this.onAuthTimedOut_.bind(this);
    window.addEventListener('auth-timed-out', this.authTimedOutListener_);
    this.$.movePasswords.addEventListener(
        'click', this.onMovePasswordsClicked_);
    this.$.importPasswords.addEventListener(
        'click', this.onImportPasswordsClicked_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.setSavedPasswordsListener_ = null;
    assert(this.authTimedOutListener_);
    window.removeEventListener('hashchange', this.authTimedOutListener_);
    this.authTimedOutListener_ = null;
  }

  override currentRouteChanged(newRoute: Route, _oldRoute: Route): void {
    const searchTerm = newRoute.queryParameters.get(UrlParam.SEARCH_TERM) || '';
    if (searchTerm !== this.searchTerm_) {
      this.searchTerm_ = searchTerm;
    }
  }

  private hideGroupsList_(): boolean {
    return this.groups_.filter(this.groupFilter_()).length === 0;
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

  private onAddPasswordClick_() {
    this.showAddPasswordDialog_ = true;
  }

  private onAddPasswordDialogClosed_() {
    this.showAddPasswordDialog_ = false;
  }

  private onAuthTimedOut_() {
    this.showAuthTimedOutDialog_ = true;
  }

  private onAuthTimedOutDialogClosed_() {
    this.showAuthTimedOutDialog_ = false;
  }

  private computeNumberOfPasswordsOnDevice_(): number {
    const localStorage = [
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT,
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
    ];
    return this.groups_.map(group => group.entries)
        .flat()
        .filter(entry => localStorage.includes(entry.storedIn))
        .length;
  }

  private computeShowMovePasswords_(): boolean {
    // TODO(crbug.com/1420548): Check for conflicts if needed.
    return this.computeNumberOfPasswordsOnDevice_() > 0 &&
        this.isOptedInForAccountStorage;
  }

  private async onGroupsChanged_() {
    this.movePasswordsText_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'movePasswords', this.computeNumberOfPasswordsOnDevice_());
  }

  private getMovePasswordsText_(): TrustedHTML {
    return sanitizeInnerHtml(this.movePasswordsText_);
  }


  private onMovePasswordsClicked_(e: Event) {
    e.preventDefault();
    // TODO(crbug.com/1420548): Show move passwords dialog.
  }

  private showImportPasswordsOption_(): boolean {
    if (!this.groups_) {
      return false;
    }
    return this.groups_.length === 0;
  }

  private getImportPasswordsText_(): TrustedHTML {
    return this.i18nAdvanced('emptyState');
  }

  private onImportPasswordsClicked_(e: Event) {
    e.preventDefault();
    // TODO(crbug.com/1420548): Show import passwords dialog.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-section': PasswordsSectionElement;
  }
}

customElements.define(PasswordsSectionElement.is, PasswordsSectionElement);
