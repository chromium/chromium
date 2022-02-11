// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordListItem represents one row in a list of passwords,
 * with a "more actions" button. It needs to be its own component because
 * FocusRowBehavior provides good a11y.
 * Clicking the button fires a password-more-actions-clicked event.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import '../settings_shared_css.js';
import '../site_favicon.js';
import './passwords_shared_css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './password_list_item.html.js';
import {ShowPasswordMixin, ShowPasswordMixinInterface} from './show_password_mixin.js';

export type PasswordMoreActionsClickedEvent = CustomEvent<{
  target: HTMLElement,
  listItem: PasswordListItemElement,
}>;

export interface PasswordListItemElement {
  $: {
    moreActionsButton: HTMLElement,
    originUrl: HTMLAnchorElement,
    username: HTMLInputElement,
  };
}

const PasswordListItemElementBase = ShowPasswordMixin(PolymerElement) as
    {new (): PolymerElement & ShowPasswordMixinInterface};

export class PasswordListItemElement extends PasswordListItemElementBase {
  static get is() {
    return 'password-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether to hide the 3 dot button that open the more actions menu.
       */
      shouldHideMoreActionsButton: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * Selects the password on tap if revealed.
   */
  private onReadonlyInputTap_() {
    if (this.entry.password) {
      (this.shadowRoot!.querySelector('#password') as HTMLInputElement)
          .select();
    }
  }

  private onPasswordMoreActionsButtonTap_() {
    this.dispatchEvent(new CustomEvent('password-more-actions-clicked', {
      bubbles: true,
      composed: true,
      detail: {
        target: this.$.moreActionsButton,
        listItem: this,
      },
    }));
  }

  /**
   * Get the aria label for the More Actions button on this row.
   */
  private getMoreActionsLabel_(): string {
    // Avoid using I18nMixin.i18n, because it will filter sequences, which
    // are otherwise not illegal for usernames. Polymer still protects against
    // XSS injection.
    return loadTimeData.getStringF(
        (this.entry.federationText) ? 'passwordRowFederatedMoreActionsButton' :
                                      'passwordRowMoreActionsButton',
        this.entry.username, this.entry.urls.shown);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-list-item': PasswordListItemElement;
  }
}

customElements.define(PasswordListItemElement.is, PasswordListItemElement);
