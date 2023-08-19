// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../shared_style.css.js';
import './share_password_dialog_header.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './share_password_loading_dialog.html.js';

export class SharePasswordLoadingDialogElement extends PolymerElement {
  static get is() {
    return 'share-password-loading-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {dialogTitle: {type: String}};
  }

  dialogTitle: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-loading-dialog': SharePasswordLoadingDialogElement;
  }
}

customElements.define(
    SharePasswordLoadingDialogElement.is, SharePasswordLoadingDialogElement);
