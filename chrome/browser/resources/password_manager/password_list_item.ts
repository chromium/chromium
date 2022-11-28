// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './site_favicon.js';
import './shared_style.css.js';

import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_list_item.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {Page, Router} from './router.js';

export interface PasswordListItemElement {
  $: {
    displayName: HTMLAnchorElement,
  };
}

export class PasswordListItemElement extends PolymerElement {
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

      first: Boolean,

      elementClass_: {
        type: String,
        computed: 'computeElementClass_(first)',
      },

      /**
       * The number of accounts in a group as a formatted string.
       */
      numberOfAccounts_: String,
    };
  }

  item: chrome.passwordsPrivate.CredentialGroup;
  first: boolean;
  private numberOfAccounts_: string;

  private computeElementClass_(): string {
    return this.first ? 'flex-centered' : 'flex-centered hr';
  }

  override ready() {
    super.ready();
    this.addEventListener('click', this.onRowClick_);
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
          Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);
        })
        .catch(() => {});
  }

  private async onItemChanged_() {
    this.numberOfAccounts_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'numberOfAccounts', this.item.entries.length);
  }

  private showNumberOfAccounts_(): boolean {
    return this.item.entries.length > 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-list-item': PasswordListItemElement;
  }
}

customElements.define(PasswordListItemElement.is, PasswordListItemElement);
