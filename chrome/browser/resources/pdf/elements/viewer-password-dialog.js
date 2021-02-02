// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ViewerPasswordDialogElement extends PolymerElement {
  static get is() {
    return 'viewer-password-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      invalid: Boolean,
    };
  }

  close() {
    this.$.dialog.close();
  }

  deny() {
    const password = /** @type {!CrInputElement} */ (this.$.password);
    password.disabled = false;
    this.$.submit.disabled = false;
    this.invalid = true;
    password.select();

    this.dispatchEvent(new CustomEvent('password-denied-for-testing'));
  }

  submit() {
    const password = /** @type {!CrInputElement} */ (this.$.password);
    if (password.value.length === 0) {
      return;
    }
    password.disabled = true;
    this.$.submit.disabled = true;
    this.dispatchEvent(new CustomEvent('password-submitted', {
      detail: {password: password.value},
    }));
  }
}

customElements.define(
    ViewerPasswordDialogElement.is, ViewerPasswordDialogElement);
