// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './viewer-password-dialog.html.js';

export interface ViewerPasswordDialogElement {
  $: {
    dialog: CrDialogElement,
    password: CrInputElement,
    submit: CrButtonElement,
  };
}

export class ViewerPasswordDialogElement extends PolymerElement {
  static get is() {
    return 'viewer-password-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      invalid: Boolean,
    };
  }

  invalid: boolean;

  close() {
    this.$.dialog.close();
  }

  deny() {
    const password = this.$.password;
    password.disabled = false;
    this.$.submit.disabled = false;
    this.invalid = true;
    password.select();

    this.dispatchEvent(new CustomEvent('password-denied-for-testing'));
  }

  submit() {
    const password = this.$.password;
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

declare global {
  interface HTMLElementTagNameMap {
    'viewer-password-dialog': ViewerPasswordDialogElement;
  }
}

customElements.define(
    ViewerPasswordDialogElement.is, ViewerPasswordDialogElement);
