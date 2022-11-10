// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './site_favicon.js';
import './shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_list_item.html.js';
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
      item: Object,

      first: Boolean,

      elementClass_: {
        type: String,
        computed: 'computeElementClass_(first)',
      },
    };
  }

  item: chrome.passwordsPrivate.CredentialGroup;
  first: boolean;

  private computeElementClass_(): string {
    return this.first ? 'flex-centered' : 'flex-centered hr';
  }

  override ready() {
    super.ready();
    this.addEventListener('click', this.onRowClick_);
  }

  private onRowClick_() {
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, this.item);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-list-item': PasswordListItemElement;
  }
}

customElements.define(PasswordListItemElement.is, PasswordListItemElement);
