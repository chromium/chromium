// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './share_password_dialog_header.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './share_password_no_members_dialog.html.js';

export interface SharePasswordNoMembersDialogElement {
  $: {
    description: HTMLElement,
    action: HTMLElement,
    header: HTMLElement,
  };
}

export class SharePasswordNoMembersDialogElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'share-password-no-members-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {dialogTitle: {type: String}};
  }

  dialogTitle: string;

  private onClickActionButton_() {
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-no-members-dialog': SharePasswordNoMembersDialogElement;
  }
}

customElements.define(
    SharePasswordNoMembersDialogElement.is,
    SharePasswordNoMembersDialogElement);
