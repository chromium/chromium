// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './share_password_recipient.css.js';
import {getHtml} from './share_password_recipient.html.js';

export interface SharePasswordRecipientElement {
  $: {
    avatar: HTMLImageElement,
    name: HTMLElement,
    email: HTMLElement,
    checkbox: CrCheckboxElement,
  };
}

export class SharePasswordRecipientElement extends CrLitElement {
  static get is() {
    return 'share-password-recipient';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      recipient: {type: Object},

      selected: {
        type: Boolean,
        reflect: true,
        notify: true,
      },
    };
  }

  accessor disabled: boolean = false;
  accessor selected: boolean = false;
  accessor recipient: chrome.passwordsPrivate.RecipientInfo = {
    userId: '',
    email: '',
    displayName: '',
    profileImageUrl: '',
    isEligible: false,
  };

  override firstUpdated() {
    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener('mouseover', this.onMouseOver_.bind(this));
    this.addEventListener('mouseout', this.onMouseOut_.bind(this));
  }

  protected onCheckboxCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.selected = e.detail.value;
  }

  private onClick_(e: Event) {
    if (this.disabled) {
      return;
    }

    e.preventDefault();
    this.$.checkbox.click();
  }

  private onMouseOver_(e: Event) {
    if (this.disabled) {
      return;
    }

    e.preventDefault();
    this.$.checkbox.getRipple().showAndHoldDown();
  }

  private onMouseOut_(e: Event) {
    if (this.disabled) {
      return;
    }

    e.preventDefault();
    this.$.checkbox.getRipple().clear();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-recipient': SharePasswordRecipientElement;
  }
}

customElements.define(
    SharePasswordRecipientElement.is, SharePasswordRecipientElement);
