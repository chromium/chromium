// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordPreviewItem represents one row in a list of passwords.
 */

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../site_favicon.js';
import '../shared_style.css.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ShowPasswordMixin} from '../show_password_mixin.js';

import {getTemplate} from './password_preview_item.html.js';

export interface PasswordPreviewItemElement {
  $: {
    checkbox: CrCheckboxElement,
    website: HTMLElement,
    username: HTMLElement,
    password: HTMLInputElement,
    showPasswordButton: CrIconButtonElement,
  };
}

const PasswordPreviewItemElementBase =
    I18nMixin(ShowPasswordMixin(PolymerElement));

export class PasswordPreviewItemElement extends PasswordPreviewItemElementBase {
  static get is() {
    return 'password-preview-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      passwordId: Number,
      url: String,
      username: String,
      password: String,
      first: Boolean,

      checked: {
        type: Boolean,
        value: true,
      },
    };
  }

  passwordId: number;
  url: string;
  username: string;
  password: string;
  first: boolean;
  checked: boolean;

  private getElementClass_(): string {
    return this.first ? '' : 'hr';
  }

  private getPasswordValue_(): string {
    return this.isPasswordVisible ? this.password : ' '.repeat(10);
  }

  private getShowHidePasswordButtonA11yLabel_(): string {
    return this.i18n(
        this.isPasswordVisible ? 'hidePasswordA11yLabel' :
                                 'showPasswordA11yLabel',
        this.username, this.url);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-preview-item': PasswordPreviewItemElement;
  }
}

customElements.define(
    PasswordPreviewItemElement.is, PasswordPreviewItemElement);
