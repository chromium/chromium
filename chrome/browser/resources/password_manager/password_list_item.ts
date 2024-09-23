// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './site_favicon.js';
import './searchable_label.js';
import './shared_style.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_list_item.html.js';
import {PasswordManagerImpl, PasswordViewPageInteractions} from './password_manager_proxy.js';
import {Page, Router, UrlParam} from './router.js';

export interface PasswordListItemElement {
  $: {
    displayedName: HTMLElement,
    numberOfAccounts: HTMLElement,
    seePasswordDetails: HTMLElement,
  };
}
const PasswordListItemElementBase = I18nMixin(PolymerElement);

export class PasswordListItemElement extends PasswordListItemElementBase {
  static get is() {
    return 'password-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      item: {
        type: Object,
        observer: 'onItemChanged_',
      },

      isAccountStoreUser: Boolean,

      first: Boolean,

      searchTerm: String,

      elementClass_: {
        type: String,
        computed: 'computeElementClass_(first)',
      },

      /**
       * The number of accounts in a group as a formatted string.
       */
      numberOfAccounts_: String,

      deviceOnlyCredentialsAccessibilityLabelText_: String,
    };
  }

  item: chrome.passwordsPrivate.CredentialGroup;
  isAccountStoreUser: boolean;
  first: boolean;
  searchTerm: string;
  private numberOfAccounts_: string;
  private tooltipText_: string;
  private deviceOnlyCredentialsAccessibilityLabelText_: string;

  private computeElementClass_(): string {
    return this.first ? 'flex-centered' : 'flex-centered hr';
  }

  override ready() {
    super.ready();
    this.addEventListener('click', this.onRowClick_);
  }

  override focus() {
    this.$.seePasswordDetails.focus();
  }

  private async onRowClick_() {
    const ids = this.item.entries.map(entry => entry.id);
    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails(ids)
        .then(entries => {
          const group: chrome.passwordsPrivate.CredentialGroup = {
            name: this.item.name,
            iconUrl: this.item.iconUrl,
            entries: entries,
          };
          this.dispatchEvent(new CustomEvent(
              'password-details-shown',
              {bubbles: true, composed: true, detail: this}));
          // Keep current search query.
          Router.getInstance().navigateTo(
              Page.PASSWORD_DETAILS, group,
              Router.getInstance().currentRoute.queryParameters);
        })
        .catch(() => {});
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.CREDENTIAL_ROW_CLICKED);

    const searchTerm = Router.getInstance().currentRoute.queryParameters.get(
                           UrlParam.SEARCH_TERM) || '';
    chrome.metricsPrivate.recordBoolean(
        'PasswordManager.UI.OpenedPasswordDetailsWhileSearching', !!searchTerm);
  }

  private async onItemChanged_() {
    if (this.item.entries.length > 1) {
      this.numberOfAccounts_ =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'numberOfAccounts', this.item.entries.length);
    }
    this.tooltipText_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'deviceOnlyPasswordsIconTooltip',
            this.getNumberOfCredentialsOnDevice_());
    if (this.shouldShowDeviceOnlyCredentialsIcon_()) {
      this.deviceOnlyCredentialsAccessibilityLabelText_ =
          await PluralStringProxyImpl.getInstance()
              .getPluralString(
                  'deviceOnlyListItemAriaLabel', this.item.entries.length)
              .then(label => label.replace('$1', this.item.name));
    }
  }

  private showNumberOfAccounts_(): boolean {
    return !this.searchTerm && this.item.entries.length > 1;
  }

  private getTitle_() {
    const term = this.searchTerm.trim().toLowerCase();
    if (!term) {
      return this.item.name;
    }

    if (this.item.name.includes(term)) {
      return this.item.name;
    }

    const entries = this.item.entries;
    // Show matching username in the title if it exists.
    const matchingUsername =
        entries.find(c => c.username.toLowerCase().includes(term))?.username;
    if (matchingUsername) {
      return this.item.name + ' • ' + matchingUsername;
    }

    // Show matching domain in the title if it exists.
    const domains =
        Array.prototype.concat(...entries.map(c => c.affiliatedDomains || []));
    const matchingDomain =
        domains.find(d => d.name.toLowerCase().includes(term))?.name;
    if (matchingDomain) {
      return this.item.name + ' • ' + matchingDomain;
    }
    return this.item.name;
  }

  private getNumberOfCredentialsOnDevice_(): number {
    return this.item.entries
        .filter(
            entry => entry.storedIn ===
                chrome.passwordsPrivate.PasswordStoreSet.DEVICE)
        .length;
  }

  private shouldShowDeviceOnlyCredentialsIcon_(): boolean {
    return this.isAccountStoreUser &&
        (this.getNumberOfCredentialsOnDevice_() > 0);
  }

  private getAriaLabel_(): string {
    if (this.shouldShowDeviceOnlyCredentialsIcon_()) {
      return this.deviceOnlyCredentialsAccessibilityLabelText_;
    }
    return this.i18n('viewPasswordAriaDescription', this.item.name);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-list-item': PasswordListItemElement;
  }
}

customElements.define(PasswordListItemElement.is, PasswordListItemElement);
