// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordListItem represents one row in a list of passwords,
 * with a "more actions" button. It needs to be its own component because
 * FocusRowBehavior provides good a11y.
 * Clicking the button fires a password-more-actions-clicked event.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
// <if expr="is_chromeos">
import '/shared/settings/controls/password_prompt_dialog.js';
// </if>
import './passwords_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './password_list_item.html.js';
import {PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME, PasswordViewPageInteractions, recordPasswordViewInteraction} from './password_view.js';
import {ShowPasswordMixin, ShowPasswordMixinInterface} from './show_password_mixin.js';


declare global {
  interface HTMLElementEventMap {
    [PASSWORD_MORE_ACTIONS_CLICKED_EVENT_NAME]: PasswordMoreActionsClickedEvent;
  }
}

export type PasswordMoreActionsClickedEvent = CustomEvent<{
  target: HTMLElement,
  listItem: PasswordListItemElement,
}>;

export const PASSWORD_MORE_ACTIONS_CLICKED_EVENT_NAME =
    'password-more-actions-clicked';

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

  override focus() {
    this.shouldShowSubpageButton_ ? this.$.seePasswordDetails.focus() :
                                    super.focus();
  }

  override ready() {
    super.ready();
    this.addEventListener('click', this.onRowClick_);
  }

  private computeShouldShowSubpageButton_(): boolean {
    return !this.shouldHideActionButtons && this.isPasswordViewPageEnabled_;
  }

  private shouldHideMoreActionsButton_(): boolean {
    return this.isPasswordViewPageEnabled_ || this.shouldHideActionButtons;
  }

  /**
   * Selects the password on tap if revealed.
   */
  private onReadonlyInputClick_() {
    if (this.entry.password) {
      (this.shadowRoot!.querySelector('#password') as HTMLInputElement)
          .select();
    }
  }

  private onRowClick_() {
    if (!this.shouldShowSubpageButton_) {
      return;
    }
    recordPasswordViewInteraction(
        PasswordViewPageInteractions.CREDENTIAL_ROW_CLICKED);
    this.dispatchEvent(
        new CustomEvent(PASSWORD_VIEW_PAGE_REQUESTED_EVENT_NAME, {
          bubbles: true,
          composed: true,
          detail: this,
        }));
  }

  private onPasswordMoreActionsButtonClick_() {
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
