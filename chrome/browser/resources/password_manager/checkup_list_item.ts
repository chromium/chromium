// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './site_favicon.js';
import './shared_style.css.js';

import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './checkup_list_item.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {ShowPasswordMixin, ShowPasswordMixinInterface} from './show_password_mixin.js';

export interface CheckupListItemElement extends ShowPasswordMixinInterface {
  $: {
    shownUrl: HTMLElement,
    username: HTMLElement,
    insecurePassword: HTMLInputElement,
    more: CrIconButtonElement,
  };
}

const CheckupListItemElementBase = ShowPasswordMixin(I18nMixin(PolymerElement));

export class CheckupListItemElement extends CheckupListItemElementBase {
  static get is() {
    return 'checkup-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      item: Object,

      group: Object,

      first: Boolean,

      showDetails: Boolean,
    };
  }

  item: chrome.passwordsPrivate.PasswordUiEntry;
  group: chrome.passwordsPrivate.CredentialGroup;
  first: boolean;
  showDetails: boolean;

  private getPasswordValue_(): string|undefined {
    return this.isPasswordVisible ? this.item.password : ' '.repeat(10);
  }

  private getCompromiseDescription_(): string {
    assert(this.item.compromisedInfo);
    const isLeaked = this.item.compromisedInfo.compromiseTypes.some(
        type => type === chrome.passwordsPrivate.CompromiseType.LEAKED);
    const isPhished = this.item.compromisedInfo.compromiseTypes.some(
        type => type === chrome.passwordsPrivate.CompromiseType.PHISHED);
    if (isLeaked && isPhished) {
      return this.i18n('phishedAndLeakedPassword');
    }
    if (isPhished) {
      return this.i18n('phishedPassword');
    }
    if (isLeaked) {
      return this.i18n('leakedPassword');
    }

    assertNotReached(
        'Can\'t find a string for type: ' + this.item.compromisedInfo);
  }

  private onMoreClick_(event: Event) {
    this.dispatchEvent(new CustomEvent('more-actions-click', {
      bubbles: true,
      composed: true,
      detail: {
        listItem: this,
        target: event.target,
      },
    }));
  }

  public showHidePassword() {
    if (this.isPasswordVisible === true) {
      this.onShowHidePasswordButtonClick();
      this.item.password = undefined;
      this.item.note = undefined;
      return;
    }

    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails([this.item.id])
        .then(entries => {
          const entry = entries[0];
          assert(!!entry);
          this.item.password = entry.password;
          this.item.note = entry.note;
          this.onShowHidePasswordButtonClick();
        })
        .catch(() => {});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'checkup-list-item': CheckupListItemElement;
  }
}

customElements.define(CheckupListItemElement.is, CheckupListItemElement);
