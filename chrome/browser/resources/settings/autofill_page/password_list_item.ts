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
import './passwords_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './password_list_item.html.js';
import {PasswordViewPageInteractions, PasswordViewPageUrlParams, recordPasswordViewInteraction} from './password_view.js';
import {ShowPasswordMixin, ShowPasswordMixinInterface} from './show_password_mixin.js';

export type PasswordMoreActionsClickedEvent = CustomEvent<{
  target: HTMLElement,
  listItem: PasswordListItemElement,
}>;

export interface PasswordListItemElement {
  $: {
    moreActionsButton: HTMLElement,
    originUrl: HTMLAnchorElement,
    seePasswordDetails: HTMLElement,
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
      isPasswordViewPageEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordViewPage');
        },
      },

      /**
       * Whether subpage button is visible or not. Subpage button should be
       * visible only if password notes is enabled and |shouldHideActionButton|
       * is false.
       */
      shouldShowSubpageButton_: {
        type: Boolean,
        computed: 'computeShouldShowSubpageButton_(' +
            'isPasswordViewPageEnabled_, shouldHideActionButtons)',
        reflectToAttribute: true,
      },

      /**
       * Whether to hide buttons that open the subpage or the more actions menu.
       */
      shouldHideActionButtons: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isPasswordViewPageEnabled_: boolean;
  private shouldShowSubpageButton_: boolean;
  shouldHideActionButtons: boolean;

  private computeShouldShowSubpageButton_(): boolean {
    return !this.shouldHideActionButtons && this.isPasswordViewPageEnabled_;
  }

  private shouldHideMoreActionsButton_(): boolean {
    return this.isPasswordViewPageEnabled_ || this.shouldHideActionButtons;
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

  private onRowClick_() {
    if (!this.shouldShowSubpageButton_) {
      return;
    }
    const params = new URLSearchParams();
    params.set(PasswordViewPageUrlParams.SITE, this.entry.urls.shown);
    params.set(PasswordViewPageUrlParams.USERNAME, this.entry.username);
    // For sync'ing and signed-out users, there is strictly only one password
    // store, and hence no need to specify store information.
    // For account store users, a credential can exist in one or both of the
    // device and account stores, in which case, store information is required.
    // For consistency with the sync'ing and signed-out case, store information
    // isn't provided when the credentials exist only in the device store.
    if (this.entry.isPresentInAccount()) {
      params.set(PasswordViewPageUrlParams.IN_ACCOUNT, 'true');
      if (this.entry.isPresentOnDevice()) {
        params.set(PasswordViewPageUrlParams.ON_DEVICE, 'true');
      }
    }
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.CREDENTIAL_ROW_CLICKED);
    Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);
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

  /**
   * Get the aria label for the password details subpage.
   */
  private getSubpageLabel_(): string {
    return loadTimeData.getStringF(
        'passwordRowPasswordDetailPageButton', this.entry.username,
        this.entry.urls.shown);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-list-item': PasswordListItemElement;
  }
}

customElements.define(PasswordListItemElement.is, PasswordListItemElement);
