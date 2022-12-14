// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './strings.m.js';
import './password_list_item.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './passwords_section.html.js';
import {Route, RouteObserverMixin, UrlParam} from './router.js';

export interface PasswordsSectionElement {
  $: {
    passwordsList: IronListElement,
  };
}

const PasswordsSectionElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

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
      },

      /** Filter on the saved passwords and exceptions. */
      searchTerm_: {
        type: String,
        value: '',
      },
    };
  }

  private groups_: chrome.passwordsPrivate.CredentialGroup[] = [];
  private searchTerm_: string;

  private setSavedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;

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
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.setSavedPasswordsListener_ = null;
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
    // TODO(crbug.com/1400289): Announce search result.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-section': PasswordsSectionElement;
  }
}

customElements.define(PasswordsSectionElement.is, PasswordsSectionElement);
