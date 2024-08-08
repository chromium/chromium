// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './viewer_password_dialog.css.js';
import {getHtml} from './viewer_password_dialog.html.js';

export interface ViewerPasswordDialogElement {
  $: {
    dialog: CrDialogElement,
    password: CrInputElement,
    submit: CrButtonElement,
  };
}

export class ViewerPasswordDialogElement extends CrLitElement {
  static get is() {
    return 'viewer-password-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      invalid: {type: Boolean},
    };
  }

  invalid: boolean = false;

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
