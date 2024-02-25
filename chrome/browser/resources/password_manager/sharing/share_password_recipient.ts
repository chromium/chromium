// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '../shared_style.css.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './share_password_recipient.html.js';

export interface SharePasswordRecipientElement {
  $: {
    avatar: HTMLImageElement,
    name: HTMLElement,
    email: HTMLElement,
    checkbox: CrCheckboxElement,
  };
}

export class SharePasswordRecipientElement extends PolymerElement {
  static get is() {
    return 'share-password-recipient';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      recipient: Object,

      selected: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        notify: true,
      },
    };
  }
  disabled: boolean;
  selected: boolean;
  recipient: chrome.passwordsPrivate.RecipientInfo;

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener('mouseover', this.onMouseOver_.bind(this));
    this.addEventListener('mouseout', this.onMouseOut_.bind(this));
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
