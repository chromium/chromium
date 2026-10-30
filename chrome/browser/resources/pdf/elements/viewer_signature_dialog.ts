// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './viewer_signature_dialog.css.js';
import {getHtml} from './viewer_signature_dialog.html.js';

export interface ViewerSignatureDialogElement {
  $: {
    clear: CrButtonElement,
    create: CrButtonElement,
    dialog: CrDialogElement,
  };
}

export class ViewerSignatureDialogElement extends CrLitElement {
  static get is() {
    return 'viewer-signature-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-signature-dialog': ViewerSignatureDialogElement;
  }
}

customElements.define(
    ViewerSignatureDialogElement.is, ViewerSignatureDialogElement);
