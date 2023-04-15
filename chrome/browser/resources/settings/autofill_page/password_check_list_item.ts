// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordCheckListItem represents one insecure credential in the
 * list of insecure passwords.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/js/action_link.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
// <if expr="is_chromeos">
import '../controls/password_prompt_dialog.js';
// </if>
import './passwords_shared.css.js';

import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './password_check_list_item.html.js';
import {PasswordCheckInteraction, PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {PasswordRequestorMixin} from './password_requestor_mixin.js';

export interface PasswordCheckListItemElement {
  $: {
    insecureOrigin: HTMLElement,
    insecureUsername: HTMLElement,
    insecurePassword: HTMLInputElement,
    more: CrIconButtonElement,
  };
}

const PasswordCheckListItemElementBase = PasswordRequestorMixin(PolymerElement);

export class PasswordCheckListItemElement extends
    PasswordCheckListItemElementBase {
  static get is() {
    return 'password-check-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The password that is being displayed.
       */
      item: Object,

      showDetails: Boolean,

      isPasswordVisible: {
        type: Boolean,
        computed: 'computePasswordVisibility_(item.password)',
      },

      password_: {
        type: String,
        computed: 'computePassword_(item.password)',
      },

      clickedChangePassword: {
        type: Boolean,
        value: false,
      },

      buttonClass_: {
        type: String,
        computed: 'computeButtonClass_(showDetails)',
      },

      iconClass_: {
        type: String,
        computed: 'computeIconClass_(showDetails)',
      },
    };
  }

  item: chrome.passwordsPrivate.PasswordUiEntry;
  showDetails: boolean = false;
  isPasswordVisible: boolean;
  private password_: string;
  clickedChangePassword: boolean;
  private buttonClass_: string;
  private iconClass_: string;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  private getCompromiseType_(): string {
    const isLeaked = this.item.compromisedInfo!.compromiseTypes.some(
        type => type === chrome.passwordsPrivate.CompromiseType.LEAKED);
    const isPhished = this.item.compromisedInfo!.compromiseTypes.some(
        type => type === chrome.passwordsPrivate.CompromiseType.PHISHED);
    if (isLeaked && isPhished) {
      return loadTimeData.getString('phishedAndLeakedPassword');
    }
    if (isPhished) {
      return loadTimeData.getString('phishedPassword');
    }
    if (isLeaked) {
      return loadTimeData.getString('leakedPassword');
    }

    assertNotReached(
        'Can\'t find a string for type: ' + this.item.compromisedInfo!);
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onChangePasswordClick_() {
    this.fire_('change-password-clicked', {id: this.item.id});

    assert(this.item.changePasswordUrl);
    OpenWindowProxyImpl.getInstance().openUrl(this.item.changePasswordUrl);
    PasswordManagerImpl.getInstance().recordChangePasswordFlowStarted(
        this.item);
    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.CHANGE_PASSWORD);
  }

  private onMoreClick_(event: Event) {
    this.fire_('more-actions-click', {moreActionsButton: event.target});
  }

  private getInputType_(): string {
    return this.isPasswordVisible ? 'text' : 'password';
  }

  private computePasswordVisibility_(): boolean {
    return !!this.item.password;
  }

  private computeButtonClass_(): string {
    if (this.showDetails) {
      // Strong CTA.
      return 'action-button';
    }
    // Weak CTA.
    return '';
  }

  private computeIconClass_(): string {
    if (this.showDetails) {
      // Strong CTA, white icon.
      return '';
    }
    // Weak CTA, non-white-icon.
    return 'icon-weak-cta';
  }

  private computePassword_(): string {
    const NUM_PLACEHOLDERS = 10;
    return this.item.password || ' '.repeat(NUM_PLACEHOLDERS);
  }

  hidePassword() {
    this.set('item.password', null);
  }

  showPassword() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordCheckInteraction.SHOW_PASSWORD);
    this.requestPlaintextPassword(
            this.item.id, chrome.passwordsPrivate.PlaintextReason.VIEW)
        .then(password => this.set('item.password', password), _error => {});
  }

  private onReadonlyInputClick_() {
    if (this.isPasswordVisible) {
      (this.shadowRoot!.querySelector('#leakedPassword') as HTMLInputElement)
          .select();
    }
  }

  private onAlreadyChangedClick_(event: Event) {
    event.preventDefault();
    this.fire_('already-changed-password-click', event.target);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-check-list-item': PasswordCheckListItemElement;
  }
}

customElements.define(
    PasswordCheckListItemElement.is, PasswordCheckListItemElement);
