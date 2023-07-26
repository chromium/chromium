// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './share_password_recipient.html.js';

export interface SharePasswordRecipientElement {
  $: {
    avatar: HTMLImageElement,
    name: HTMLElement,
    email: HTMLElement,
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
      recipient: Object,
    };
  }

  recipient: chrome.passwordsPrivate.RecipientInfo;
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-recipient': SharePasswordRecipientElement;
  }
}

customElements.define(
    SharePasswordRecipientElement.is, SharePasswordRecipientElement);
