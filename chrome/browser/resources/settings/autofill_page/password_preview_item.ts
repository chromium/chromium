// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordPreviewItem represents one row in a list of passwords.
 * It needs to be its own component because FocusRowBehavior provides good a11y.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
import './passwords_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_preview_item.html.js';

export class PasswordPreviewItemElement extends PolymerElement {
  static get is() {
    return 'password-preview-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      url: String,
      username: String,
      password: String,
      first: Boolean,

      passwordHidden_: {
        type: Boolean,
        value: true,
      },
    };
  }

  url: string;
  username: string;
  password: string;
  first: boolean;
  private passwordHidden_: boolean;

  private computeElementClass_(): string {
    return this.first ? '' : 'hr';
  }

  private getIconClass_(): string {
    return !this.passwordHidden_ ? 'icon-visibility-off' : 'icon-visibility';
  }

  private getPassword_(): string {
    return !this.passwordHidden_ ? this.password : '•••••••••••';
  }

  private onShowPasswordButtonClick_(): void {
    this.passwordHidden_ = !this.passwordHidden_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-preview-item': PasswordPreviewItemElement;
  }
}

customElements.define(
    PasswordPreviewItemElement.is, PasswordPreviewItemElement);
