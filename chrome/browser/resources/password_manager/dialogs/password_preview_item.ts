// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordPreviewItem represents one row in a list of passwords.
 */

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../site_favicon.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ShowPasswordMixinLit} from '../show_password_mixin_lit.js';

import {getCss} from './password_preview_item.css.js';
import {getHtml} from './password_preview_item.html.js';

export interface PasswordPreviewItemElement {
  $: {
    checkbox: CrCheckboxElement,
    username: HTMLElement,
    password: HTMLInputElement,
    showPasswordButton: CrIconButtonElement,
  };
}

const PasswordPreviewItemElementBase =
    I18nMixinLit(ShowPasswordMixinLit(CrLitElement));

export class PasswordPreviewItemElement extends PasswordPreviewItemElementBase {
  static get is() {
    return 'password-preview-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      passwordId: {type: Number},
      url: {type: String},
      username: {type: String},
      password: {type: String},
      first: {type: Boolean},

      checked: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor passwordId: number = 0;
  accessor url: string = '';
  accessor username: string = '';
  accessor password: string = '';
  accessor first: boolean = false;
  accessor checked: boolean = true;

  protected onCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.checked = e.detail.value;
  }

  protected getElementClass_(): string {
    return this.first ? '' : 'hr';
  }

  protected getPasswordValue_(): string {
    return this.isPasswordVisible ? this.password : ' '.repeat(10);
  }

  protected getShowHidePasswordButtonA11yLabel_(): string {
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
