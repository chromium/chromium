// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './share_password_dialog_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './share_password_loading_dialog.css.js';
import {getHtml} from './share_password_loading_dialog.html.js';

export class SharePasswordLoadingDialogElement extends CrLitElement {
  static get is() {
    return 'share-password-loading-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dialogTitle: {type: String},
    };
  }

  accessor dialogTitle: string = '';
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-loading-dialog': SharePasswordLoadingDialogElement;
  }
}

customElements.define(
    SharePasswordLoadingDialogElement.is, SharePasswordLoadingDialogElement);
